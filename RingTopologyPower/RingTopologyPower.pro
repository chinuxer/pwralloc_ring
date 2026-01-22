#RingTopologyPower/
#├── RingTopologyPower.pro
#├── main.cpp
#├── mainwindow.h
#├── mainwindow.cpp
#├── mainwindow.ui
#└── powertopology.h
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
CONFIG += console
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
