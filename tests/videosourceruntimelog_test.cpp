#include "../src/videosourceruntimelog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdio>

static int s_passCount = 0;
static int s_failCount = 0;

static void expect(bool cond, const char *label)
{
    if (cond) {
        ++s_passCount;
    } else {
        ++s_failCount;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

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
    expect(events.size() == 2, "capacity limits to 2 events");
    expect(events.first().occurredAt == secondTime, "first event is second recorded");
    expect(events.last().occurredAt == thirdTime, "last event is third recorded");

    const QString interruptedText = VideoSourceRuntimeLog::formatEvent(events.first());
    expect(interruptedText.contains("09:00:02"), "format contains timestamp");
    expect(interruptedText.contains("RTSP"), "format contains source type");
    expect(interruptedText.contains("视频输入中断"), "format contains state text");
    expect(!interruptedText.contains(QLatin1Char('\n')), "format has no newline");

    expect(VideoSourceRuntimeLog::sourceTypeText("local-camera") == "本机摄像头",
           "sourceTypeText for local-camera");

    QTemporaryDir dir;
    expect(dir.isValid(), "temp dir created");

    const QString savePath = dir.filePath("log.txt");
    QString error;
    expect(log.saveToFile(savePath, &error), "saveToFile succeeds");
    expect(error.isEmpty(), "saveToFile no error");
    expect(QFileInfo::exists(savePath), "log file created");

    QFile readFile(savePath);
    readFile.open(QIODevice::ReadOnly | QIODevice::Text);
    const QString content = QString::fromUtf8(readFile.readAll());
    expect(content.contains("09:00:02"), "file contains timestamp");
    expect(content.contains("rtsp"), "file contains source type");
    expect(content.contains("|"), "file uses pipe delimiter");
    readFile.close();

    VideoSourceRuntimeLog loaded(50);
    expect(loaded.loadFromFile(savePath, &error), "loadFromFile succeeds");
    expect(error.isEmpty(), "loadFromFile no error");
    const QList<VideoSourceRuntimeEvent> loadedEvents = loaded.events();
    expect(loadedEvents.size() == 2, "loaded 2 events");
    expect(loadedEvents.first().sourceType == "rtsp", "first event source preserved");
    expect(loadedEvents.first().state == VideoSourceState::Interrupted, "state preserved");
    expect(loadedEvents.first().detail == "等待重连 不会跨行", "detail preserved with newline sanitized");

    VideoSourceRuntimeLog empty(10);
    expect(empty.loadFromFile(dir.filePath("nonexistent.txt"), &error), "load from nonexistent returns true");
    expect(empty.events().isEmpty(), "empty log stays empty");

    VideoSourceRuntimeLog log2(5);
    log2.record("test", VideoSourceState::Playing, "x", QDateTime(QDate(2026,1,1), QTime(0,0,0)));
    expect(log2.events().size() == 1, "log2 has 1 event before clear");
    log2.clear();
    expect(log2.events().isEmpty(), "log2 empty after clear");

    std::fprintf(stdout, "VideoSourceRuntimeLog test: %d passed, %d failed\n",
                 s_passCount, s_failCount);
    return s_failCount > 0 ? 1 : 0;
}
