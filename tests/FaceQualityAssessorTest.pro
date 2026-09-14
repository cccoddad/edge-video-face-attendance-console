QT += core
QT -= gui

TARGET = FaceQualityAssessorTest
CONFIG += console c++11
CONFIG -= app_bundle

TEMPLATE = app

include($$PWD/../src/third_party.pri)

isEmpty(THIRD_PARTY_ROOT) {
    error("Set THIRD_PARTY_ROOT in third_party.pri.")
}

win32 {
    OPENCV_ROOT = $$THIRD_PARTY_ROOT/opencv452
    SEETAFACE_ROOT = $$THIRD_PARTY_ROOT/SeetaFace

    INCLUDEPATH += $$OPENCV_ROOT/include
    INCLUDEPATH += $$OPENCV_ROOT/include/opencv2
    INCLUDEPATH += $$SEETAFACE_ROOT/include
    INCLUDEPATH += $$SEETAFACE_ROOT/include/seeta

    LIBS += -L$$OPENCV_ROOT/x64/mingw/lib \
            -lopencv_core452 \
            -lopencv_imgproc452 \
            -lopencv_imgcodecs452 \
            -lopencv_videoio452

    LIBS += -L$$SEETAFACE_ROOT/lib \
            -lSeetaFaceDetector \
            -lSeetaFaceLandmarker \
            -lSeetaQualityAssessor \
            -lSeetaNet
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
    facequalityassessor_test.cpp \
    ../src/vision/facequalitypolicy.cpp

HEADERS += \
    ../src/vision/facequalitypolicy.h
