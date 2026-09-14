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

INCLUDEPATH += \
    $$PWD/app \
    $$PWD/media \
    $$PWD/vision \
    $$PWD/domain \
    $$PWD/storage \
    $$PWD/monitor \
    $$PWD/ui

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    app/main.cpp \
    app/appconfig.cpp \
    media/ivideosource.cpp \
    media/videofilesource.cpp \
    media/localcamerasource.cpp \
    media/rtspsource.cpp \
    media/rtspconfiguration.cpp \
    media/rtspconfigurationdialog.cpp \
    media/rtspreconnectscheduler.cpp \
    media/videosourceworker.cpp \
    vision/qfaceobject.cpp \
    vision/facequalitypolicy.cpp \
    domain/attendancestatemachine.cpp \
    domain/checkoutconfirmation.cpp \
    storage/attendancerepository.cpp \
    storage/databasemigration.cpp \
    storage/snapshotstore.cpp \
    storage/attendancereport.cpp \
    storage/personnelimportexport.cpp \
    storage/attendancewriter.cpp \
    monitor/videosourceruntimelog.cpp \
    monitor/performancemetrics.cpp \
    ui/facerecognitionwin.cpp \
    ui/qregisterwidget.cpp \
    ui/qquerywidget.cpp \
    ui/theme.cpp

HEADERS += \
    app/appconfig.h \
    media/ivideosource.h \
    media/videofilesource.h \
    media/localcamerasource.h \
    media/rtspsource.h \
    media/rtspconfiguration.h \
    media/rtspconfigurationdialog.h \
    media/rtspreconnectscheduler.h \
    media/videosourceworker.h \
    vision/qfaceobject.h \
    vision/facequalitypolicy.h \
    domain/attendancestatemachine.h \
    domain/checkoutconfirmation.h \
    storage/attendancerepository.h \
    storage/databasemigration.h \
    storage/snapshotstore.h \
    storage/attendancereport.h \
    storage/personnelimportexport.h \
    storage/attendancewriter.h \
    monitor/videosourceruntimelog.h \
    monitor/performancemetrics.h \
    ui/facerecognitionwin.h \
    ui/qregisterwidget.h \
    ui/qquerywidget.h \
    ui/theme.h

FORMS += \
    ui/facerecognitionwin.ui \
    ui/qquerywidget.ui \
    ui/qregisterwidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

