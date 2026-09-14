QT += core sql

TARGET = PersonnelImportExportTest
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
    personnelimportexport_test.cpp \
    ../src/storage/personnelimportexport.cpp

HEADERS += \
    ../src/storage/personnelimportexport.h
