QT += core sql

TARGET = DatabaseMigrationTest
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
    databasemigration_test.cpp \
    ../src/storage/databasemigration.cpp

HEADERS += \
    ../src/storage/databasemigration.h
