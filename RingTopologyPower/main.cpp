#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include "telnetconsole.h"

// 全局指针，用于消息处理器访问
static TelnetConsole *g_telnetConsole = nullptr;

// 自定义消息处理器
// ANSI 颜色代码
#define ANSI_COLOR_RESET "\033[0m"
#define ANSI_COLOR_DEBUG "\033[36m"    // 青色
#define ANSI_COLOR_INFO "\033[32m"     // 绿色
#define ANSI_COLOR_WARNING "\033[33m"  // 黄色
#define ANSI_COLOR_CRITICAL "\033[31m" // 红色
#define ANSI_COLOR_FATAL "\033[31;1m"  // 红色加粗

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 构建带颜色的消息（用于 telnet）
    QString coloredMsg;
    QString plainMsg; // 用于控制台输出（不带颜色，保持可读）

    switch (type)
    {
    case QtDebugMsg:
        coloredMsg = QString("%1[Debug] %2%3").arg(ANSI_COLOR_DEBUG).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Debug] %1").arg(msg);
        break;
    case QtInfoMsg:
        coloredMsg = QString("%1[Info] %2%3").arg(ANSI_COLOR_INFO).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Info] %1").arg(msg);
        break;
    case QtWarningMsg:
        coloredMsg = QString("%1[Warning] %2%3").arg(ANSI_COLOR_WARNING).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Warning] %1").arg(msg);
        break;
    case QtCriticalMsg:
        coloredMsg = QString("%1[Critical] %2%3").arg(ANSI_COLOR_CRITICAL).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Critical] %1").arg(msg);
        break;
    case QtFatalMsg:
        coloredMsg = QString("%1[Fatal] %2%3").arg(ANSI_COLOR_FATAL).arg(msg).arg(ANSI_COLOR_RESET);
        plainMsg = QString("[Fatal] %1").arg(msg);
        break;
    }

    // 输出到控制台/日志文件（不带颜色，避免乱码）
    fprintf(stderr, "%s\n", plainMsg.toLocal8Bit().constData());
    fflush(stderr);

    // 异步发送带颜色的消息到 Telnet 客户端
    if (g_telnetConsole)
    {
        QMetaObject::invokeMethod(g_telnetConsole, "doSendToAll",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, coloredMsg));
    }

    if (type == QtFatalMsg)
    {
        abort();
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 创建 Telnet 控制台
    TelnetConsole telnetConsole;
    g_telnetConsole = &telnetConsole;
    telnetConsole.start(24601);

    // 安装消息处理器
    qInstallMessageHandler(messageHandler);

    // 平台特定设置
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORM", "xcb");
    printf("Running on Linux platform\r\n");
#elif defined(Q_OS_WIN)
    printf("Running on Windows platform\r\n");
#endif

    a.setApplicationName("RingTopologyPower");
    a.setApplicationVersion("1.0");
    a.setOrganizationName("chinuxer");

    MainWindow w;
    w.show();

    int ret = a.exec();

    // 清理：恢复默认消息处理器
    qInstallMessageHandler(nullptr);
    g_telnetConsole = nullptr;

    return ret;
}