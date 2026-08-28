#include "../src/videofilesource.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) {
        qCritical() << "Usage: VideoFileSourceSmokeTest <local-video-file>";
        return 2;
    }

    const QString videoPath = application.arguments().at(1);
    VideoFileSource source;
    QString errorMessage;
    if (!source.open(videoPath, &errorMessage)) {
        qCritical() << "Cannot open local fixture:" << errorMessage;
        return 3;
    }

    int frameCount = 0;
    cv::Mat frame;
    while (source.read(frame)) {
        if (frame.empty()) {
            qCritical() << "Video source reported a successful empty frame";
            return 4;
        }
        ++frameCount;
    }

    if (frameCount == 0 || source.state() != VideoSourceState::Ended) {
        qCritical() << "Unexpected playback completion state:" << frameCount
                    << IVideoSource::stateText(source.state());
        return 5;
    }

    source.close();
    if (source.state() != VideoSourceState::Stopped) {
        qCritical() << "Unexpected stop state:" << IVideoSource::stateText(source.state());
        return 6;
    }

    if (source.open(QFileInfo(videoPath).absolutePath() + "/missing-video.avi", &errorMessage)
            || source.state() != VideoSourceState::Error || errorMessage.isEmpty()) {
        qCritical() << "Open failure state was not reported";
        return 7;
    }

    std::fprintf(stdout, "VideoFileSource smoke test passed with %d frames\n", frameCount);
    return 0;
}
