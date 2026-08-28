QT += core

TEMPLATE = app
TARGET = VideoFileSourceSmokeTest
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
    LIBS += -L$$OPENCV_ROOT/x64/mingw/lib \
            -lopencv_core452 \
            -lopencv_imgproc452 \
            -lopencv_videoio452
}

SOURCES += \
    ../src/ivideosource.cpp \
    ../src/videofilesource.cpp \
    videofilesource_smoketest.cpp

HEADERS += \
    ../src/ivideosource.h \
    ../src/videofilesource.h
