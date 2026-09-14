#include "localcamerasource.h"

#include <QDateTime>

namespace {
const int kMaximumConsecutiveReadFailures = 15;
const int kMaximumConsecutiveBlackFrames = 25;
const int kMaximumReconnectAttempts = 3;
const int kReconnectDelayMilliseconds = 2000;
const double kBlackFrameMaximumChannelValue = 1.0;
}

LocalCameraSource::LocalCameraSource()
    : m_cameraIndex(-1)
    , m_consecutiveReadFailures(0)
    , m_consecutiveBlackFrames(0)
    , m_reconnectAttempts(0)
    , m_reconnectAtMilliseconds(0)
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
        setError(QStringLiteral("无法打开本机摄像头 #%1：设备不存在、仍在释放、被其他程序占用、"
                                "未授权 Windows 摄像头权限，或 OpenCV 采集后端不可用")
                 .arg(cameraIndex));
    } else {
        m_cameraIndex = cameraIndex;
        m_consecutiveReadFailures = 0;
        m_consecutiveBlackFrames = 0;
        m_reconnectAttempts = 0;
        m_reconnectAtMilliseconds = 0;
        m_reconnectReason.clear();
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
    if (m_state == VideoSourceState::Reconnecting) {
        if (QDateTime::currentMSecsSinceEpoch() < m_reconnectAtMilliseconds) {
            return false;
        }
        if (reconnect()) {
            return false;
        }
        if (scheduleReconnect(m_reconnectReason)) {
            return false;
        }
        setError(QStringLiteral("本机摄像头延迟重连 %1 次后仍未恢复（%2）："
                                "请检查摄像头是否在采集时从 Windows 设备列表消失，并更新或回退摄像头驱动")
                 .arg(m_reconnectAttempts)
                 .arg(m_reconnectReason));
        return false;
    }
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
                if (scheduleReconnect(QStringLiteral("连续返回全黑画面"))) {
                    return false;
                }
                setError(QStringLiteral("本机摄像头连续返回全黑画面，延迟重连 %1 次后仍未恢复："
                                        "请检查 Windows 摄像头驱动、物理隐私遮挡或摄像头热键")
                         .arg(m_reconnectAttempts));
            }
            return false;
        }
        m_consecutiveBlackFrames = 0;
        m_reconnectAttempts = 0;
        return true;
    }

    ++m_consecutiveReadFailures;
    if (m_consecutiveReadFailures < kMaximumConsecutiveReadFailures) {
        return false;
    }

    if (scheduleReconnect(QStringLiteral("连续读取失败"))) {
        return false;
    }
    setError(QStringLiteral("本机摄像头连续读取失败，延迟重连 %1 次后仍未恢复：设备可能已断开")
             .arg(m_reconnectAttempts));
    return false;
}

void LocalCameraSource::close()
{
    const bool wasActive = m_state != VideoSourceState::Closed
            && m_state != VideoSourceState::Stopped;
    m_capture.release();
    m_cameraIndex = -1;
    m_consecutiveReadFailures = 0;
    m_consecutiveBlackFrames = 0;
    m_reconnectAttempts = 0;
    m_reconnectAtMilliseconds = 0;
    m_reconnectReason.clear();
    m_lastError.clear();
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
    if (!openCapture(cameraIndex)) {
        return false;
    }

    m_consecutiveReadFailures = 0;
    m_consecutiveBlackFrames = 0;
    m_reconnectAtMilliseconds = 0;
    m_reconnectReason.clear();
    m_lastError.clear();
    m_state = VideoSourceState::Playing;
    return true;
}

bool LocalCameraSource::scheduleReconnect(const QString &reason)
{
    if (m_reconnectAttempts >= kMaximumReconnectAttempts) {
        return false;
    }

    m_capture.release();
    ++m_reconnectAttempts;
    m_reconnectReason = reason;
    m_reconnectAtMilliseconds = QDateTime::currentMSecsSinceEpoch()
            + static_cast<qint64>(kReconnectDelayMilliseconds) * m_reconnectAttempts;
    m_lastError = QStringLiteral("%1，等待第 %2/%3 次重连")
            .arg(reason)
            .arg(m_reconnectAttempts)
            .arg(kMaximumReconnectAttempts);
    m_state = VideoSourceState::Reconnecting;
    return true;
}

void LocalCameraSource::setError(const QString &message)
{
    m_capture.release();
    m_cameraIndex = -1;
    m_consecutiveReadFailures = 0;
    m_consecutiveBlackFrames = 0;
    m_reconnectAtMilliseconds = 0;
    m_reconnectReason.clear();
    m_lastError = message;
    m_state = VideoSourceState::Error;
}
