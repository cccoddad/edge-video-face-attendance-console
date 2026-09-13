#include "../src/appconfig.h"
#include "../src/rtspsource.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QThread>
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
    if (application.arguments().size() < 4) {
        std::fprintf(stderr,
                     "Usage: RtspSourceLoopbackTest <fixture-video> <mediamtx-exe> <ffmpeg-exe>\n");
        return 2;
    }
    const QString fixture = application.arguments().at(1);
    const QString mediamtxPath = application.arguments().at(2);
    const QString ffmpegPath = application.arguments().at(3);
    qputenv("FACE_ATTENDANCE_FFMPEG_PATH", ffmpegPath.toUtf8());

    const int port = 8567;
    const QString streamUrl = QStringLiteral("rtsp://127.0.0.1:%1/live/face").arg(port);

    const QString configPath = QDir::temp().filePath("rtsp-loopback-mediamtx.yml");
    const QString workDir = QDir::temp().filePath("rtsp-loopback-work");
    QDir().mkpath(workDir);
    {
        QFile config(configPath);
        if (!config.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::fprintf(stderr, "Cannot write mediamtx config\n");
            return 3;
        }
        config.write(QStringLiteral("rtspAddress: :%1\npaths:\n  all_others:\n").arg(port).toUtf8());
    }

    QProcess server;
    server.setProcessChannelMode(QProcess::SeparateChannels);
    server.setWorkingDirectory(workDir);
    server.start(mediamtxPath, QStringList() << configPath);
    expect(server.waitForStarted(8000), "mediamtx started");
    QThread::msleep(1500);

    QStringList pushArgs;
    pushArgs << QStringLiteral("-hide_banner")
             << QStringLiteral("-loglevel") << QStringLiteral("error")
             << QStringLiteral("-re")
             << QStringLiteral("-stream_loop") << QStringLiteral("-1")
             << QStringLiteral("-i") << fixture
             << QStringLiteral("-c:v") << QStringLiteral("libx264")
             << QStringLiteral("-preset") << QStringLiteral("ultrafast")
             << QStringLiteral("-tune") << QStringLiteral("zerolatency")
             << QStringLiteral("-g") << QStringLiteral("10")
             << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
             << QStringLiteral("-an")
             << QStringLiteral("-rtsp_transport") << QStringLiteral("tcp")
             << QStringLiteral("-f") << QStringLiteral("rtsp") << streamUrl;

    QProcess pusher;
    pusher.setProcessChannelMode(QProcess::SeparateChannels);
    pusher.start(ffmpegPath, pushArgs);
    expect(pusher.waitForStarted(8000), "ffmpeg pusher started");
    QThread::msleep(2500);

    std::fprintf(stdout, "Test: connect and pull frames via loopback RTSP\n");
    RtspSource source(1000);
    QString openError;
    const bool opened = source.open(streamUrl, &openError);
    expect(opened, "RtspSource connected to loopback RTSP");
    if (!opened) {
        std::fprintf(stderr, "open error: %s\n", qPrintable(openError));
    }

    int frames = 0;
    bool validSize = true;
    QElapsedTimer timer;
    timer.start();
    while (frames < 30 && timer.elapsed() < 20000) {
        cv::Mat frame;
        if (source.read(frame) && !frame.empty()) {
            ++frames;
            if (frame.cols != 640 || frame.rows != 480 || frame.channels() != 3) {
                validSize = false;
            }
        } else {
            QThread::msleep(20);
        }
        QCoreApplication::processEvents();
    }
    expect(frames >= 30, "pulled at least 30 frames via RTSP");
    expect(validSize, "frames are 640x480 BGR");
    expect(source.state() == VideoSourceState::Playing, "state is Playing while streaming");

    std::fprintf(stdout, "Test: detect interruption after publisher stops\n");
    pusher.kill();
    pusher.waitForFinished(3000);
    bool interrupted = false;
    QElapsedTimer interruptTimer;
    interruptTimer.start();
    while (interruptTimer.elapsed() < 15000) {
        cv::Mat frame;
        source.read(frame);
        if (source.state() == VideoSourceState::Interrupted) {
            interrupted = true;
            break;
        }
        QThread::msleep(50);
        QCoreApplication::processEvents();
    }
    expect(interrupted, "interruption detected after publisher stopped");

    std::fprintf(stdout, "Test: recover after publisher restarts\n");
    QProcess pusherAgain;
    pusherAgain.setProcessChannelMode(QProcess::SeparateChannels);
    pusherAgain.start(ffmpegPath, pushArgs);
    pusherAgain.waitForStarted(8000);

    int recoveredFrames = 0;
    QElapsedTimer recoverTimer;
    recoverTimer.start();
    while (recoveredFrames < 10 && recoverTimer.elapsed() < 30000) {
        cv::Mat frame;
        if (source.read(frame) && !frame.empty()) {
            ++recoveredFrames;
        } else {
            QThread::msleep(50);
        }
        QCoreApplication::processEvents();
    }
    expect(recoveredFrames >= 10, "frames resumed after publisher restart");
    expect(source.state() == VideoSourceState::Playing, "state Playing after recovery");

    source.close();
    pusherAgain.kill();
    pusherAgain.waitForFinished(3000);
    server.kill();
    server.waitForFinished(3000);

    std::fprintf(stdout, "\nRtspSourceLoopbackTest: %d passed, %d failed (frames=%d, recovered=%d)\n",
                 s_passCount, s_failCount, frames, recoveredFrames);
    return s_failCount > 0 ? 1 : 0;
}
