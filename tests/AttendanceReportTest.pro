QT += core sql

TEMPLATE = app
TARGET = AttendanceReportTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

SOURCES += \
    ../src/attendancereport.cpp \
    attendancereport_test.cpp

HEADERS += \
    ../src/attendancereport.h
