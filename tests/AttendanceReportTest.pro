QT += core sql

TEMPLATE = app
TARGET = AttendanceReportTest
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
    ../src/storage/attendancereport.cpp \
    attendancereport_test.cpp

HEADERS += \
    ../src/storage/attendancereport.h
