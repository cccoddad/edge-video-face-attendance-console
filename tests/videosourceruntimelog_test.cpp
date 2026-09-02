#include "../src/videosourceruntimelog.h"

#include <QCoreApplication>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    VideoSourceRuntimeLog log(2);
    const QDateTime firstTime(QDate(2026, 9, 2), QTime(9, 0, 1));
    const QDateTime secondTime(QDate(2026, 9, 2), QTime(9, 0, 2));
    const QDateTime thirdTime(QDate(2026, 9, 2), QTime(9, 0, 3));
    log.record(QStringLiteral("video-file"), VideoSourceState::Opening,
               QStringLiteral("准备打开夹具"), firstTime);
    log.record(QStringLiteral("rtsp"), VideoSourceState::Interrupted,
               QStringLiteral("等待重连\n不会跨行"), secondTime);
    log.record(QStringLiteral("local-camera"), VideoSourceState::Stopped,
               QStringLiteral("用户停止"), thirdTime);

    const QList<VideoSourceRuntimeEvent> events = log.events();
    if (events.size() != 2 || events.first().occurredAt != secondTime
            || events.last().occurredAt != thirdTime) {
        std::fprintf(stderr, "Runtime event capacity or ordering failed\n");
        return 2;
    }
    const QString interruptedText = VideoSourceRuntimeLog::formatEvent(events.first());
    if (!interruptedText.contains(QStringLiteral("09:00:02"))
            || !interruptedText.contains(QStringLiteral("RTSP"))
            || !interruptedText.contains(QStringLiteral("视频输入中断"))
            || interruptedText.contains(QLatin1Char('\n'))) {
        std::fprintf(stderr, "Runtime event formatting failed\n");
        return 3;
    }
    if (VideoSourceRuntimeLog::sourceTypeText(QStringLiteral("local-camera"))
            != QStringLiteral("本机摄像头")) {
        std::fprintf(stderr, "Runtime source type text failed\n");
        return 4;
    }

    std::fprintf(stdout, "Video source runtime log test passed\n");
    return 0;
}
