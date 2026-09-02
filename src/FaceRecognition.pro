QT += core gui sql

TARGET = FaceAttendance

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
CONFIG += warn_on

# Keep machine-specific dependency paths out of version control.
include($$PWD/third_party.pri)

isEmpty(THIRD_PARTY_ROOT) {
    error("Set THIRD_PARTY_ROOT in third_party.pri. See third_party.pri.example.")
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
            -lopencv_videoio452 \
            -lopencv_highgui452

    LIBS += -L$$SEETAFACE_ROOT/lib \
            -lSeetaFaceDetector \
            -lSeetaFaceTracker \
            -lSeetaFaceLandmarker \
            -lSeetaNet \
            -lSeetaFaceRecognizer \
            -lSeetaQualityAssessor
}
unix{
    INCLUDEPATH += /opt/opencv4-pc/include
    INCLUDEPATH += /opt/opencv4-pc/include/opencv4
    INCLUDEPATH += /opt/opencv4-pc/include/opencv4/opencv2
    INCLUDEPATH += /opt/opencv4-pc/include/seeta/
    LIBS += -L/opt/opencv4-pc/lib/ -lopencv_world -lSeetaFaceDetector   -lSeetaFaceTracker\
            -lSeetaFaceLandmarker  -lSeetaNet  -lSeetaFaceRecognizer  -lSeetaQualityAssessor

}


# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    appconfig.cpp \
    attendancereport.cpp \
    attendancerepository.cpp \
    attendancestatemachine.cpp \
    checkoutconfirmation.cpp \
    ivideosource.cpp \
    localcamerasource.cpp \
    main.cpp \
    facerecognitionwin.cpp \
    qfaceobject.cpp \
    qquerywidget.cpp \
    qregisterwidget.cpp \
    rtspsource.cpp \
    snapshotstore.cpp \
    theme.cpp \
    videosourceruntimelog.cpp \
    videofilesource.cpp

HEADERS += \
    appconfig.h \
    attendancereport.h \
    attendancerepository.h \
    attendancestatemachine.h \
    checkoutconfirmation.h \
    ivideosource.h \
    localcamerasource.h \
    facerecognitionwin.h \
    qfaceobject.h \
    qquerywidget.h \
    qregisterwidget.h \
    rtspsource.h \
    snapshotstore.h \
    theme.h \
    videosourceruntimelog.h \
    videofilesource.h

FORMS += \
    facerecognitionwin.ui \
    qquerywidget.ui \
    qregisterwidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

