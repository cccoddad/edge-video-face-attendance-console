#include "localcamerasource.h"

#include <QThread>

namespace {
const int kMaximumConsecutiveReadFailures = 15;
const int kMaximumConsecutiveBlackFrames = 25;
const int kMaximumBlackFrameReconnectAttempts = 2;
const double kBlackFrameMaximumChannelValue = 1.0;
}

LocalCameraSource::LocalCameraSource()
    : m_cameraIndex(-1)
    , m_consecutiveReadFailures(0)
    , m_consecutiveBlackFrames(0)
    , m_blackFrameReconnectAttempts(0)
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
    } else if (!openCapture(cameraIndex)) {
        setError(QStringLiteral("无法打开本机摄像头 #%1：设备不存在、被占用或未授权 Windows 摄像头权限")
                 .arg(cameraIndex));
    } else {
        m_cameraIndex = cameraIndex;
        m_consecutiveReadFailures = 0;
        m_consecutiveBlackFrames = 0;
        m_blackFrameReconnectAttempts = 0;
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
        m_consecutiveReadFailures = 0;
        const cv::Scalar brightness = cv::mean(frame);
        const bool essentiallyBlack = brightness[0] < kBlackFrameMaximumChannelValue
                && brightness[1] < kBlackFrameMaximumChannelValue
                && brightness[2] < kBlackFrameMaximumChannelValue;
        if (essentiallyBlack) {
            ++m_consecutiveBlackFrames;
            if (m_consecutiveBlackFrames >= kMaximumConsecutiveBlackFrames) {
                if (m_blackFrameReconnectAttempts < kMaximumBlackFrameReconnectAttempts) {
                    ++m_blackFrameReconnectAttempts;
                    if (reconnect()) {
                        return false;
                    }
                }
                setError(QStringLiteral("本机摄像头连续返回全黑画面，自动重连 %1 次后仍未恢复："
                                        "请检查物理隐私遮挡、摄像头热键或彩色摄像头驱动")
                         .arg(m_blackFrameReconnectAttempts));
            }
            return false;
        }
        m_consecutiveBlackFrames = 0;
        m_blackFrameReconnectAttempts = 0;
        return true;
    }

    ++m_consecutiveReadFailures;
    if (m_consecutiveReadFailures < kMaximumConsecutiveReadFailures) {
        return false;
    }

    if (reconnect()) {
        return false;
    }
    setError(QStringLiteral("本机摄像头读取中断：设备可能被占用或已断开"));
    return false;
}

void LocalCameraSource::close()
{
    const bool wasActive = m_capture.isOpened() || m_state == VideoSourceState::Playing;
    m_capture.release();
    m_cameraIndex = -1;
    m_consecutiveReadFailures = 0;
    m_consecutiveBlackFrames = 0;
    m_blackFrameReconnectAttempts = 0;
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

bool LocalCameraSource::openCapture(int cameraIndex)
{
    if (m_capture.open(cameraIndex, cv::CAP_DSHOW)) {
        return true;
    }
    m_capture.release();
    return m_capture.open(cameraIndex, cv::CAP_MSMF);
}

bool LocalCameraSource::reconnect()
{
    const int cameraIndex = m_cameraIndex;
    m_capture.release();
    QThread::msleep(200);
    if (!openCapture(cameraIndex)) {
        return false;
    }

    m_consecutiveReadFailures = 0;
    m_consecutiveBlackFrames = 0;
    return true;
}

void LocalCameraSource::setError(const QString &message)
{
    m_capture.release();
    m_cameraIndex = -1;
    m_consecutiveReadFailures = 0;
    m_consecutiveBlackFrames = 0;
    m_blackFrameReconnectAttempts = 0;
    m_lastError = message;
    m_state = VideoSourceState::Error;
}
