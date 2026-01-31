#RingTopologyPower/
#├── RingTopologyPower.pro
#├── main.cpp
#├── mainwindow.h
#├── mainwindow.cpp
#├── mainwindow.ui
#└── powertopology.h
#RingTopologyPower.pro
QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# 根据构建类型配置
CONFIG(debug, debug|release) {
    # 调试版本设置
    CONFIG -= release
    CONFIG += debug
    TARGET = $$join(TARGET,,,_debug)
    DESTDIR = debug
    DEFINES += QT_DEBUG
    QMAKE_CXXFLAGS += -g -O0  # 启用调试信息，禁用优化
} else {
    # 发布版本设置
    CONFIG -= debug
    CONFIG += release
    DESTDIR = release
    DEFINES += QT_NO_DEBUG_OUTPUT
    QMAKE_CXXFLAGS += -O2  # 启用优化
}

# Windows 特定配置
win32 {
    # 设置应用程序图标（可选）
    # RC_ICONS = app.ico
    
    # 链接库
    LIBS += -lmingw32
    
    # 确保生成 GUI 应用
    CONFIG += windows
    QMAKE_LFLAGS_WINDOWS = -Wl,-subsystem,windows
    
    # 调试版本添加控制台
    CONFIG(debug, debug|release) {
        CONFIG += console
        QMAKE_LFLAGS += -Wl,-subsystem,console
    }
}

SOURCES += \
    directedgraph.cpp \
    main.cpp \
    mainwindow.cpp \
    powertopology.cpp

HEADERS += \
    mainwindow.h \
    powertopology.h

FORMS += \
    mainwindow.ui

# 编译选项
win32: {
    QMAKE_CXXFLAGS += -std=c++11
    QMAKE_CXXFLAGS += -finput-charset=UTF-8
    QMAKE_CXXFLAGS += -fexec-charset=UTF-8
} else: unix: {
    QMAKE_CXXFLAGS += -std=c++11
    QMAKE_CXXFLAGS += -Wall -Wextra    
}

# 链接数学库
unix: LIBS += -lm

# 设置输出目录
OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.rcc
UI_DIR = $$DESTDIR/.ui