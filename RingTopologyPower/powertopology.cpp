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
        node.availablePower = config.unitPower; // 每个节点额定功率
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
        pile.requiredNodes = 0;
        pile.color = colors[i];
        pile.priority = 0; // 默认优先级为0
        m_piles.append(pile);
    }

    qInfo() << "拓扑初始化完成:" << config.nodeCount << "节点,"
            << config.pileCount << "充电桩";
    qInfo() << "环形接触器:" << config.nodeCount << "个";
    qInfo() << "对角线接触器:" << config.nodeCount << "个";
    qInfo() << "总接触器:" << m_contactors.size() << "个";
}
QVector<QColor> SimpleTopology::generateColors(int count)
{
    QVector<QColor> colors;
    QColor baseColors[] = {
        QColor(205, 20, 30),   // 红色
        QColor(215, 110, 15),  // 橙色
        QColor(255, 255, 100), // 黄色
        QColor(100, 255, 100), // 绿色
        QColor(100, 255, 255), // 青色
        QColor(100, 125, 255), // 蓝色
        QColor(50, 30, 110),   // 紫色
        QColor(255, 50, 225),  // 洋红色
        QColor(65, 160, 140),  // 海蓝
        QColor(130, 130, 100), // 松青
        QColor(255, 60, 120),  // 深红/猩红 (更强烈的红色)
        QColor(50, 200, 200),  // 深青/绿松石 (更鲜明的青色)
        QColor(150, 100, 255), // 靛色
        QColor(255, 200, 100), // 琥珀色
        QColor(200, 255, 100), // 酸橙色
        QColor(50, 40, 30),    // 棕色

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
bool SimpleTopology::allocateNodes_auto(int pileId, int quota)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return false;
    }
    ChargingPile &pile = m_piles[pileId - 1];

    pile.state = PILE_CHARGING;

    qInfo() << "充电桩" << pileId << "请求功率节点:" << quota << "个";

    if (quota == 0)
    {
        qInfo() << "充电桩" << pileId << "无需分配节点";
        return true;
    }
    quota = quota - pile.allocatedNodes.size();
    quota = qMax(quota, 0);
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

int SimpleTopology::makesScores(Senario senario, int pileId, int neighborPileId, int nodeId, int neighborNodeId)
{
#define WEIGHT_HIERARCHY 10
#define WEIGHT_1 1
#define WEIGHT_2 WEIGHT_HIERARCHY *WEIGHT_1
#define WEIGHT_3 WEIGHT_HIERARCHY *WEIGHT_2
#define WEIGHT_4 WEIGHT_HIERARCHY *WEIGHT_3
#define WEIGHT_5 WEIGHT_HIERARCHY *WEIGHT_4
#define WEIGHT_6 WEIGHT_HIERARCHY *WEIGHT_5
#define WEIGHT_7 WEIGHT_HIERARCHY *WEIGHT_6
#define WEIGHT_8 WEIGHT_HIERARCHY *WEIGHT_7
#define WEIGHT_9 WEIGHT_HIERARCHY *WEIGHT_8
#define WEIGHT_10 WEIGHT_HIERARCHY *WEIGHT_9
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return -1;
    }
    if (neighborPileId < 1 || neighborPileId > m_piles.size())
    {
        qWarning() << "无效的邻居充电桩ID:" << neighborPileId;
        return -1;
    }
    if (nodeId < 1 || nodeId > m_nodes.size())
    {
        qWarning() << "无效的节点ID:" << nodeId;
        return -1;
    }
    if (neighborNodeId < 1 || neighborNodeId > m_nodes.size())
    {
        qWarning() << "无效的邻居节点ID:" << neighborNodeId;
        return -1;
    }

    int get_hops_occupied(int start, int nodeid, int pileid);
    int score = 0;
    int shortage = 0;
    switch (senario)
    {
    case SENARIO_INHERIT:
        score += WEIGHT_1 * (WEIGHT_HIERARCHY * WEIGHT_HIERARCHY - 1 - m_piles[neighborPileId - 1].allocatedNodes.size());
        score += WEIGHT_3 * (m_piles[neighborPileId - 1].priority % WEIGHT_HIERARCHY);
        shortage = m_piles[neighborPileId - 1].requiredNodes - m_piles[neighborPileId - 1].allocatedNodes.size();
        shortage = shortage < 0 ? 0 : shortage;
        shortage = shortage >= WEIGHT_HIERARCHY ? WEIGHT_HIERARCHY - 1 : shortage;
        score += WEIGHT_4 * (shortage % WEIGHT_HIERARCHY);
        score = (pileId == neighborPileId) ? 0 : score; // 如果邻居节点所属充电桩与当前充电桩相同,则得分为0,不进行继承
        return score;
    case SENARIO_PREEMPT:
        score += WEIGHT_1 * m_piles[neighborPileId - 1].allocatedNodes.size();                                                                  // 已分配节点数越多得分越高
        score += WEIGHT_3 * (m_piles[neighborPileId - 1].requiredNodes - m_piles[neighborPileId - 1].allocatedNodes.size()) % WEIGHT_HIERARCHY; // 功率缺额越大得分越高
        score += WEIGHT_4 * (WEIGHT_HIERARCHY - m_piles[neighborPileId - 1].priority % WEIGHT_HIERARCHY);                                       // 优先级越低得分越高
        int hops = get_hops_occupied(nodeId, m_piles[pileId - 1].connectedNode, pileId);
        hops = hops >= WEIGHT_HIERARCHY ? (WEIGHT_HIERARCHY - 1) : hops;
        score += WEIGHT_5 * (WEIGHT_HIERARCHY - 1 - hops); // 距离越近得分越高
        // neighborNodeId的邻居节点中有多少于neighborNodeId的所属充电桩id相同的
        QVector<int> neighbors;
        neighbors.clear();

        getNeighbors(nodeId, neighbors);
        int neighbor_same_pile_count = 0;
        for (int neighbor : neighbors)
        {
            if (m_nodes[neighbor - 1].chargerId == neighborPileId)
            {
                neighbor_same_pile_count++;
            }
        }
        score += WEIGHT_6 * (neighbor_same_pile_count % WEIGHT_HIERARCHY);
        hops = get_hops_occupied(neighborNodeId, m_piles[neighborPileId - 1].connectedNode, neighborPileId);
        score += WEIGHT_7 * (hops % WEIGHT_HIERARCHY);                                                     // 距离越远得分越高
        score += WEIGHT_8 * (m_piles[neighborPileId - 1].priority < m_piles[pileId - 1].priority ? 1 : 0); // 如果邻居充电桩优先级低于充电桩优先级，则可抢占
        return score;
    }
}

/*继承策略:在所有空节点中进行遍历,找到每个空节点的邻节点所属充电桩,按继承策略的优先级权重组合计算分数,如果空节点个数不大于得分最高充电桩所欠额功率quota,则直接把所有空节点分配给这个充电桩;如果空节点个数大于得分最高充电桩所欠额功率quota,则继续找到得分第二高的充电桩,并把所有空节点分配给这个充电桩;如果得分第二高的充电桩所欠额功率quota不大于(空节点个数-得分最高充电桩所欠额功率quota),则先让得分第二高的充电桩对空节点进行抢占,然后再让得分第一高的充电桩再次对所有空节点和得分第二高的充电桩所抢占的节点进行抢占,直到满足缺额或者无法获得所有节点都抢占完或得分第二高的充电桩抢占过的空节点仅剩一个点;以此类推*/
void SimpleTopology::inheritor(int pileId, const QVector<int> &nodeIds_to_release)
{

    if (nodeIds_to_release.isEmpty())
    {
        qWarning() << "没有可继承的节点";
        return;
    }
    // 1. 计算每个充电桩的得分
    // 遍历nodeIds_to_release,找到每个节点的邻节点所属充电桩,并计算每个充电桩的得分

    for (int nodeId : nodeIds_to_release)
    {
        if (nodeId < 0 || nodeId > m_nodes.size() || m_nodes[nodeId - 1].state != NODE_IDLE)
        {
            continue;
        }
        QVector<int> pileScores;
        pileScores.resize(m_piles.size());
        pileScores.fill(-1);
        QVector<int> neighbors;
        neighbors.clear();
        getNeighbors(nodeId, neighbors);
        for (int neighborId : neighbors)
        {
            int neighborPileId = m_nodes[neighborId - 1].chargerId;
            if (neighborPileId < 1 || neighborPileId > m_piles.size())
            {
                continue;
            }
            int score = makesScores(SENARIO_INHERIT, pileId, neighborPileId, nodeId, neighborId);
            if (pileScores[neighborPileId - 1] == -1)
            {
                pileScores[neighborPileId - 1] = score;
                qInfo() << "对于继承节点" << nodeId << ",充电桩" << neighborPileId << "得分:" << score;
            }
        }
        int bestPileId = 0;
        int bestScore = -1;
        for (int i = 0; i < pileScores.size(); ++i)
        {
            if (pileScores[i] > bestScore)
            {
                bestScore = pileScores[i];
                bestPileId = i + 1;
            }
        }
        if (bestScore < WEIGHT_4 * 1) // 如果没有充电桩有功率缺口)
        {
            qWarning() << "没有得分符合标准的充电桩";
            continue;
        }
        if (bestPileId > 0)
        {
            qInfo() << "继承策略结果:充电桩" << bestPileId << "继承节点" << nodeId << "得分:" << bestScore;
            allocateNodeToPile(nodeId, bestPileId);
        }
    }
}

// 遍历释放节点中是否有特征节点(特征为三个邻节点中有与释放节点所属充电桩id相同的节点,有一个是空节点,有一个占用桩存在功率欠额大于等于两个节点)
bool SimpleTopology::maneuver_ReleasedNodes(int pileId)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return false;
    }

    const int MIN_DEFICIT_NODES = 2;

    // To store candidate piles that have sufficient deficit
    struct CandidateNode
    {
        int id;
        int pileId;
        int deficit;
        int priority;
        int idle_neighborId;
    };
    QVector<CandidateNode> CandidateNodes;
    for (int nodeId = 1; nodeId <= m_nodes.size(); nodeId++)
    {
        int deficit = 0;
        int priority = 0;
        int occupiedPileId = 0;
        int idle_neighborId = 0;
        if (m_nodes[nodeId - 1].state != NodeState::NODE_OCCUPIED)
        {
            continue;
        }

        occupiedPileId = m_nodes[nodeId - 1].chargerId;
        // 如果是直连基节点
        if (m_piles[occupiedPileId - 1].connectedNode == nodeId)
        {
            continue;
        }
        // 1. Get the 3 neighbors of the released node
        QVector<int> neighbors;
        getNeighbors(nodeId, neighbors);

        if (neighbors.size() != 3)
        {
            continue; // Should not happen in a valid ring topology with diagonals
        }

        // 2. Analyze neighbors to check for the specific features
        bool hasSamePileNeighbor = false;
        bool hasIdleNeighbor = false;
        bool hasSignificantDeficit = false;

        for (int neighborId : neighbors)
        {
            if (neighborId < 1 || neighborId > m_nodes.size())
                continue;

            PowerNode &neighborNode = m_nodes[neighborId - 1];

            // Check Feature 1: Empty Node
            if (neighborNode.state == NODE_IDLE)
            {
                hasIdleNeighbor = true;
                idle_neighborId = neighborId;
            }
            // Check Occupied Nodes
            else if (neighborNode.state == NODE_OCCUPIED)
            {

                // Check Feature 2: Same Pile Neighbor
                if (neighborNode.chargerId == occupiedPileId)
                {
                    hasSamePileNeighbor = true;
                }
                // Check Feature 3: Other Pile with Significant Deficit
                else
                {
                    ChargingPile &nPool = m_piles[neighborNode.chargerId - 1];
                    deficit = nPool.requiredNodes - nPool.allocatedNodes.size();

                    if (deficit >= MIN_DEFICIT_NODES)
                    {
                        hasSignificantDeficit = true;
                        priority = nPool.priority;
                    }
                }
            }
        }

        // 3. Verify all conditions are met
        if (hasSamePileNeighbor && hasIdleNeighbor && hasSignificantDeficit)
        {
            // Select the best candidate pile (Highest Deficit, then Highest Priority)
            CandidateNodes.append({nodeId, occupiedPileId, deficit, priority, idle_neighborId});
        }
    }
    if (!CandidateNodes.isEmpty())
    {
        // Select the best candidate pile (Highest Deficit, then Highest Priority)
        CandidateNode &bestCandidate = *std::min_element(CandidateNodes.begin(), CandidateNodes.end(), [](const CandidateNode &a, const CandidateNode &b)
                                                         { return a.deficit > b.deficit || (a.deficit == b.deficit && a.priority > b.priority); });

        if (bestCandidate.id >= 1 && bestCandidate.id <= m_piles.size())
        {
            releaseNodeFromPile(bestCandidate.id, bestCandidate.pileId);
            bool res = allocateNodes_auto(bestCandidate.pileId, m_piles[bestCandidate.pileId].allocatedNodes.size() + 1);
            if (res)
            {
                qInfo() << "动态调优结果:充电桩" << bestCandidate.pileId << "转移节点" << bestCandidate.id << "保持桩功率正常";

                return true;
            }
            else
            {
                allocateNodes_manu(bestCandidate.id, bestCandidate.pileId); // 回退占据这个节点
                // 如果那个空闲节点的邻居节点（除bestCandidate.id外)有一个被占据充电桩号和当前节点被占据充电桩号相等，则占据这个空闲节点
                QVector<int> _neighbors_;
                getNeighbors(bestCandidate.idle_neighborId, _neighbors_);
                if (_neighbors_.size() > 0)
                {
                    for (int i = 0; i < _neighbors_.size(); i++)
                    {
                        int _neighborId_ = _neighbors_[i];
                        if (_neighborId_ == bestCandidate.id)
                        {
                            continue;
                        }
                        if (m_nodes[_neighborId_ - 1].state == NODE_OCCUPIED)
                        {
                            if (m_nodes[_neighborId_ - 1].chargerId == m_nodes[bestCandidate.id - 1].chargerId)
                            {
                                allocateNodes_manu(bestCandidate.idle_neighborId, m_nodes[_neighborId_ - 1].chargerId); // 占据这个空闲节点
                                qInfo() << "动态调优结果:充电桩" << m_nodes[_neighborId_ - 1].chargerId << "转移节点" << bestCandidate.id << "到" << bestCandidate.idle_neighborId << "保持桩功率正常";
                            }
                        }
                    }
                }
            }
        }
    }
}
bool SimpleTopology::preemptor(int pileId, int quota)
{

    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return false;
    }
    ChargingPile &pile = m_piles[pileId - 1];

    pile.state = PILE_CHARGING;

    qInfo() << "充电桩" << pileId << "请求功率模块:" << quota << "个";

    quota = quota - pile.allocatedNodes.size();
    quota = qMax(quota, 0);
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
                int score = makesScores(SENARIO_PREEMPT, pileId, neighbors_pileId, nodeId, neighborId);
                if (nodeScores[neighborId - 1] == -1)
                {
                    nodeScores[neighborId - 1] = score;
                }
            }
        }
        // 找到得分最高的节点并返回节点序号
        // 打印nodeScores得分
        qInfo() << "nodeScores:";
        QString logStr;
        for (int i = 0; i < nodeScores.size(); ++i)
        {
            logStr.append(QString("N%1:%2 ").arg(i + 1).arg(nodeScores[i]));
        }
        qInfo() << "nodeScores:" << logStr;

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
        qInfo() << "bestNode:" << bestNode << "bestScore:" << bestScore << "\n";
        if (bestNode == 0)
        {
            qWarning() << "没有可抢占的节点";
            return false;
        }
        if (bestScore < (WEIGHT_8 * 1 + WEIGHT_7 * 1)) // 如果最高分都小于抢占分值,则不进行抢占)
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

    if (requiredPower < 0 || requiredPower > m_nodes.size() * m_config.unitPower)
    {
        qWarning() << "无效的功率请求:" << requiredPower;
        return false;
    }

    // 如果充电桩没有占用过节点,则首先申请充电桩自带的电气连接节点,
    // 如果这个电气连接节点已经被其他桩占有,则尝试让其他充电桩推掉这个节点
    ChargingPile &pile = m_piles[pileId - 1];
    PowerNode &node = m_nodes[pile.connectedNode - 1];
    int quota = (pile.requiredPower + requiredPower) / m_config.unitPower;
    quota += (pile.requiredPower + requiredPower) % m_config.unitPower ? 1 : 0;
    if (pile.state == PILE_IDLE && node.state == NODE_OCCUPIED)
    {
        bool res = releaseNodes_manu(pile.connectedNode);
        allocateNodeToPile(pile.connectedNode, pileId);
        pile.state = PILE_CHARGING;
        quota -= 1;
        if (res)
        {
            QVector<int> idleNodes;
            idleNodes.clear();

            for (int i = 0; i < m_nodes.size(); ++i)
            {
                if (m_nodes[i].state == NODE_IDLE)
                {
                    idleNodes.append(i + 1);
                }
            }

            int cnt = idleNodes.size();
            while (cnt-- > 0)
            {
                inheritor(pileId, idleNodes);
            }
        }
    }

    bool res = allocateNodes_auto(pileId, quota);

    pile.requiredPower += requiredPower;
    pile.requiredPower = qMin(pile.requiredPower, m_nodes.size() * m_config.unitPower);
    pile.requiredNodes = (pile.requiredPower + m_config.unitPower - 1) / m_config.unitPower; // 向上取整计算需要的节点数
    if (res)
    {
        return true;
    }
    else
    {
        // 节点分配不成功,尝试抢占/分享机制
        return preemptor(pileId, quota);
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

    qInfo() << "充电桩" << pileId << "释放功率:" << QString::asprintf("%.1f", powerToRelease / 10.0) << "kW";
    int quota = (pile.requiredPower + m_config.unitPower - 1) / m_config.unitPower;
    pile.requiredNodes = quota;
    quota = occupiedNodesNum - quota; // 计算需要释放多少个节点
    quota = qMax(quota, 0);
    if (quota <= 0)
    {
        emit topologyChanged();
        return;
    }
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
    // bool res = true;
    // int cnt = 0;
    // while (res && cnt < m_config.pileCount) // 添加循环次数限制，避免无限循环
    //{
    //   res = maneuver_ReleasedNodes(pileId, farthestNodes);
    //  if (!res)
    //  {
    //     qWarning() << "没有可优化节点移动";
    //      break;
    // }
    // cnt++;
    // }

    // 找到所有空节点
    // maneuver_ReleasedNodes(pileId);
    QVector<int> idleNodes;
    idleNodes.clear();
    qInfo() << "空闲节点:";
    for (int i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].state == NODE_IDLE)
        {
            qInfo() << i + 1 << " ";
            idleNodes.append(i + 1);
        }
    }
    if (idleNodes.size() <= 0)
    {
        qWarning() << "没有空闲节点";
        return;
    }

    int cnt = idleNodes.size();
    while (cnt-- > 0)
    {

        inheritor(pileId, idleNodes);
    }
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

    // 更新接触器状态
    updateContactorStates(pileId, 0);

    qInfo() << "分配节点" << nodeId << "给充电桩" << pileId;

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
    pile.requiredNodes = (pile.requiredPower + m_config.unitPower - 1) / m_config.unitPower;

    if (pile.allocatedNodes.size() == 0)
    {
        pile.requiredPower = 0;
        pile.state = PILE_IDLE;
    }

    // 更新接触器状态
    updateContactorStates(pileId, nodeId);

    qInfo() << "从充电桩" << pileId << "释放节点" << nodeId;

    emit topologyChanged();
}

// 添加接触器状态更新方法
void SimpleTopology::updateContactorStates(int pileId, int nodeId)
{
    if (pileId < 1 || pileId > m_piles.size())
        return;
    if (m_piles[pileId - 1].state != PILE_CHARGING)
        return;

    // 处理单节点释放：仅断开与该节点相关的接触器
    if (nodeId > 0)
    {
        for (Contactor &c : m_contactors)
            if (c.node1 == nodeId || c.node2 == nodeId)
                c.isClosed = false;
        // return;
    }

    // 整体更新：基于并查集构建无环生成树
    const ChargingPile &pile = m_piles[pileId - 1];
    QSet<int> allocated = pile.allocatedNodes;
    if (allocated.isEmpty())
        return;

    // 充电桩连接节点必须视为已占有（即使它可能未被显式分配）
    allocated.insert(pile.connectedNode);

    // ---- 1. 收集所有可能的边（环形边 + 对角线边） ----
    struct Edge
    {
        int u, v;
        bool diagonal; // false:环形边, true:对角线边
    };
    QVector<Edge> candidates;
    int half = m_config.nodeCount / 2;

    for (int u : allocated)
    {
        // 左邻 (环形)
        int left = (u == 1) ? m_config.nodeCount : u - 1;
        if (allocated.contains(left) && left < u) // 去重：仅当 left < u 时加入
            candidates.push_back({left, u, false});
        // 右邻 (环形)
        int right = (u == m_config.nodeCount) ? 1 : u + 1;
        if (allocated.contains(right) && right < u)
            candidates.push_back({right, u, false});
        // 对径点
        int opp = u + half;
        if (opp > m_config.nodeCount)
            opp -= m_config.nodeCount;
        if (allocated.contains(opp) && opp < u)
            candidates.push_back({opp, u, true});
    }

    // ---- 2. 并查集准备 ----
    QMap<int, int> parent;
    std::function<int(int)> find = [&](int x) -> int
    {
        if (parent[x] != x)
            parent[x] = find(parent[x]);
        return parent[x];
    };
    auto unite = [&](int x, int y)
    { parent[find(x)] = find(y); };

    for (int v : allocated)
        parent[v] = v;

    // ---- 3. 临时关闭所有与该充电桩相关的接触器 ----
    for (Contactor &c : m_contactors)
        if (allocated.contains(c.node1) && allocated.contains(c.node2))
            c.isClosed = false;

    // ---- 4. 贪心选择边（优先环形边，后对角线边）构建生成树 ----
    // 先处理环形边（保证局部连通）
    for (const Edge &e : candidates)
    {
        if (e.diagonal)
            continue;
        if (find(e.u) != find(e.v))
        {
            unite(e.u, e.v);
            // 闭合对应的接触器
            for (Contactor &c : m_contactors)
                if ((c.node1 == e.u && c.node2 == e.v) || (c.node1 == e.v && c.node2 == e.u))
                {
                    c.isClosed = true;
                    break;
                }
        }
    }
    // 再处理对角线边，仅用于连接尚未连通的分量
    for (const Edge &e : candidates)
    {
        if (!e.diagonal)
            continue;
        if (find(e.u) != find(e.v))
        {
            unite(e.u, e.v);
            for (Contactor &c : m_contactors)
                if ((c.node1 == e.u && c.node2 == e.v) || (c.node1 == e.v && c.node2 == e.u))
                {
                    c.isClosed = true;
                    break;
                }
        }
    }
    // 此时所得边集一定无环，且连接了可能的最大分量（若所有节点属于同一分量则全连通）
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
    releaseNodeFromPile(nodeId, pileId);
    if (nodesToRelease.size() <= 0)
    {
        return false;
    }
    for (int allocatedNodeId : nodesToRelease)
    {
        releaseNodeFromPile(allocatedNodeId, pileId);
    }

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
    // qInfo() << "充电桩" << pileId << "优先级设置为:" << priority;

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
    void set_locked(int pileid, int nodeid);
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
        pileObj["requiredNodes"] = pile.requiredNodes;
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
    // 保存locked数组状态
    QJsonArray lockedArray;
    int get_locked(int nodeid);
    lockedArray.append(0);
    for (int i = 1; i <= m_nodes.size(); i++)
    {
        lockedArray.append(get_locked(i));
    }
    state["locked"] = lockedArray;
    return state;
}

bool SimpleTopology::loadState(const QJsonObject &state)
{
    void set_locked(int pileid, int nodeid);
    // 1. 验证基本配置是否匹配
    int savedNodeCount = state["nodeCount"].toInt();
    int savedPileCount = state["pileCount"].toInt();
    if (savedNodeCount != m_config.nodeCount || savedPileCount != m_config.pileCount)
    {
        qWarning() << "保存的配置与当前拓扑不一致，无法加载";
        return false;
    }

    // 2. 先完全清空所有节点的占用状态

    for (int i = 0; i < m_config.nodeCount; i++)
    {
        m_nodes[i].state = NODE_IDLE;
        m_nodes[i].chargerId = -1;
    }
    for (int i = 0; i < m_config.pileCount; i++)
    {
        m_piles[i].priority = 0;
        m_piles[i].requiredPower = 0;
        m_piles[i].requiredNodes = 0;
        m_piles[i].state = PILE_IDLE;
        m_piles[i].allocatedNodes.clear();
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
        pile.requiredNodes = pileObj["requiredNodes"].toInt();
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
    // 复制locked状态
    QJsonArray lockedArray = state["locked"].toArray();
    set_locked(0, 0);
    for (int i = 1; i <= m_config.nodeCount; i++)
    {
        int pileId = lockedArray[i].toInt();
        if (pileId < 1 || pileId > m_config.pileCount)
        {
            continue;
        }
        set_locked(pileId, i);
    }

    // 4. 更新接触器状态（根据新的节点分配重新计算）
    for (int pileId = 1; pileId <= m_piles.size(); ++pileId)
    {
        if (m_piles[pileId - 1].state == PILE_CHARGING)
        {
            updateContactorStates(pileId, 0); // 0 表示更新该充电桩的所有接触器
        }
    }

    emit topologyChanged();
    return true;
}

void SimpleTopology::stopCharging(int pileId)
{
    if (pileId < 1 || pileId > m_piles.size())
    {
        qWarning() << "无效的充电桩ID:" << pileId;
        return;
    }

    ChargingPile &pile = m_piles[pileId - 1];
    if (pile.state != PILE_CHARGING && pile.allocatedNodes.isEmpty())
    {
        qInfo() << "充电桩" << pileId << "已经空闲，无需结束充电";
        return;
    }

    // 释放所有已分配的节点
    releasePower(pileId, pile.requiredPower);

    // 清空功率需求并设置状态为空闲
    pile.requiredPower = 0;
    pile.requiredNodes = 0;
    pile.state = PILE_IDLE;

    // 更新接触器状态（该桩不再占用任何节点，所有与其相关的接触器应断开）
    updateContactorStates(pileId, 0);

    qInfo() << "充电桩" << pileId << "已结束充电，释放所有节点";

    emit topologyChanged();
}
