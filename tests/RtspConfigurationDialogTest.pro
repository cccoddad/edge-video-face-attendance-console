QT += core widgets testlib

TEMPLATE = app
TARGET = RtspConfigurationDialogTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

SOURCES += \
    ../src/rtspconfiguration.cpp \
    ../src/rtspconfigurationdialog.cpp \
    rtspconfigurationdialog_test.cpp

HEADERS += \
    ../src/rtspconfiguration.h \
    ../src/rtspconfigurationdialog.h
