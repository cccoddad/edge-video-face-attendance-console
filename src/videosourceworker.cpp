#include "videosourceworker.h"

#include "localcamerasource.h"
#include "rtspsource.h"
#include "videofilesource.h"

#include <QTimer>

namespace {
const int kPollIntervalMilliseconds = 40;
}

VideoSourceWorker::VideoSourceWorker(QObject *parent)
    : QObject(parent)
    , mPollTimer(nullptr)
    , mLastReportedState(VideoSourceState::Closed)
{
}

VideoSourceWorker::~VideoSourceWorker() = default;

void VideoSourceWorker::openFile(const QString &filePath, bool loopEnabled)
{
    std::unique_ptr<VideoFileSource> source(new VideoFileSource);
    source->setLoopEnabled(loopEnabled);
    startSource(std::move(source), QStringLiteral("video-file"), filePath);
}

void VideoSourceWorker::openCamera(int cameraIndex)
{
    startSource(std::unique_ptr<IVideoSource>(new LocalCameraSource),
                QStringLiteral("local-camera"), QString::number(cameraIndex));
}

void VideoSourceWorker::openRtsp(const QString &url, int reconnectIntervalMilliseconds)
{
    startSource(std::unique_ptr<IVideoSource>(new RtspSource(reconnectIntervalMilliseconds)),
                QStringLiteral("rtsp"), url);
}

void VideoSourceWorker::stop()
{
    if (mPollTimer) {
        mPollTimer->stop();
    }
    if (mSource) {
        mSource->close();
        mSource.reset();
    }
    mSourceType.clear();
    mLastReportedState = VideoSourceState::Stopped;
    mLastReportedError.clear();
}

void VideoSourceWorker::poll()
{
    if (!mSource) {
        if (mPollTimer) {
            mPollTimer->stop();
        }
        return;
    }

    cv::Mat frame;
    const bool hasFrame = mSource->read(frame);

    const VideoSourceState currentState = mSource->state();
    const QString currentError = mSource->lastError();
    if (currentState != mLastReportedState || currentError != mLastReportedError) {
        mLastReportedState = currentState;
        mLastReportedError = currentError;
        emit sourceStateChanged(static_cast<int>(currentState), currentError);
    }

    if (!hasFrame) {
        if (IVideoSource::shouldKeepPolling(currentState)) {
            return;
        }
        mPollTimer->stop();
        emit sourceReadFinished(static_cast<int>(currentState));
        return;
    }

    emit frameReady(frame);
}

void VideoSourceWorker::startSource(std::unique_ptr<IVideoSource> source,
                                    const QString &sourceType, const QString &location)
{
    stop();

    mSource = std::move(source);
    mSourceType = sourceType;

    QString errorMessage;
    if (!mSource->open(location, &errorMessage)) {
        emit sourceOpenFailed(sourceType, errorMessage);
        mSource.reset();
        mSourceType.clear();
        return;
    }

    mLastReportedState = mSource->state();
    mLastReportedError = mSource->lastError();
    emit sourceOpened(sourceType, mSource->displayName(), static_cast<int>(mSource->state()));

    if (!mPollTimer) {
        mPollTimer = new QTimer(this);
        mPollTimer->setInterval(kPollIntervalMilliseconds);
        connect(mPollTimer, &QTimer::timeout, this, &VideoSourceWorker::poll);
    }
    mPollTimer->start();
}
