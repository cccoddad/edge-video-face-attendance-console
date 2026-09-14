QT += core sql

TARGET = AttendanceRepositoryTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += \
    $$PWD/../src/app \
    $$PWD/../src/media \
    $$PWD/../src/vision \
    $$PWD/../src/domain \
    $$PWD/../src/storage \
    $$PWD/../src/monitor \
    $$PWD/../src/ui

SOURCES += \
    attendancerepository_test.cpp \
    ../src/storage/attendancerepository.cpp \
    ../src/domain/attendancestatemachine.cpp \
    ../src/storage/databasemigration.cpp

HEADERS += \
    ../src/storage/attendancerepository.h \
    ../src/domain/attendancestatemachine.h \
    ../src/storage/databasemigration.h
