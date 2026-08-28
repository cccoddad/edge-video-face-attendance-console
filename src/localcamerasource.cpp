#include "localcamerasource.h"

LocalCameraSource::LocalCameraSource()
    : m_cameraIndex(-1)
    , m_state(VideoSourceState::Closed)
{
}

LocalCameraSource::~LocalCameraSource()
{
    m_capture.release();
}

bool LocalCameraSource::open(const QString &location, QString *errorMessage)
{
    close();
    m_lastError.clear();
    m_state = VideoSourceState::Opening;

    bool ok = false;
    const int cameraIndex = location.trimmed().toInt(&ok);
    if (!ok || cameraIndex < 0 || cameraIndex > 15) {
        setError(QStringLiteral("本机摄像头编号无效：%1").arg(location));
    } else if (!m_capture.open(cameraIndex, cv::CAP_DSHOW)) {
        setError(QStringLiteral("无法打开本机摄像头 #%1：设备不存在、被占用或未授权 Windows 摄像头权限")
                 .arg(cameraIndex));
    } else {
        m_cameraIndex = cameraIndex;
        m_state = VideoSourceState::Playing;
        return true;
    }

    if (errorMessage) {
        *errorMessage = m_lastError;
    }
    return false;
}

bool LocalCameraSource::read(cv::Mat &frame)
{
    frame.release();
    if (m_state != VideoSourceState::Playing) {
        return false;
    }
    if (m_capture.read(frame) && !frame.empty()) {
        return true;
    }

    setError(QStringLiteral("本机摄像头读取中断：设备可能被占用或已断开"));
    return false;
}

void LocalCameraSource::close()
{
    const bool wasActive = m_capture.isOpened() || m_state == VideoSourceState::Playing;
    m_capture.release();
    m_cameraIndex = -1;
    if (wasActive) {
        m_state = VideoSourceState::Stopped;
    }
}

VideoSourceState LocalCameraSource::state() const
{
    return m_state;
}

QString LocalCameraSource::lastError() const
{
    return m_lastError;
}

QString LocalCameraSource::displayName() const
{
    return m_cameraIndex >= 0
            ? QStringLiteral("本机摄像头 #%1").arg(m_cameraIndex)
            : QString();
}

void LocalCameraSource::setError(const QString &message)
{
    m_capture.release();
    m_cameraIndex = -1;
    m_lastError = message;
    m_state = VideoSourceState::Error;
}
