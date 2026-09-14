QT += core sql

TEMPLATE = app
TARGET = AttendanceStateMachineTest
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
    ../src/storage/attendancerepository.cpp \
    ../src/storage/databasemigration.cpp \
    ../src/domain/attendancestatemachine.cpp \
    ../src/domain/checkoutconfirmation.cpp \
    attendancestatemachine_test.cpp

HEADERS += \
    ../src/storage/attendancerepository.h \
    ../src/storage/databasemigration.h \
    ../src/domain/attendancestatemachine.h \
    ../src/domain/checkoutconfirmation.h
