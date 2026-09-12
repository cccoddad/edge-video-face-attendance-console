QT += core sql

TARGET = DatabaseMigrationTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    databasemigration_test.cpp \
    ../src/databasemigration.cpp

HEADERS += \
    ../src/databasemigration.h
