QT += core sql

TEMPLATE = app
TARGET = AttendanceStateMachineTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

SOURCES += \
    ../src/attendancerepository.cpp \
    ../src/attendancestatemachine.cpp \
    ../src/checkoutconfirmation.cpp \
    attendancestatemachine_test.cpp

HEADERS += \
    ../src/attendancerepository.h \
    ../src/attendancestatemachine.h \
    ../src/checkoutconfirmation.h
