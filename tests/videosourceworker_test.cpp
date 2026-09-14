#include "../src/media/videosourceworker.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QMetaType>
#include <QThread>
#include <QTimer>
#include <cstdio>

Q_DECLARE_METATYPE(cv::Mat)

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
    qRegisterMetaType<cv::Mat>("cv::Mat");

    if (application.arguments().size() != 2) {
        std::fprintf(stderr, "Usage: VideoSourceWorkerTest <local-video-file>\n");
        return 2;
    }
    const QString videoPath = application.arguments().at(1);

    std::fprintf(stdout, "Test: open failure reported from worker thread\n");
    {
        QThread thread;
        VideoSourceWorker worker;
        worker.moveToThread(&thread);
        thread.start();

        bool failedReceived = false;
        QString failedType;
        QString failedError;
        QEventLoop loop;
        QObject::connect(&worker, &VideoSourceWorker::sourceOpenFailed, &application,
                         [&](const QString &type, const QString &error) {
            failedReceived = true;
            failedType = type;
            failedError = error;
            loop.quit();
        });

        QMetaObject::invokeMethod(&worker, "openFile", Qt::QueuedConnection,
                                  Q_ARG(QString, QFileInfo(videoPath).absolutePath() + "/missing.avi"),
                                  Q_ARG(bool, false));
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();

        expect(failedReceived, "open failure signal received");
        expect(failedType == QStringLiteral("video-file"), "open failure source type is video-file");
        expect(!failedError.isEmpty(), "open failure error message is not empty");

        QMetaObject::invokeMethod(&worker, "stop", Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait(3000);
    }

    std::fprintf(stdout, "Test: fixture video plays to end in worker thread\n");
    int frames = 0;
    {
        QThread thread;
        VideoSourceWorker worker;
        worker.moveToThread(&thread);
        thread.start();

        bool opened = false;
        bool finished = false;
        int finishedState = -1;
        QEventLoop loop;
        QObject::connect(&worker, &VideoSourceWorker::sourceOpened, &application,
                         [&](const QString &sourceType, const QString &, int) {
            opened = sourceType == QStringLiteral("video-file");
        });
        QObject::connect(&worker, &VideoSourceWorker::frameReady, &application,
                         [&](const cv::Mat &frame) {
            if (!frame.empty()) {
                ++frames;
            }
        });
        QObject::connect(&worker, &VideoSourceWorker::sourceReadFinished, &application,
                         [&](int state) {
            finished = true;
            finishedState = state;
            loop.quit();
        });

        QMetaObject::invokeMethod(&worker, "openFile", Qt::QueuedConnection,
                                  Q_ARG(QString, videoPath), Q_ARG(bool, false));
        QTimer::singleShot(20000, &loop, &QEventLoop::quit);
        loop.exec();

        expect(opened, "source opened signal received in main thread");
        expect(finished, "read finished signal received");
        expect(finishedState == static_cast<int>(VideoSourceState::Ended), "finished state is Ended");
        expect(frames >= 20, "at least 20 frames delivered via signal");

        QMetaObject::invokeMethod(&worker, "stop", Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait(3000);
    }

    std::fprintf(stdout, "\nVideoSourceWorkerTest: %d passed, %d failed (frames=%d)\n",
                 s_passCount, s_failCount, frames);
    return s_failCount > 0 ? 1 : 0;
}
