#include "videofilesource.h"

#include <QFileInfo>

VideoFileSource::VideoFileSource()
    : m_state(VideoSourceState::Closed)
    , m_loopEnabled(false)
{
}

VideoFileSource::~VideoFileSource()
{
    m_capture.release();
}

bool VideoFileSource::open(const QString &location, QString *errorMessage)
{
    m_capture.release();
    m_filePath.clear();
    m_lastError.clear();
    m_state = VideoSourceState::Opening;

    const QFileInfo fileInfo(location);
    if (location.isEmpty() || !fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        setError(QStringLiteral("无法读取本地视频文件：%1").arg(location));
    } else if (!m_capture.open(fileInfo.absoluteFilePath().toUtf8().constData())) {
        setError(QStringLiteral("OpenCV 无法打开视频文件：%1").arg(fileInfo.absoluteFilePath()));
    } else {
        m_filePath = fileInfo.absoluteFilePath();
        m_state = VideoSourceState::Playing;
        return true;
    }

    if (errorMessage) {
        *errorMessage = m_lastError;
    }
    return false;
}

bool VideoFileSource::read(cv::Mat &frame)
{
    frame.release();
    if (m_state != VideoSourceState::Playing) {
        return false;
    }

    if (m_capture.read(frame) && !frame.empty()) {
        return true;
    }

    if (m_loopEnabled && m_capture.set(cv::CAP_PROP_POS_FRAMES, 0)
            && m_capture.read(frame) && !frame.empty()) {
        return true;
    }

    m_state = VideoSourceState::Ended;
    return false;
}

void VideoFileSource::close()
{
    const bool wasActive = m_capture.isOpened() || m_state == VideoSourceState::Playing;
    m_capture.release();
    if (wasActive) {
        m_state = VideoSourceState::Stopped;
    }
}

VideoSourceState VideoFileSource::state() const
{
    return m_state;
}

QString VideoFileSource::lastError() const
{
    return m_lastError;
}

QString VideoFileSource::displayName() const
{
    return m_filePath;
}

void VideoFileSource::setLoopEnabled(bool enabled)
{
    m_loopEnabled = enabled;
}

bool VideoFileSource::isLoopEnabled() const
{
    return m_loopEnabled;
}

void VideoFileSource::setError(const QString &message)
{
    m_capture.release();
    m_lastError = message;
    m_state = VideoSourceState::Error;
}
