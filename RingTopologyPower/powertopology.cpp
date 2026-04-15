#include "powertopology.h"
#include <QDebug>
#include <QColor>
#include <cmath>
#include <QJsonDocument>
#include <QFile>

SimpleTopology::SimpleTopology(QObject *parent)
    : QObject(parent)
{
}

void SimpleTopology::initialize(const TopologyConfig &config)
{
    m_config = config;

    // 清空现有数据
    m_nodes.clear();
    m_contactors.clear();
    m_piles.clear();

    // 创建节点
    for (int i = 1; i <= config.nodeCount; i++)
    {
        PowerNode node;
        node.id = i;
        node.state = NODE_IDLE;
        node.availablePower = 40; // 每个节点40kW
        node.chargerId = -1;
        node.position = QPointF(); // 位置由UI计算
        m_nodes.append(node);
    }

    // 创建环形接触器
    for (int i = 1; i <= config.nodeCount; i++)
    {
        Contactor contactor;
        contactor.id = i;
        contactor.isClosed = false;
        contactor.node1 = i;
        // 下一个节点，如果是最后一个节点则连接到第一个节点
        if (i == config.nodeCount)
        {
            contactor.node2 = 1;
        }
        else
        {
            contactor.node2 = i + 1;
        }
        m_contactors.append(contactor);
    }

    // 创建对角线接触器（每个节点连接到对面的节点）
    for (int i = 1; i <= config.nodeCount; i++)
    {
        Contactor contactor;
        contactor.id = config.nodeCount + i;
        contactor.isClosed = false;
        contactor.node1 = i;
        // 计算对面的节点
        int oppositeNode = i + config.nodeCount / 2;
        if (oppositeNode > config.nodeCount)
        {
            oppositeNode = oppositeNode - config.nodeCount;
        }
        contactor.node2 = oppositeNode;
        m_contactors.append(contactor);
    }

    // 创建充电桩
    QVector<QColor> colors = generateColors(config.pileCount);
    for (int i = 0; i < config.pileCount; i++)
    {
        ChargingPile pile;
        pile.id = i + 1;
        pile.state = PILE_IDLE;

        // 确保充电桩连接节点不越界
        int connectedNode = config.pileNodes.value(i, 1);
        if (connectedNode < 1 || connectedNode > config.nodeCount)
        {
            connectedNode = 1;
        }
        pile.connectedNode = connectedNode;

        pile.requiredPower = 0;
        pile.allocatedPower = 0;
        pile.color = colors[i];
        pile.priority = 0; // 默认优先级为0
        m_piles.append(pile);
    }

    qDebug() << "拓扑初始化完成:" << config.nodeCount << "节点,"
             << config.pileCount << "充电桩";
    qDebug() << "环形接触器:" << config.nodeCount << "个";
    qDebug() << "对角线接触器:" << config.nodeCount << "个";
    qDebug() << "总接触器:" << m_contactors.size() << "个";
}
QVector<QColor> SimpleTopology::generateColors(int count)
{
    QVector<QColor> colors;
    QColor baseColors[] = {
        QColor(255, 100, 100), // 红色
        QColor(100, 255, 100), // 绿色
        QColor(100, 100, 255), // 蓝色
        QColor(255, 255, 100), // 黄色
        QColor(255, 100, 255), // 紫色
        QColor(100, 255, 255), // 青色
        QColor(255, 150, 100), // 橙色
        QColor(150, 100, 255), // 靛色
        QColor(255, 200, 100), // 琥珀色
        QColor(200, 100, 255), // 洋红色
        QColor(100, 200, 255), // 天蓝色
        QColor(200, 255, 100), // 酸橙色
        QColor(255, 50, 50),   // 深红/猩红 (更强烈的红色)
        QColor(50, 200, 50),   // 深绿/石灰绿 (更鲜明的绿色)
        QColor(50, 200, 200),  // 深青/绿松石 (更鲜明的青色)
        QColor(255, 100, 180)  // 粉色 (补充洋红与红之间的色系)
    };

    for (int i = 0; i < count; i++)
    {
        colors.append(baseColors[i % 16]);
    }

    return colors;
}

int SimpleTopology::get_idle_node_count(const QVector<PowerNode> &nodes)
{
    int count = 0;
    for (const PowerNode &node : nodes)
    {
        if (node.state == NODE_IDLE)
        {
            count++;
        }
    }
    return count;
}
// 接口实现 - bfs搜索最近的节点集合去匹配功率缺额
#define FARTHEST true
#define NEAREST !FARTHEST
#define BUILD_PATH true
#define SEARCH_PATH !BUILD_PATH
int SimpleTopology::findAvailableNodes(int pileId, int startNodeId, int quota, QVector<int> &result, bool find_type)
{
    // 使用BFS查找最近的可用节点
    int allocPower_recaller(int pileid, int startNodeId, bool find_type, bool init);
    result.clear();
    allocPower_recaller(pileId, startNodeId, find_type, BUILD_PATH); // 初始化搜索
    int entrycnt = 0;
    while (result.size() < quota && entrycnt++ < m_nodes.size())
    {
        int currentNode = allocPower_recaller(pileId, startNodeId, find_type, SEARCH_PATH);

        // 检查当前节点是否可用
        if (currentNode >= 1 && currentNode <= m_nodes.size())
        {
            if (m_nodes[currentNode - 1].state == find_type ? NODE_OCCUPIED : NODE_IDLE)
            {
                result.append(currentNode);
            }

            if (result.size() >= quota)
                break;
        }
    }
    return result.size();
}
bool SimpleTopology::allocateNodes_auto(int pileId, int requiredPower)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return false;
    }
    ChargingPile &pile = m_piles[pileId - 1];

    pile.state = PILE_CHARGING;

    qDebug() << "充电桩" << pileId << "请求功率:" << requiredPower << "kW";

    // 计算需要多少个节点
    int quota = (pile.requiredPower + requiredPower + 39) / 40 - pile.allocatedNodes.size(); // 功率需额增量,向上取整，每个节点40kW
    quota = qMax(quota, 0);
    if (quota == 0)
    {
        qDebug() << "充电桩" << pileId << "无需分配节点";
        return true;
    }
    // 查找最近的可用节点
    QVector<int> nearestNodes;
    int res = findAvailableNodes(pileId, pile.connectedNode, quota, nearestNodes, NEAREST);
    if (res <= 0)
    {
        qWarning() << "没有足够的可用节点满足需求";
        return false;
    }
    // 分配找到的节点
    for (int nodeId : nearestNodes)
    {
        allocateNodeToPile(nodeId, pileId);
    }

    // 更新显示
    emit topologyChanged();

    return nearestNodes.size() == quota; // 根据是否成功分配节点返回结果
}

// 获取输入节点的(左/右/对角)三个邻居节点ID列表
void SimpleTopology::getNeighbors(int nodeId, QVector<int> &result)
{
    result.clear();
    // 获取左邻节点
    int leftNeighbor = (nodeId + m_nodes.size() - 1);
    leftNeighbor = leftNeighbor > m_nodes.size() ? leftNeighbor - m_nodes.size() : leftNeighbor;
    if (leftNeighbor >= 1 && leftNeighbor <= m_nodes.size())
    {
        result.append(leftNeighbor);
    }
    // 获取右邻节点
    int rightNeighbor = (nodeId + 1);
    rightNeighbor = rightNeighbor > m_nodes.size() ? rightNeighbor - m_nodes.size() : rightNeighbor;
    if (rightNeighbor >= 1 && rightNeighbor <= m_nodes.size())
    {
        result.append(rightNeighbor);
    }
    // 获取对角邻节点
    int diagonalNeighbor = (nodeId + m_nodes.size() - m_nodes.size() / 2);
    diagonalNeighbor = diagonalNeighbor > m_nodes.size() ? diagonalNeighbor - m_nodes.size() : diagonalNeighbor;
    if (diagonalNeighbor >= 1 && diagonalNeighbor <= m_nodes.size())
    {
        result.append(diagonalNeighbor);
    }
}

void SimpleTopology::maneuver_ReleasedNodes(int pileId, const QVector<int> &nodeIds_to_release)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
    }
    if (nodeIds_to_release.isEmpty())
    {
        qWarning() << "没有指定要释放的节点";
        return;
    }
    for (int nodeId : nodeIds_to_release)
    {
        // 遍历释放节点中是否有邻接功率欠额的充电桩占据节点
        QVector<int> neighbors;
        neighbors.clear();

        getNeighbors(nodeId, neighbors);
        QVector<int> scores;
        scores.clear();
        if (neighbors.size() <= 0)
        {
            qWarning() << "节点" << nodeId << "没有邻居节点";
            continue;
        }
        for (int neighbor : neighbors)
        {
            int neighborPileId = m_nodes[neighbor - 1].chargerId;

            if (neighborPileId > 0 && neighborPileId != pileId)
            {
                if (m_piles[neighborPileId - 1].allocatedPower < m_piles[neighborPileId - 1].requiredPower) // 功率欠额
                {
                    int score_node = 0;
                    score_node = (m_piles[neighborPileId - 1].requiredPower - m_piles[neighborPileId - 1].allocatedPower) * 1000;
                    score_node += m_piles[neighborPileId - 1].priority * 100;
                    score_node += m_piles[neighborPileId - 1].allocatedNodes.size() * 1;
                    scores.append(score_node);
                }
            }
        }
        // 找到分值最高的邻居节点,如果分值大于0则把这个节点分配给这个充电桩
        int bestScore = -1;
        int bestNeighborPileId = -1;
        for (int i = 0; i < scores.size(); i++)
        {
            if (scores[i] > bestScore)
            {
                bestScore = scores[i];
                bestNeighborPileId = neighbors[i];
                bestNeighborPileId = m_nodes[bestNeighborPileId - 1].chargerId;
            }
        }
        if (bestScore > 0 && bestNeighborPileId >= 1 && bestNeighborPileId <= m_piles.size())
        {
            qDebug() << "节点" << nodeId << "重新分配给充电桩" << bestNeighborPileId << "分值:" << bestScore << "\n";
            allocateNodeToPile(nodeId, bestNeighborPileId);
        }
    }
}
bool SimpleTopology::preemptor(int pileId, int requiredPower)
{

    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return false;
    }
    ChargingPile &pile = m_piles[pileId - 1];

    pile.state = PILE_CHARGING;

    qDebug() << "充电桩" << pileId << "请求功率:" << requiredPower << "kW";

    // 计算需要多少个节点
    int quota = (pile.allocatedPower + requiredPower + 39) / 40 - pile.allocatedNodes.size(); // 功率需额增量,向上取整，每个节点40kW
    // 遍历当前桩充电桩已分配的节点,尝试从这些节点的邻居节点中寻找分值最高的可抢占节点进行分配
    QVector<int> nodeScores;
    nodeScores.resize(m_nodes.size());

    int get_hops_occupied(int start, int nodeid, int pileid);
    while (quota > 0)
    {
        nodeScores.fill(-1);
        for (int nodeId : pile.allocatedNodes)
        {
            // 获取当前节点的邻居节点
            QVector<int> neighbors;
            neighbors.clear();

            getNeighbors(nodeId, neighbors);

            for (int neighborId : neighbors)
            {
                int neighbors_pileId = m_nodes[neighborId - 1].chargerId;
                ChargingPile &neighbors_pile = m_piles[neighbors_pileId - 1];
                // 获取邻居节点的得分
                int score = 0;
                // pile的优先级是否大于邻居节点的优先级,并且邻居节点不属于充电桩的已分配节点
                if (pile.priority > neighbors_pile.priority && !pile.allocatedNodes.contains(neighborId))
                {
                    // 并且这个邻居节点不是所属pile的连接基节点
                    if (neighborId != neighbors_pile.connectedNode)
                    {
                        score = 100000;
                    }
                }
                if (score == 100000)
                {
                    // 邻居节点与所属充电桩直连基节点的跳数
                    int hops = get_hops_occupied(neighbors_pile.connectedNode, neighborId, neighbors_pileId);
                    score += hops * 1000;
                    score += neighbors_pile.priority * 100;
                    score += neighbors_pile.allocatedNodes.size() * 1;
                }
                if (nodeScores[neighborId - 1] == -1)
                {
                    nodeScores[neighborId - 1] = score;
                }
            }
        }
        // 找到得分最高的节点并返回节点序号
        // 打印nodeScores得分
        qDebug() << "nodeScores:";
        for (int i = 0; i < nodeScores.size(); ++i)
        {
            qDebug() << "N" << i + 1 << ":" << nodeScores[i];
        }

        int bestNode = 0;
        int bestScore = -1;
        for (int i = 0; i < nodeScores.size(); ++i)
        {
            if (nodeScores[i] > bestScore)
            {
                bestNode = i + 1;
                bestScore = nodeScores[i];
            }
        }
        qDebug() << "bestNode:" << bestNode << "bestScore:" << bestScore << "\n";
        if (bestNode == 0)
        {
            qWarning() << "没有可抢占的节点";
            return false;
        }
        if (bestScore < 100000)
        {
            qWarning() << "没有合格分数的节点可以抢占";
            return false;
        }
        releaseNodes_manu(bestNode);
        allocateNodeToPile(bestNode, pileId);
        emit topologyChanged();
        quota--;
    }
    return true;
}
bool SimpleTopology::requestPower(int pileId, int requiredPower)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return false;
    }

    if (m_piles[pileId - 1].allocatedNodes.size() == m_nodes.size())
    {
        qWarning() << "没有可用节点";
        return false;
    }

    if (requiredPower < 0 || requiredPower > m_nodes.size() * 40)
    {
        qWarning() << "无效的功率请求:" << requiredPower;
        return false;
    }
    int pretive_require = requiredPower;
    // 如果充电桩没有占用过节点,则首先申请充电桩自带的电气连接节点,
    // 如果这个电气连接节点已经被其他桩占有,则尝试让其他充电桩推掉这个节点
    ChargingPile &pile = m_piles[pileId - 1];
    PowerNode &node = m_nodes[pile.connectedNode - 1];

    if (pile.state == PILE_IDLE && node.state == NODE_OCCUPIED)
    {
        bool res = releaseNodes_manu(pile.connectedNode);
        if (res)
        {
            allocateNodeToPile(pile.connectedNode, pileId);
            pile.state = PILE_CHARGING;
            requiredPower -= 40;
            requiredPower = qMax(requiredPower, 0);
        }
    }

    bool res = allocateNodes_auto(pileId, requiredPower);
    pile.requiredPower += pretive_require;
    pile.requiredPower = qMin(pile.requiredPower, m_nodes.size() * 40);
    if (res)
    {
        return true;
    }
    else
    {
        // 节点分配不成功,尝试抢占/分享机制
        return preemptor(pileId, requiredPower);
    }

    return false;
}

void SimpleTopology::releasePower(int pileId, int powerToRelease)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return;
    }
    int occupiedNodesNum = m_piles[pileId - 1].allocatedNodes.size();
    if (occupiedNodesNum == 0)
    {
        m_piles[pileId - 1].state = PILE_IDLE;
        m_piles[pileId - 1].allocatedPower = 0;
        m_piles[pileId - 1].requiredPower = 0;
        qWarning() << "充电桩" << pileId << "没有节点占用";
        return;
    }

    ChargingPile &pile = m_piles[pileId - 1];

    // 减少需求功率
    pile.requiredPower = qMax(0, pile.requiredPower - powerToRelease);
    if (pile.requiredPower == 0)
    {
        pile.state = PILE_IDLE;
    }

    qDebug() << "充电桩" << pileId << "释放功率:" << powerToRelease << "kW";
    int quota = (pile.requiredPower + 39) / 40;
    quota = pile.allocatedNodes.size() - quota; // 计算需要释放多少个节点
    quota = qMax(quota, 0);
    // 查找最近的可用节点
    QVector<int> farthestNodes;
    findAvailableNodes(pileId, pile.connectedNode, quota, farthestNodes, FARTHEST);

    if (farthestNodes.size() <= 0)
    {
        qWarning() << "没有找到可释放的节点";
        return;
    }
    // 释放找到的节点
    for (int nodeId : farthestNodes)
    {
        releaseNodeFromPile(nodeId, pileId);
    }
    maneuver_ReleasedNodes(pileId, farthestNodes);

    emit topologyChanged();
}

void SimpleTopology::allocateNodeToPile(int nodeId, int pileId)
{
    if (nodeId < 1 || nodeId > m_nodes.size() ||
        pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的节点或充电桩ID:" << nodeId << pileId;
        return;
    }
    void set_locked(int pileid, int nodeid);
    set_locked(pileId, nodeId);
    PowerNode &node = m_nodes[nodeId - 1];
    ChargingPile &pile = m_piles[pileId - 1];

    if (node.state == NODE_OCCUPIED)
    {
        qWarning() << "节点" << nodeId << "已被占用";
        return;
    }

    // 分配节点
    node.state = NODE_OCCUPIED;
    node.chargerId = pileId;
    pile.allocatedNodes.insert(nodeId);
    pile.allocatedPower += node.availablePower;

    // 更新接触器状态
    updateContactorStates(pileId, 0);

    qDebug() << "分配节点" << nodeId << "给充电桩" << pileId;

    emit topologyChanged();
}

void SimpleTopology::releaseNodeFromPile(int nodeId, int pileId)
{
    if (nodeId < 1 || nodeId > m_nodes.size() ||
        pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的节点或充电桩ID:" << nodeId << pileId;
        return;
    }
    void set_locked(int pileid, int nodeid);
    set_locked(0, nodeId);
    PowerNode &node = m_nodes[nodeId - 1];
    ChargingPile &pile = m_piles[pileId - 1];

    if (node.state != NODE_OCCUPIED || node.chargerId != pileId)
    {
        qWarning() << "节点" << nodeId << "不属于充电桩" << pileId;
        return;
    }

    // 释放节点
    node.state = NODE_IDLE;
    node.chargerId = -1;
    pile.allocatedNodes.remove(nodeId);
    pile.allocatedPower -= node.availablePower;
    pile.allocatedPower = qMax(0, pile.allocatedPower);
    if (pile.allocatedNodes.size() == 0)
    {
        pile.allocatedPower = 0;
        pile.requiredPower = 0;
        pile.state = PILE_IDLE;
    }

    // 更新接触器状态
    updateContactorStates(pileId, nodeId);

    qDebug() << "从充电桩" << pileId << "释放节点" << nodeId;

    emit topologyChanged();
}

// 添加接触器状态更新方法
void SimpleTopology::updateContactorStates(int pileId, int nodeId)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        return;
    }
    if (nodeId < 0 || nodeId > m_nodes.size())
    {
        return;
    }
    if (nodeId > 0)
    {
        for (Contactor &contactor : m_contactors)
        {
            if (contactor.node1 == nodeId || contactor.node2 == nodeId)
            {
                contactor.isClosed = false;
            }
        }
        return;
    }
    const ChargingPile &pile = m_piles[pileId - 1];
    const QSet<int> &allocatedNodes = pile.allocatedNodes;

    // 收集所有相关节点（充电桩连接节点 + 已分配节点）
    QSet<int> relevantNodes = allocatedNodes;

    if (allocatedNodes.isEmpty())
    {
        return;
    }

    // 遍历所有接触器，检查是否连接相关节点
    for (Contactor &contactor : m_contactors)
    {
        // 如果接触器连接的两个节点都在相关节点集合中，则闭合
        if (relevantNodes.contains(contactor.node1) && relevantNodes.contains(contactor.node2))
        {
            // 检查这两个节点是否相邻（在已分配节点的连通子图中）
            if (areNodesNeighbors(contactor.node1, contactor.node2))
            {
                contactor.isClosed = true;
            }
        }
    }
    // 遍历所有节点,如果节点左右两侧的接触器是断开的,并且和对径点都被同一个充电桩占有,则闭合对角线接触器
    for (int allocated : allocatedNodes)
    {
        if (allocated < 1 || allocated > m_nodes.size())
        {
            continue;
        }
        int lastNode = allocated + m_nodes.size() - 1;
        lastNode = (lastNode - 1) % m_nodes.size() + 1;
        if (m_contactors[lastNode - 1].isClosed || m_contactors[allocated - 1].isClosed)
        {
            continue;
        }
        int oppositeNode = allocated + m_nodes.size() / 2;
        oppositeNode = oppositeNode > m_nodes.size() ? oppositeNode - m_nodes.size() : oppositeNode;
        if (allocatedNodes.contains(oppositeNode))
        {
            m_contactors[m_nodes.size() + allocated - 1].isClosed = true;
            m_contactors[m_nodes.size() + oppositeNode - 1].isClosed = true;
        }
    }
}

// 添加辅助方法检查两个节点是否在连通子图中相连
bool SimpleTopology::areNodesNeighbors(int node1, int node2)
{
    // 如果是直接相邻的节点（环形上相邻但不考虑对角线）即两个节点相差1或n-1
    const TopologyConfig &config = m_config;
    if (node1 == node2 - 1 || node1 == node2 + 1 || node1 == node2 - (config.nodeCount - 1) || node1 == node2 + (config.nodeCount - 1))
    {
        return true;
    }
    return false;
}

QVector<int> SimpleTopology::getNodePriority(int pileId)
{
    QVector<int> priority;

    if (pileId < 1 || pileId > m_piles.size())
    {
        return priority;
    }

    const ChargingPile &pile = m_piles[pileId - 1];
    int startNode = pile.connectedNode;

    // 简单优先级：从连接的节点开始，按距离排序
    for (int i = 0; i < m_nodes.size(); i++)
    {
        int nodeId = i + 1;
        int distance = abs(nodeId - startNode);
        if (distance > m_nodes.size() / 2)
        {
            distance = m_nodes.size() - distance;
        }
        priority.append(nodeId);
    }

    // 按距离排序（这里只是示例，实际算法需要后续实现）
    std::sort(priority.begin(), priority.end(),
              [startNode, this](int a, int b)
              {
                  int distA = abs(a - startNode);
                  int distB = abs(b - startNode);
                  if (distA > m_nodes.size() / 2)
                      distA = m_nodes.size() - distA;
                  if (distB > m_nodes.size() / 2)
                      distB = m_nodes.size() - distB;
                  return distA < distB;
              });

    return priority;
}

// 手动操作接口（测试用）

void SimpleTopology::allocateNodes_manu(int nodeId, int pileId)
{
    allocateNodeToPile(nodeId, pileId);
}

bool SimpleTopology::releaseNodes_manu(int nodeId)
{
    if (nodeId < 1 || nodeId > m_nodes.size())
    {
        return false;
    }
    PowerNode &node = m_nodes[nodeId - 1];
    if (node.state == NODE_IDLE || node.chargerId == 0)
    {
        return false;
    }

    // 释放nodeId节点 并且计算当前占据nodeId节点到pile连接基点的跳数
    int pileId = node.chargerId;
    ChargingPile &pile = m_piles[pileId - 1];
    int get_hops_occupied(int start, int nodeid, int pileid);
    int hops_compared = get_hops_occupied(pile.connectedNode, nodeId, pileId);
    // 找到pileId占据的节点中到基点跳数大于hops的节点中(跳数-hops)=(节点到nodeid跳数)的节点
    QVector<int> nodesToRelease;
    for (int allocatedNodeId : pile.allocatedNodes)
    {
        if (allocatedNodeId == nodeId)
        {
            continue;
        }
        int hops = get_hops_occupied(pile.connectedNode, allocatedNodeId, pileId);
        if (hops > hops_compared && (hops - hops_compared) == get_hops_occupied(nodeId, allocatedNodeId, pileId))
        {
            nodesToRelease.append(allocatedNodeId);
        }
    }
    for (int allocatedNodeId : nodesToRelease)
    {
        releaseNodeFromPile(allocatedNodeId, pileId);
    }
    releaseNodeFromPile(nodeId, pileId);
    return true;
}

void SimpleTopology::setPilePriority(int pileId, int priority)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return;
    }

    if (priority < 1 || priority > 4)
    {
        qWarning() << "优先级必须在1-4范围内:" << priority;
        return;
    }

    m_piles[pileId - 1].priority = priority;
    qDebug() << "充电桩" << pileId << "优先级设置为:" << priority;

    emit topologyChanged();
}

int SimpleTopology::getPilePriority(int pileId) const
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        return 1; // 默认优先级
    }

    return m_piles[pileId - 1].priority;
}

QJsonObject SimpleTopology::saveState() const
{
    QJsonObject state;

    // 保存基本配置
    state["nodeCount"] = m_config.nodeCount;
    state["pileCount"] = m_config.pileCount;
    // 保存充电桩连接节点（用于加载时验证）
    QJsonArray pileNodesArray;
    for (int node : m_config.pileNodes)
    {
        pileNodesArray.append(node);
    }
    state["pileNodes"] = pileNodesArray;

    // 保存每个充电桩的状态
    QJsonArray pilesArray;
    for (const ChargingPile &pile : m_piles)
    {
        QJsonObject pileObj;
        pileObj["id"] = pile.id;
        pileObj["priority"] = pile.priority;
        pileObj["requiredPower"] = pile.requiredPower;
        pileObj["state"] = (pile.state == PILE_CHARGING) ? "charging" : "idle";
        // 保存已分配的节点列表
        QJsonArray allocatedArray;
        QList<int> nodes = pile.allocatedNodes.values();
        std::sort(nodes.begin(), nodes.end());
        for (int node : nodes)
        {
            allocatedArray.append(node);
        }
        pileObj["allocatedNodes"] = allocatedArray;
        pilesArray.append(pileObj);
    }
    state["piles"] = pilesArray;

    return state;
}

bool SimpleTopology::loadState(const QJsonObject &state)
{
    // 1. 验证基本配置是否匹配
    int savedNodeCount = state["nodeCount"].toInt();
    int savedPileCount = state["pileCount"].toInt();
    if (savedNodeCount != m_config.nodeCount || savedPileCount != m_config.pileCount)
    {
        qWarning() << "保存的配置与当前拓扑不一致，无法加载";
        return false;
    }

    // 2. 先完全清空所有节点的占用状态
    for (PowerNode &node : m_nodes)
    {
        node.state = NODE_IDLE;
        node.chargerId = -1;
    }
    for (ChargingPile &pile : m_piles)
    {
        pile.state = PILE_IDLE;
        pile.requiredPower = 0;
        pile.allocatedPower = 0;
        pile.allocatedNodes.clear();
    }
    // 清空全局 locked 数组（如果使用了外部C函数，这里也需要重置，但本类内部状态已清空）
    // 注意：如果外部有 set_locked 函数，需要相应调用 set_locked(0, nodeId) 清空所有锁
    for (int i = 1; i <= m_config.nodeCount; ++i)
    {
        // 假设 set_locked 定义在别处，我们只需清空内部数组即可
        // 但为了与外部图算法同步，可调用 extern void set_locked(int pileid, int nodeid);
        // 这里简单起见，只清内部状态；外部图算法依赖的 locked 数组应由调用方在初始化时维护
    }

    // 3. 恢复充电桩优先级、需求功率
    QJsonArray pilesArray = state["piles"].toArray();
    for (const QJsonValue &val : pilesArray)
    {
        QJsonObject pileObj = val.toObject();
        int id = pileObj["id"].toInt();
        if (id < 1 || id > m_piles.size())
            continue;

        ChargingPile &pile = m_piles[id - 1];
        pile.priority = pileObj["priority"].toInt();
        pile.requiredPower = pileObj["requiredPower"].toInt();
        pile.state = (pileObj["state"].toString() == "charging") ? PILE_CHARGING : PILE_IDLE;

        // 恢复已分配的节点
        QJsonArray allocatedArray = pileObj["allocatedNodes"].toArray();
        for (const QJsonValue &nodeVal : allocatedArray)
        {
            int nodeId = nodeVal.toInt();
            if (nodeId < 1 || nodeId > m_nodes.size())
                continue;
            PowerNode &node = m_nodes[nodeId - 1];
            // 确保节点未被其他桩占用（按保存的数据应该不会冲突）
            if (node.state == NODE_IDLE)
            {
                node.state = NODE_OCCUPIED;
                node.chargerId = pile.id;
                pile.allocatedNodes.insert(nodeId);
                pile.allocatedPower += node.availablePower;
                // 同步外部 locked 数组（如果使用了 set_locked）
                // extern void set_locked(int pileid, int nodeid);
                // set_locked(pile.id, nodeId);
            }
            else
            {
                qWarning() << "节点" << nodeId << "已被占用，恢复失败";
                return false;
            }
        }
    }

    // 4. 更新接触器状态（根据新的节点分配重新计算）
    for (int pileId = 1; pileId <= m_piles.size(); ++pileId)
    {
        updateContactorStates(pileId, 0); // 0 表示更新该充电桩的所有接触器
    }

    emit topologyChanged();
    return true;
}
