QT += core sql

TARGET = PersonnelImportExportTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    personnelimportexport_test.cpp \
    ../src/personnelimportexport.cpp

HEADERS += \
    ../src/personnelimportexport.h
