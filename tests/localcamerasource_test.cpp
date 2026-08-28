#include "../src/appconfig.h"
#include "../src/localcamerasource.h"

#include <QCoreApplication>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    qputenv("FACE_ATTENDANCE_LOCAL_CAMERA_INDEX", "2");
    if (AppConfig::localCameraIndex() != 2) {
        std::fprintf(stderr, "Local camera index configuration was not read correctly\n");
        return 1;
    }

    LocalCameraSource invalidSource;
    QString errorMessage;
    if (invalidSource.open(QStringLiteral("-1"), &errorMessage)
            || invalidSource.state() != VideoSourceState::Error || errorMessage.isEmpty()) {
        std::fprintf(stderr, "Invalid camera index was not rejected\n");
        return 2;
    }

    if (application.arguments().size() == 1) {
        std::fprintf(stdout, "Local camera source validation passed\n");
        return 0;
    }
    if (application.arguments().size() != 2) {
        std::fprintf(stderr, "Usage: LocalCameraSourceTest [camera-index]\n");
        return 3;
    }

    LocalCameraSource source;
    if (!source.open(application.arguments().at(1), &errorMessage)) {
        std::fprintf(stderr, "Cannot open local camera: %s\n", errorMessage.toUtf8().constData());
        return 4;
    }
    cv::Mat frame;
    if (!source.read(frame) || frame.empty()) {
        std::fprintf(stderr, "Local camera did not return a frame\n");
        return 5;
    }
    source.close();
    if (source.state() != VideoSourceState::Stopped) {
        std::fprintf(stderr, "Local camera did not enter stopped state\n");
        return 6;
    }

    std::fprintf(stdout, "Local camera source smoke test passed\n");
    return 0;
}
