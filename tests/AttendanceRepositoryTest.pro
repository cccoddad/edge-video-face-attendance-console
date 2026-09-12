QT += core sql

TARGET = AttendanceRepositoryTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    attendancerepository_test.cpp \
    ../src/attendancerepository.cpp \
    ../src/attendancestatemachine.cpp \
    ../src/databasemigration.cpp

HEADERS += \
    ../src/attendancerepository.h \
    ../src/attendancestatemachine.h \
    ../src/databasemigration.h
