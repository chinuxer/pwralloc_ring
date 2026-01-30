#ifndef POWERTOPOLOGY_H
#define POWERTOPOLOGY_H

#include <QObject>
#include <QVector>
#include <QSet>
#include <QPointF>
#include <QColor>

// 节点状态
enum NodeState
{
    NODE_IDLE = 0,
    NODE_OCCUPIED,
    NODE_FAULT
};

// 充电桩状态
enum PileState
{
    PILE_IDLE = 0,
    PILE_CHARGING
};

// 功率节点
struct PowerNode
{
    int id;             // 节点ID (1-n)
    NodeState state;    // 节点状态
    int availablePower; // 可用功率 (kW)
    int chargerId;      // 占用的充电桩ID (-1表示空闲)
    QPointF position;   // 图形位置
};

// 接触器
struct Contactor
{
    int id;        // 接触器ID
    bool isClosed; // 是否闭合
    int node1;     // 连接的节点1
    int node2;     // 连接的节点2
};

// 充电桩
struct ChargingPile
{
    int id;                   // 充电桩ID
    int priority;             // 优先级 (1-4)
    PileState state;          // 充电桩状态
    int connectedNode;        // 连接的节点ID
    int requiredPower;        // 需求功率 (kW)
    int allocatedPower;       // 已分配功率 (kW)
    QSet<int> allocatedNodes; // 已分配的节点
    QColor color;             // 显示颜色
};

// 拓扑配置
struct TopologyConfig
{
    int nodeCount;          // 节点数量
    int pileCount;          // 充电桩数量
    QVector<int> pileNodes; // 每个充电桩连接的节点
    double circleRadius;    // 环形半径
    QPointF center;         // 圆心位置
};

// 算法接口（供后续实现）
class PowerAlgorithmInterface
{
public:
    virtual ~PowerAlgorithmInterface() {}

    // 充电桩请求功率 - 需要后续实现具体的分配策略
    virtual bool requestPower(int pileId, int requiredPower) = 0;

    // 充电桩释放功率 - 需要后续实现具体的释放策略
    virtual void releasePower(int pileId, int powerToRelease) = 0;

    // 分配节点给充电桩 - 供策略调用
    virtual void allocateNodeToPile(int nodeId, int pileId) = 0;

    // 从充电桩释放节点 - 供策略调用
    virtual void releaseNodeFromPile(int nodeId, int pileId) = 0;

    // 获取节点优先级列表 - 需要后续实现具体的优先级算法
    virtual QVector<int> getNodePriority(int pileId) = 0;
};

// 简单的拓扑管理类
class SimpleTopology : public QObject, public PowerAlgorithmInterface
{
    Q_OBJECT

public:
    explicit SimpleTopology(QObject *parent = nullptr);

    // 初始化拓扑
    void initialize(const TopologyConfig &config);

    // 获取拓扑数据
    const QVector<PowerNode> &getNodes() const { return m_nodes; }
    const QVector<Contactor> &getContactors() const { return m_contactors; }
    const QVector<ChargingPile> &getChargingPiles() const { return m_piles; }
    const TopologyConfig &getConfig() const { return m_config; }

    // 接口实现 - 节点分配基础版本所需最少实现（后续可基类重写）
    int get_idle_node_count(const QVector<PowerNode> &nodes);
    int findAvailableNodes(int pileId, int startNodeId, int quota, QVector<int> &result, bool find_type);
    bool allocateNodes_auto(int pileId, int requiredPower);
    bool preemptor(int pileId, int requiredPower);
    bool requestPower(int pileId, int requiredPower) override;
    void releasePower(int pileId, int powerToRelease) override;
    void allocateNodeToPile(int nodeId, int pileId) override;
    void releaseNodeFromPile(int nodeId, int pileId) override;
    QVector<int> getNodePriority(int pileId) override;

    // 手动操作接口（用于测试）
    void allocateNodes_manu(int nodeId, int pileId);
    bool releaseNodes_manu(int nodeId);
public slots:
    // 设置充电桩优先级
    void setPilePriority(int pileId, int priority);

    // 获取充电桩优先级
    int getPilePriority(int pileId) const;

signals:
    void topologyChanged();

private:
    TopologyConfig m_config;
    QVector<PowerNode> m_nodes;
    QVector<Contactor> m_contactors;
    QVector<ChargingPile> m_piles;

    // 生成颜色列表
    QVector<QColor> generateColors(int count);
    // 更新接触器状态
    void updateContactorStates(int pileId, int nodeId);
    // 检查节点连通性
    bool areNodesNeighbors(int node1, int node2);
};

#endif // POWERTOPOLOGY_H
