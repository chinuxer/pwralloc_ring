#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "powertopology.h"
#include <QDebug>
#include <QMessageBox>
#include <QGraphicsTextItem>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_topology(new SimpleTopology(this)), m_scene(new QGraphicsScene(this)), m_selectedNode(-1), m_selectedPile(-1)
{
    ui->setupUi(this);

    // 初始化UI
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);

    // 默认配置
    ui->nodeCountSpinBox->setValue(18);
    ui->pileCountSpinBox->setValue(9);

    // 设置节点列表
    ui->nodeListWidget->clear();
    ui->nodeListWidget->addItem("点击节点选择");
    for (int i = 1; i <= ui->nodeCountSpinBox->value(); i++)
    {
        ui->nodeListWidget->addItem(QString("节点 %1").arg(i));
    }

    // 连接信号槽
    connect(ui->applyConfigButton, &QPushButton::clicked,
            this, &MainWindow::onApplyConfigClicked);
    connect(ui->requestButton, &QPushButton::clicked,
            this, &MainWindow::onRequestPowerClicked);
    connect(ui->releaseButton, &QPushButton::clicked,
            this, &MainWindow::onReleasePowerClicked);
    connect(ui->pileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPileSelectionChanged);
    connect(ui->prioritySpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPriorityChanged);
    connect(ui->nodeListWidget, &QListWidget::currentRowChanged,
            [this](int row)
            { m_selectedNode = row; });
    connect(ui->allocateNodeButton, &QPushButton::clicked,
            this, &MainWindow::onAllocateNodeClicked);
    connect(ui->releaseNodeButton, &QPushButton::clicked,
            this, &MainWindow::onReleaseNodeClicked);

    connect(m_topology, &SimpleTopology::topologyChanged,
            this, &MainWindow::onTopologyChanged);

    // 初始配置
    onApplyConfigClicked();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onApplyConfigClicked()
{
    int nodeCount = ui->nodeCountSpinBox->value();
    int pileCount = ui->pileCountSpinBox->value();

    // 验证配置
    if (nodeCount % 2 != 0)
    {
        QMessageBox::warning(this, "配置错误", "节点数量必须是偶数");
        ui->nodeCountSpinBox->setValue(nodeCount + 1);
        nodeCount = nodeCount + 1; // 更新变量
    }

    if (pileCount <= 0)
    {
        QMessageBox::warning(this, "配置错误", "充电桩数量必须大于0");
        return;
    }
    ui->nodeListWidget->clear();
    ui->nodeListWidget->addItem("点击节点选择");
    for (int i = 1; i <= nodeCount; i++)
    {
        ui->nodeListWidget->addItem(QString("节点 %1").arg(i));
    }
    // 确保充电桩数量不超过节点数量的一半
    if (pileCount > nodeCount / 2)
    {
        pileCount = nodeCount / 2;
        ui->pileCountSpinBox->setValue(pileCount);
        QMessageBox::information(this, "配置调整",
                                 QString("充电桩数量已调整为节点数量的一半: %1").arg(pileCount));
    }

    // 生成充电桩连接节点
    QVector<int> pileNodes;
    if (pileCount > 0)
    {
        int nodesPerPile = nodeCount / pileCount;
        for (int i = 0; i < pileCount; i++)
        {
            // 确保节点ID在有效范围内
            int nodeId = i * nodesPerPile + 1;
            if (nodeId > nodeCount)
            {
                nodeId = nodeCount;
            }
            pileNodes.append(nodeId);
        }
    }

    // 更新配置文本框
    QString pileConfigText;
    for (int i = 0; i < pileNodes.size(); i++)
    {
        pileConfigText += QString("桩%1->节点%2\n").arg(i + 1).arg(pileNodes[i]);
    }
    ui->pileConfigTextEdit->setPlainText(pileConfigText);

    // 创建配置
    TopologyConfig config;
    config.nodeCount = nodeCount;
    config.pileCount = pileCount;
    config.pileNodes = pileNodes;
    config.circleRadius = 200.0;
    config.center = QPointF(300, 300);

    // 初始化拓扑
    m_topology->initialize(config);

    // 更新UI
    updatePileComboBox();
    setupGraphicsScene(); // 重新设置场景
    onTopologyChanged();
    void pau_init(int nodes, int piles);
    pau_init(nodeCount, pileCount);
    ui->logTextEdit->append(QString("✓ 配置已应用: %1节点, %2充电桩").arg(nodeCount).arg(pileCount));
}
void MainWindow::onRequestPowerClicked()
{
    int pileId = ui->pileComboBox->currentIndex() + 1;
    int power = ui->powerSpinBox->value();
    int priority = ui->prioritySpinBox->value();

    if (pileId < 1 || pileId > m_topology->getChargingPiles().size())
    {
        QMessageBox::warning(this, "错误", "请选择有效的充电桩");
        return;
    }

    // 设置优先级
    QMetaObject::invokeMethod(m_topology, "setPilePriority",
                              Qt::QueuedConnection,
                              Q_ARG(int, pileId),
                              Q_ARG(int, priority));

    // 调用算法接口
    bool success = m_topology->requestPower(pileId, power);

    if (success)
    {
        ui->logTextEdit->append(QString("✓ 充电桩%1 (优先级%2) 请求 %3kW 功率成功").arg(pileId).arg(priority).arg(power));
    }
    else
    {
        ui->logTextEdit->append(QString("✗ 充电桩%1 (优先级%2) 功率请求失败").arg(pileId).arg(priority));
    }
}

void MainWindow::onReleasePowerClicked()
{
    int pileId = ui->pileComboBox->currentIndex() + 1;
    int power = ui->powerSpinBox->value();

    if (pileId < 1 || pileId > m_topology->getChargingPiles().size())
    {
        QMessageBox::warning(this, "错误", "请选择有效的充电桩");
        return;
    }

    // 调用算法接口（后续实现）
    m_topology->releasePower(pileId, power);

    ui->logTextEdit->append(QString("→ 充电桩%1 释放 %2kW 功率").arg(pileId).arg(power));
}

void MainWindow::onPileSelectionChanged(int index)
{
    if (index < 0)
        return;

    m_selectedPile = index + 1;

    // 显示充电桩信息
    const auto &piles = m_topology->getChargingPiles();
    if (index < piles.size())
    {
        const auto &pile = piles[index];
        ui->pileInfoTextEdit->setPlainText(
            QString("充电桩 %1\n"
                    "状态: %2\n"
                    "连接节点: %3\n"
                    "需求功率: %4kW\n"
                    "已分配功率: %5kW\n"
                    "占用节点数: %6\n"
                    "优先级: %7") // 添加优先级显示
                .arg(pile.id)
                .arg(pile.state == PILE_CHARGING ? "充电中" : "空闲")
                .arg(pile.connectedNode)
                .arg(pile.requiredPower)
                .arg(pile.allocatedPower)
                .arg(pile.allocatedNodes.size())
                .arg(pile.priority));

        // 同步优先级选择框的值
        ui->prioritySpinBox->setValue(pile.priority);
    }
}
void MainWindow::onPriorityChanged()
{
    int priority = ui->prioritySpinBox->value();

    if (m_selectedPile > 0)
    {
        // 调用拓扑类的方法设置优先级
        // 注意：需要在 SimpleTopology 中添加 setPilePriority 方法
        QMetaObject::invokeMethod(m_topology, "setPilePriority",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, m_selectedPile),
                                  Q_ARG(int, priority));

        ui->logTextEdit->append(QString("→ 充电桩%1优先级更新为%2").arg(m_selectedPile).arg(priority));
    }
}
void MainWindow::onTopologyChanged()
{
    updateGraphics();
    updateStatusDisplay();
}

void MainWindow::onAllocateNodeClicked()
{
    if (m_selectedNode <= 0 || m_selectedPile <= 0)
    {
        QMessageBox::warning(this, "错误", "请先选择节点和充电桩");
        return;
    }

    // 手动分配节点（测试用）
    m_topology->allocateNodes_manu(m_selectedNode, m_selectedPile);
    ui->logTextEdit->append(QString("→ 手动分配: 节点%1 -> 充电桩%2").arg(m_selectedNode).arg(m_selectedPile));
}

void MainWindow::onReleaseNodeClicked()
{
    if (m_selectedNode <= 0)
    {
        QMessageBox::warning(this, "错误", "请先选择节点");
        return;
    }

    // 手动释放节点（测试用）
    m_topology->releaseNodes_manu(m_selectedNode);
    ui->logTextEdit->append(QString("→ 手动释放: 节点%1").arg(m_selectedNode));
}

void MainWindow::setupGraphicsScene()
{
    m_scene->clear();
    m_nodeItems.clear();
    m_contactorItems.clear();
    m_pileItems.clear();
    m_pileConnections.clear();

    const auto &config = m_topology->getConfig();
    const auto &nodes = m_topology->getNodes();
    const auto &contactors = m_topology->getContactors();
    const auto &piles = m_topology->getChargingPiles();

    // 创建节点图形项
    m_nodeItems.resize(nodes.size());
    for (int i = 0; i < nodes.size(); i++)
    {
        const auto &node = nodes[i];
        QPointF pos = calculateNodePosition(node.id);

        QGraphicsEllipseItem *item = new QGraphicsEllipseItem(-12, -12, 24, 24);
        item->setPos(pos);
        item->setBrush(Qt::lightGray);
        item->setPen(QPen(Qt::darkGray, 1));
        item->setData(0, node.id); // 存储节点ID
        m_scene->addItem(item);
        m_nodeItems[i] = item;

        // 节点标签
        QGraphicsTextItem *label = new QGraphicsTextItem(QString::number(node.id));
        label->setPos(pos.x() - 12, pos.y() - 12);
        label->setDefaultTextColor(Qt::black);
        label->setFont(QFont("Arial", 10));
        label->setZValue(1); // Bring labels to front
        m_scene->addItem(label);
    }

    // 创建接触器图形项 - 添加边界检查
    m_contactorItems.resize(contactors.size());
    for (int i = 0; i < contactors.size(); i++)
    {
        const auto &contactor = contactors[i];

        // 检查节点ID是否有效
        if (contactor.node1 < 1 || contactor.node1 > config.nodeCount ||
            contactor.node2 < 1 || contactor.node2 > config.nodeCount)
        {
            qWarning() << "无效的接触器节点:" << contactor.id << contactor.node1 << "-" << contactor.node2;
            continue;
        }

        QPointF pos1 = calculateNodePosition(contactor.node1);
        QPointF pos2 = calculateNodePosition(contactor.node2);

        QGraphicsLineItem *line = new QGraphicsLineItem(pos1.x(), pos1.y(), pos2.x(), pos2.y());

        // 前一半是环形接触器 <gray>，后一半是对角线接触器<darkgray>
        if (i < config.nodeCount)
        {
            line->setPen(QPen(Qt::gray, 2, Qt::DashLine)); // 环形接触器
        }
        else
        {
            line->setPen(QPen(Qt::darkGray, 1, Qt::DashLine)); // 对角线接触器
        }
        m_scene->addItem(line);
        m_contactorItems[i] = line;
    }

    // 创建充电桩图形项
    m_pileItems.resize(piles.size());
    m_pileConnections.resize(piles.size());
    for (int i = 0; i < piles.size(); i++)
    {
        const auto &pile = piles[i];

        // 检查充电桩连接节点是否有效
        if (pile.connectedNode < 1 || pile.connectedNode > config.nodeCount)
        {
            qWarning() << "充电桩" << pile.id << "连接了无效的节点:" << pile.connectedNode;
            continue;
        }

        // 充电桩位置
        QPointF pilePos = calculatePilePosition(i);

        // 充电桩图形
        QGraphicsEllipseItem *pileItem = new QGraphicsEllipseItem(-15, -15, 30, 30);
        pileItem->setPos(pilePos);
        pileItem->setBrush(pile.color);
        pileItem->setPen(QPen(Qt::lightGray, 1));
        pileItem->setData(0, pile.id); // 存储充电桩ID
        m_scene->addItem(pileItem);
        m_pileItems[i] = pileItem;

        // 充电桩标签（显示功率）
        QGraphicsTextItem *label = new QGraphicsTextItem(
            QString("P%1\n%2kW").arg(pile.id).arg(pile.requiredPower));
        label->setPos(pilePos.x() - 12, pilePos.y() - 12);
        label->setDefaultTextColor(Qt::black);
        label->setFont(QFont("Arial", 10, QFont::Bold));
        label->setZValue(1); // Bring labels to front
        m_scene->addItem(label);

        // 连接线
        QPointF nodePos = calculateNodePosition(pile.connectedNode);
        QGraphicsLineItem *connLine = new QGraphicsLineItem(
            nodePos.x(), nodePos.y(), pilePos.x(), pilePos.y());
        connLine->setPen(QPen(Qt::lightGray, 2, Qt::DashLine));
        m_scene->addItem(connLine);
        m_pileConnections[i] = connLine;
    }

    // 标题
    QGraphicsTextItem *title = new QGraphicsTextItem(
        QString("环形拓扑功率分配系统 - %1节点 %2充电桩").arg(config.nodeCount).arg(config.pileCount));
    title->setPos(140, 700);
    title->setDefaultTextColor(Qt::darkBlue);
    title->setFont(QFont("Arial", 12, QFont::Bold));
    m_scene->addItem(title);
}
void MainWindow::updateGraphics()
{
    const auto &nodes = m_topology->getNodes();
    const auto &contactors = m_topology->getContactors();
    const auto &piles = m_topology->getChargingPiles();
    const auto &config = m_topology->getConfig();

    // 更新节点颜色
    for (int i = 0; i < nodes.size() && i < m_nodeItems.size(); i++)
    {
        const auto &node = nodes[i];
        QBrush brush = Qt::lightGray;

        if (node.state == NODE_OCCUPIED && node.chargerId > 0)
        {
            // 根据充电桩颜色着色
            int chargerIndex = node.chargerId - 1;
            if (chargerIndex >= 0 && chargerIndex < piles.size())
            {
                brush = piles[chargerIndex].color;
            }
        }
        else if (node.state == NODE_FAULT)
        {
            brush = Qt::darkRed;
        }

        if (m_nodeItems[i])
        {
            m_nodeItems[i]->setBrush(brush);
        }
    }

    // 更新接触器 - 根据连接的充电桩着色
    for (int i = 0; i < contactors.size() && i < m_contactorItems.size(); i++)
    {
        if (m_contactorItems[i])
        {
            const auto &contactor = contactors[i];
            QPen pen;

            if (contactor.isClosed)
            {
                // 查找接触器连接的两个节点所属的充电桩
                int chargerId1 = -1;
                int chargerId2 = -1;

                // 检查节点1属于哪个充电桩
                for (const auto &pile : piles)
                {
                    if (pile.allocatedNodes.contains(contactor.node1))
                    {
                        chargerId1 = pile.id;
                        break;
                    }
                }

                // 检查节点2属于哪个充电桩
                for (const auto &pile : piles)
                {
                    if (pile.allocatedNodes.contains(contactor.node2))
                    {
                        chargerId2 = pile.id;
                        break;
                    }
                }

                // 确定使用哪个充电桩的颜色（优先使用节点1的充电桩）
                int chargerId = chargerId1 == chargerId2 ? chargerId1 : -1;

                if (chargerId > 0 && chargerId <= piles.size())
                {
                    // 使用充电桩的颜色，加粗显示
                    QColor pileColor = piles[chargerId - 1].color;
                    pen = QPen(pileColor, 2, Qt::SolidLine);
                }
                else
                {
                    // 默认灰色
                    pen = QPen(Qt::gray, 2, Qt::SolidLine);
                }
            }
            else
            {
                // 未闭合的接触器显示为灰色虚线
                if (i < config.nodeCount)
                {
                    pen = QPen(Qt::gray, 2, Qt::DashLine); // 环形接触器
                }
                else
                {
                    pen = QPen(Qt::darkGray, 1, Qt::DashLine); // 对角线接触器
                }
            }
            m_contactorItems[i]->setPen(pen);
        }
    }

    // 更新充电桩连接线 - 根据充电桩状态着色
    for (int i = 0; i < piles.size() && i < m_pileConnections.size(); i++)
    {
        if (m_pileConnections[i])
        {
            const auto &pile = piles[i];

            // 如果充电桩有分配的节点，则连接线使用充电桩颜色
            if (!pile.allocatedNodes.isEmpty())
            {
                m_pileConnections[i]->setPen(QPen(pile.color, 3, Qt::SolidLine));
            }
            else
            {
                m_pileConnections[i]->setPen(QPen(Qt::lightGray, 2, Qt::DashLine));
            }
            m_pileConnections[i]->setZValue(-1);
        }
    }

    // 更新充电桩标签 - 添加优先级显示
    for (int i = 0; i < piles.size() && i < m_pileItems.size(); i++)
    {
        if (m_pileItems[i])
        {
            m_pileItems[i]->setBrush(piles[i].color);

            // 更新标签
            QPointF pos = m_pileItems[i]->pos();
            // 移除旧标签
            QList<QGraphicsItem *> items = m_scene->items(pos);
            for (QGraphicsItem *item : items)
            {
                if (QGraphicsTextItem *textItem = dynamic_cast<QGraphicsTextItem *>(item))
                {
                    if (textItem->toPlainText().contains(QString("P%1").arg(piles[i].id)))
                    {
                        m_scene->removeItem(textItem);
                        delete textItem;
                        break;
                    }
                }
            }

            // 新标签包含优先级信息
            QGraphicsTextItem *label = new QGraphicsTextItem(
                QString("P%1\n%2kW\n优先级%3").arg(piles[i].id).arg(piles[i].requiredPower).arg(piles[i].priority));
            label->setPos(pos.x() - 15, pos.y() - 20);
            label->setDefaultTextColor(Qt::black);
            label->setFont(QFont("Arial", 12, QFont::Bold));
            label->setZValue(1); // Bring labels to front
            m_scene->addItem(label);
        }
    }
}
void MainWindow::updateStatusDisplay()
{
    const auto &nodes = m_topology->getNodes();
    const auto &piles = m_topology->getChargingPiles();

    QString statusText;

    // 充电桩状态
    statusText += "=== 充电桩状态 ===\n";
    for (const auto &pile : piles)
    {
        QString nodeList;
        QList<int> allocated = pile.allocatedNodes.values();
        std::sort(allocated.begin(), allocated.end());

        for (int nodeId : allocated)
        {
            nodeList += QString::number(nodeId) + " ";
        }

        statusText += QString("桩%1: %2kW/%3kW [%4]\n")
                          .arg(pile.id)
                          .arg(pile.allocatedPower)
                          .arg(pile.requiredPower)
                          .arg(nodeList.isEmpty() ? "无节点" : nodeList);
    }

    // 节点状态
    statusText += "\n=== 节点状态 ===\n";
    int occupied = 0, idle = 0;
    for (const auto &node : nodes)
    {
        if (node.state == NODE_OCCUPIED)
            occupied++;
        else if (node.state == NODE_IDLE)
            idle++;
    }
    statusText += QString("占用: %1 | 空闲: %2 | 总数: %3\n")
                      .arg(occupied)
                      .arg(idle)
                      .arg(nodes.size());

    ui->statusTextEdit->setPlainText(statusText);
}

void MainWindow::updatePileComboBox()
{
    ui->pileComboBox->clear();

    const auto &piles = m_topology->getChargingPiles();
    for (const auto &pile : piles)
    {
        ui->pileComboBox->addItem(
            QString("充电桩%1 (节点%2)").arg(pile.id).arg(pile.connectedNode));
    }

    if (!piles.isEmpty())
    {
        ui->pileComboBox->setCurrentIndex(0);
        onPileSelectionChanged(0);
    }
}

QPointF MainWindow::calculateNodePosition(int nodeId)
{
    const auto &config = m_topology->getConfig();

    double angle = 2 * M_PI * (nodeId - 1) / config.nodeCount;
    double x = config.center.x() + config.circleRadius * cos(angle);
    double y = config.center.y() + config.circleRadius * sin(angle);

    return QPointF(x, y);
}

QPointF MainWindow::calculatePilePosition(int pileIndex)
{
    const auto &config = m_topology->getConfig();
    const auto &piles = m_topology->getChargingPiles();

    if (pileIndex < 0 || pileIndex >= piles.size())
    {
        return QPointF();
    }

    int nodeId = piles[pileIndex].connectedNode;
    QPointF nodePos = calculateNodePosition(nodeId);
    QPointF center = config.center;

    // 计算从圆心到节点的方向向量
    QPointF direction = nodePos - center;
    double length = sqrt(direction.x() * direction.x() + direction.y() * direction.y());

    if (length > 0)
    {
        direction = direction / length;
    }

    // 在节点外侧延伸
    return nodePos + direction * 80;
}
