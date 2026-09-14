QT += core

TEMPLATE = app
TARGET = PerformanceMetricsTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

INCLUDEPATH += \
    $$PWD/../src/app \
    $$PWD/../src/media \
    $$PWD/../src/vision \
    $$PWD/../src/domain \
    $$PWD/../src/storage \
    $$PWD/../src/monitor \
    $$PWD/../src/ui

SOURCES += \
    performancemetrics_test.cpp \
    ../src/monitor/performancemetrics.cpp

HEADERS += \
    ../src/monitor/performancemetrics.h
