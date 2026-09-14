QT += core

TEMPLATE = app
TARGET = RtspSourceLoopbackTest
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
            -lopencv_core452
}

INCLUDEPATH += \
    $$PWD/../src/app \
    $$PWD/../src/media \
    $$PWD/../src/vision \
    $$PWD/../src/domain \
    $$PWD/../src/storage \
    $$PWD/../src/monitor \
    $$PWD/../src/ui

SOURCES += \
    rtspsourceloopback_test.cpp \
    ../src/media/rtspsource.cpp \
    ../src/media/rtspreconnectscheduler.cpp \
    ../src/media/rtspconfiguration.cpp \
    ../src/app/appconfig.cpp \
    ../src/media/ivideosource.cpp

HEADERS += \
    ../src/media/rtspsource.h \
    ../src/media/rtspreconnectscheduler.h \
    ../src/media/rtspconfiguration.h \
    ../src/app/appconfig.h \
    ../src/media/ivideosource.h
