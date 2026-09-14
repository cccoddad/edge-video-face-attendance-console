QT += core sql

TEMPLATE = app
TARGET = AttendanceWriterTest
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
            -lopencv_imgcodecs452
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
    attendancewriter_test.cpp \
    ../src/storage/attendancewriter.cpp \
    ../src/storage/attendancerepository.cpp \
    ../src/domain/attendancestatemachine.cpp \
    ../src/storage/databasemigration.cpp \
    ../src/storage/snapshotstore.cpp \
    ../src/app/appconfig.cpp

HEADERS += \
    ../src/storage/attendancewriter.h \
    ../src/storage/attendancerepository.h \
    ../src/domain/attendancestatemachine.h \
    ../src/storage/databasemigration.h \
    ../src/storage/snapshotstore.h \
    ../src/app/appconfig.h
