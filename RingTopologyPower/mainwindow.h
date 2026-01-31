#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "powertopology.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onApplyConfigClicked();
    void onRequestPowerClicked();
    void onReleasePowerClicked();
    void onPileSelectionChanged(int index);
    void onTopologyChanged();
    void onPriorityChanged();
    // 手动操作测试
    void onAllocateNodeClicked();
    void onReleaseNodeClicked();

private:
    void setupGraphicsScene();
    void updateGraphics();
    void updateStatusDisplay();
    void updatePileComboBox();

    // 计算节点位置
    QPointF calculateNodePosition(int nodeId);

    // 计算充电桩位置
    QPointF calculatePilePosition(int pileIndex);
    void drawGrid(const QRectF &rect);
    Ui::MainWindow *ui;
    SimpleTopology *m_topology;
    QGraphicsScene *m_scene;

    // 图形项
    QVector<QGraphicsEllipseItem *> m_nodeItems;
    QVector<QGraphicsLineItem *> m_contactorItems;
    QVector<QGraphicsEllipseItem *> m_pileItems;
    QVector<QGraphicsLineItem *> m_pileConnections;

    // 当前选中的节点和充电桩
    int m_selectedNode;
    int m_selectedPile;
};

#endif // MAINWINDOW_H