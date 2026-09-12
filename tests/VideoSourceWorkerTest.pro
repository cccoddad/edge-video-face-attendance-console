QT += core

TEMPLATE = app
TARGET = VideoSourceWorkerTest
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
    videosourceworker_test.cpp \
    ../src/videosourceworker.cpp \
    ../src/ivideosource.cpp \
    ../src/videofilesource.cpp \
    ../src/localcamerasource.cpp \
    ../src/rtspsource.cpp \
    ../src/rtspreconnectscheduler.cpp \
    ../src/rtspconfiguration.cpp

HEADERS += \
    ../src/videosourceworker.h \
    ../src/ivideosource.h \
    ../src/videofilesource.h \
    ../src/localcamerasource.h \
    ../src/rtspsource.h \
    ../src/rtspreconnectscheduler.h \
    ../src/rtspconfiguration.h
