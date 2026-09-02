QT += core

TEMPLATE = app
TARGET = VideoSourceRuntimeLogTest
CONFIG += console c++11 warn_on
CONFIG -= app_bundle

include($$PWD/../src/third_party.pri)

isEmpty(THIRD_PARTY_ROOT) {
    error("Set THIRD_PARTY_ROOT in src/third_party.pri. See src/third_party.pri.example.")
}

win32 {
    OPENCV_ROOT = $$THIRD_PARTY_ROOT/opencv452
    INCLUDEPATH += $$OPENCV_ROOT/include
    INCLUDEPATH += $$OPENCV_ROOT/include/opencv2
}

SOURCES += \
    ../src/ivideosource.cpp \
    ../src/videosourceruntimelog.cpp \
    videosourceruntimelog_test.cpp

HEADERS += \
    ../src/ivideosource.h \
    ../src/videosourceruntimelog.h
