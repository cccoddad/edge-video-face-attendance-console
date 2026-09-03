#include "rtspsource.h"
#include "rtspconfiguration.h"

RtspSource::RtspSource(int reconnectIntervalMilliseconds)
    : m_state(VideoSourceState::Closed)
    , mReconnectScheduler(reconnectIntervalMilliseconds)
{
}

RtspSource::~RtspSource()
{
    releaseCapture();
}

bool RtspSource::open(const QString &location, QString *errorMessage)
{
    close();
    m_location = location.trimmed();
    m_lastError.clear();
    mReconnectScheduler.clear();
    if (!isValidRtspUrl(m_location)) {
        m_state = VideoSourceState::Error;
        m_lastError = QStringLiteral("RTSP 地址无效：必须包含 rtsp 协议和主机名");
        if (errorMessage) {
            *errorMessage = m_lastError;
        }
        return false;
    }
    return connectToStream(false, errorMessage);
}

bool RtspSource::read(cv::Mat &frame)
{
    frame.release();
    if (m_state == VideoSourceState::Interrupted) {
        if (!mReconnectScheduler.isDue(QDateTime::currentDateTime())) {
            return false;
        }
        if (!connectToStream(true)) {
            return false;
        }
    }
    if (m_state != VideoSourceState::Playing) {
        return false;
    }
    if (readCapture(frame) && !frame.empty()) {
        return true;
    }

    setInterrupted(QStringLiteral("RTSP 视频读取中断，等待自动重连"));
    return false;
}

void RtspSource::close()
{
    const bool wasActive = captureIsOpen() || m_state == VideoSourceState::Playing
            || m_state == VideoSourceState::Interrupted || m_state == VideoSourceState::Reconnecting;
    releaseCapture();
    mReconnectScheduler.clear();
    if (wasActive) {
        m_state = VideoSourceState::Stopped;
    }
}

VideoSourceState RtspSource::state() const
{
    return m_state;
}

QString RtspSource::lastError() const
{
    return m_lastError;
}

QString RtspSource::displayName() const
{
    return RtspConfiguration(m_location).displayName();
}

bool RtspSource::isValidRtspUrl(const QString &location)
{
    return RtspConfiguration::isValidUrl(location);
}

bool RtspSource::openCapture(const QString &location)
{
    return m_capture.open(location.toUtf8().constData(), cv::CAP_FFMPEG);
}

bool RtspSource::readCapture(cv::Mat &frame)
{
    return m_capture.read(frame);
}

bool RtspSource::captureIsOpen() const
{
    return m_capture.isOpened();
}

void RtspSource::releaseCapture()
{
    m_capture.release();
}

bool RtspSource::connectToStream(bool reconnecting, QString *errorMessage)
{
    releaseCapture();
    m_state = reconnecting ? VideoSourceState::Reconnecting : VideoSourceState::Opening;
    if (openCapture(m_location)) {
        m_state = VideoSourceState::Playing;
        m_lastError.clear();
        return true;
    }

    const QString message = reconnecting
            ? QStringLiteral("RTSP 重连失败，将继续等待")
            : QStringLiteral("OpenCV 无法连接 RTSP 只读视频源");
    if (reconnecting) {
        setInterrupted(message);
    } else {
        m_state = VideoSourceState::Error;
        m_lastError = message;
    }
    if (errorMessage) {
        *errorMessage = m_lastError;
    }
    return false;
}

void RtspSource::setInterrupted(const QString &message)
{
    releaseCapture();
    m_state = VideoSourceState::Interrupted;
    m_lastError = message;
    mReconnectScheduler.schedule(QDateTime::currentDateTime());
}
