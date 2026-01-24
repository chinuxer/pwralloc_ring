#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
// 平台特定设置
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORM", "xcb");
    printf("Running on Linux platform\r\n");
#elif defined(Q_OS_WIN)
    printf("Running on Windows platform\r\n");
#endif

    QApplication a(argc, argv);

    // 设置应用程序信息
    a.setApplicationName("RingTopologyPower");
    a.setApplicationVersion("1.0");
    a.setOrganizationName("chinuxer");

    MainWindow w;
    w.show();

    return a.exec();
}
