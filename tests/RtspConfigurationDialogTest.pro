QT += core widgets testlib

TEMPLATE = app
TARGET = RtspConfigurationDialogTest
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
    ../src/media/rtspconfiguration.cpp \
    ../src/media/rtspconfigurationdialog.cpp \
    rtspconfigurationdialog_test.cpp

HEADERS += \
    ../src/media/rtspconfiguration.h \
    ../src/media/rtspconfigurationdialog.h
