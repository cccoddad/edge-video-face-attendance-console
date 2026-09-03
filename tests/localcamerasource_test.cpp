#include "../src/appconfig.h"
#include "../src/localcamerasource.h"

#include <QCoreApplication>

#include <cstdio>

namespace {
int parseBackend(const QString &name)
{
    if (name.compare(QStringLiteral("dshow"), Qt::CaseInsensitive) == 0) {
        return cv::CAP_DSHOW;
    }
    if (name.compare(QStringLiteral("msmf"), Qt::CaseInsensitive) == 0) {
        return cv::CAP_MSMF;
    }
    return -1;
}

int fourccFromName(const QString &name)
{
    if (name.size() != 4) {
        return 0;
    }
    return cv::VideoWriter::fourcc(name.at(0).toLatin1(), name.at(1).toLatin1(),
                                   name.at(2).toLatin1(), name.at(3).toLatin1());
}

QString fourccToName(double value)
{
    const int code = static_cast<int>(value);
    return QString::fromLatin1("%1%2%3%4")
            .arg(QChar(code & 0xff))
            .arg(QChar((code >> 8) & 0xff))
            .arg(QChar((code >> 16) & 0xff))
            .arg(QChar((code >> 24) & 0xff));
}

int runBackendProbe(const QString &cameraTarget, const QString &backendName,
                    int requestedWidth = 0, int requestedHeight = 0,
                    const QString &requestedFourcc = QString())
{
    bool indexOk = false;
    const int index = cameraTarget.toInt(&indexOk);
    const int backend = parseBackend(backendName);
    if ((indexOk && index < 0) || backend < 0) {
        std::fprintf(stderr, "Usage: LocalCameraSourceTest [camera-index|camera-name] [dshow|msmf]\n");
        return 3;
    }

    cv::VideoCapture capture;
    if (indexOk) {
        capture.open(index, backend);
    } else {
        capture.open(QStringLiteral("video=%1").arg(cameraTarget).toUtf8().constData(), backend);
    }
    if (!capture.isOpened()) {
        std::fprintf(stderr, "Cannot open camera %s with backend %s\n",
                     cameraTarget.toUtf8().constData(), backendName.toUtf8().constData());
        return 4;
    }

    if (!requestedFourcc.isEmpty()) {
        const int fourcc = fourccFromName(requestedFourcc);
        if (fourcc == 0) {
            std::fprintf(stderr, "Camera pixel format must contain exactly four characters\n");
            return 3;
        }
        capture.set(cv::CAP_PROP_FOURCC, fourcc);
    }
    if (requestedWidth > 0 && requestedHeight > 0) {
        capture.set(cv::CAP_PROP_FRAME_WIDTH, requestedWidth);
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, requestedHeight);
    }

    cv::Mat frame;
    for (int attempt = 0; attempt < 30; ++attempt) {
        if (capture.read(frame) && !frame.empty()) {
            const cv::Scalar brightness = cv::mean(frame);
            const QString actualFourcc = fourccToName(capture.get(cv::CAP_PROP_FOURCC));
            std::fprintf(stdout,
                         "Camera %s backend %s: %dx%d fourcc=%s mean_bgr=%.3f,%.3f,%.3f\n",
                         cameraTarget.toUtf8().constData(), backendName.toUtf8().constData(), frame.cols, frame.rows,
                         actualFourcc.toLatin1().constData(),
                         brightness[0], brightness[1], brightness[2]);
            return 0;
        }
    }

    std::fprintf(stderr, "Camera %s backend %s did not return a frame\n",
                 cameraTarget.toUtf8().constData(), backendName.toUtf8().constData());
    return 5;
}
}

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
    if (application.arguments().size() == 3) {
        return runBackendProbe(application.arguments().at(1), application.arguments().at(2));
    }
    if (application.arguments().size() == 6) {
        bool widthOk = false;
        bool heightOk = false;
        const int width = application.arguments().at(3).toInt(&widthOk);
        const int height = application.arguments().at(4).toInt(&heightOk);
        if (!widthOk || !heightOk || width <= 0 || height <= 0) {
            std::fprintf(stderr,
                         "Usage: LocalCameraSourceTest [camera-index|camera-name] [dshow|msmf] [width] [height] [fourcc]\n");
            return 3;
        }
        return runBackendProbe(application.arguments().at(1), application.arguments().at(2),
                               width, height, application.arguments().at(5));
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
