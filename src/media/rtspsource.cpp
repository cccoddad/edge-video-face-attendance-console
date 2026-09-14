#include "rtspsource.h"
#include "appconfig.h"
#include "rtspconfiguration.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QProcess>

namespace {
const int kOutputWidth = 640;
const int kOutputHeight = 480;
const int kOutputFrameBytes = kOutputWidth * kOutputHeight * 3;
const int kMaxBufferedFrames = 3;
const int kStartTimeoutMilliseconds = 8000;
const int kFirstFrameTimeoutMilliseconds = 6000;
const qint64 kActivityTimeoutMilliseconds = 10000;

QStringList buildCaptureArguments(const QString &location)
{
    const QString scaleFilter = QStringLiteral(
                "scale=%1:%2:force_original_aspect_ratio=decrease,"
                "pad=%1:%2:(ow-iw)/2:(oh-ih)/2")
            .arg(kOutputWidth)
            .arg(kOutputHeight);
    QStringList arguments;
    arguments << QStringLiteral("-hide_banner")
              << QStringLiteral("-loglevel") << QStringLiteral("warning")
              << QStringLiteral("-rtsp_transport") << QStringLiteral("tcp")
              << QStringLiteral("-fflags") << QStringLiteral("nobuffer")
              << QStringLiteral("-flags") << QStringLiteral("low_delay")
              << QStringLiteral("-i") << location
              << QStringLiteral("-an")
              << QStringLiteral("-sn")
              << QStringLiteral("-vf") << scaleFilter
              << QStringLiteral("-f") << QStringLiteral("rawvideo")
              << QStringLiteral("-pix_fmt") << QStringLiteral("bgr24")
              << QStringLiteral("-");
    return arguments;
}
}

RtspSource::RtspSource(int reconnectIntervalMilliseconds)
    : m_process(nullptr)
    , m_state(VideoSourceState::Closed)
    , mReconnectScheduler(reconnectIntervalMilliseconds)
    , m_lastActivityMsecs(0)
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

    // FFmpeg 子进程异步输出：单次无帧不代表断流，仅在进程退出或长时间无数据后判定中断
    if (captureIsOpen() && m_lastActivityMsecs > 0
            && QDateTime::currentMSecsSinceEpoch() - m_lastActivityMsecs
                    > kActivityTimeoutMilliseconds) {
        releaseCapture();
    }
    if (!captureIsOpen()) {
        setInterrupted(QStringLiteral("RTSP 视频读取中断，等待自动重连"));
    }
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
    releaseCapture();
    const QString ffmpegPath = AppConfig::ffmpegPath();
    if (ffmpegPath.isEmpty()) {
        m_lastError = QStringLiteral("未配置 ffmpeg 可执行文件路径");
        return false;
    }

    m_process = new QProcess;
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->start(ffmpegPath, buildCaptureArguments(location));
    m_lastActivityMsecs = QDateTime::currentMSecsSinceEpoch();
    if (!m_process->waitForStarted(kStartTimeoutMilliseconds)) {
        m_lastError = QStringLiteral("无法启动 FFmpeg RTSP 解码进程：%1")
                .arg(m_process->errorString());
        releaseCapture();
        return false;
    }

    // 等待首个原始视频数据到达，避免"进程已启动但连接失败"被误判为成功
    QElapsedTimer firstFrameTimer;
    firstFrameTimer.start();
    while (firstFrameTimer.elapsed() < kFirstFrameTimeoutMilliseconds) {
        if (m_process->state() != QProcess::Running) {
            const QByteArray errors = m_process->readAllStandardError().trimmed();
            QString detail = QString::fromUtf8(errors);
            const int lastNewLine = detail.lastIndexOf(QLatin1Char('\n'));
            if (lastNewLine >= 0) {
                detail = detail.mid(lastNewLine + 1).trimmed();
            }
            m_lastError = detail.isEmpty()
                    ? QStringLiteral("FFmpeg RTSP 解码进程提前退出")
                    : QStringLiteral("FFmpeg RTSP 连接失败：%1").arg(detail);
            releaseCapture();
            return false;
        }
        m_process->waitForReadyRead(200);
        if (m_process->bytesAvailable() > 0) {
            return true;
        }
    }

    m_lastError = QStringLiteral("等待 RTSP 首帧超时");
    releaseCapture();
    return false;
}

bool RtspSource::readCapture(cv::Mat &frame)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        return false;
    }

    const QByteArray errors = m_process->readAllStandardError();
    const QByteArray output = m_process->readAllStandardOutput();
    if (!errors.isEmpty() || !output.isEmpty()) {
        m_lastActivityMsecs = QDateTime::currentMSecsSinceEpoch();
    }
    if (!errors.isEmpty()) {
        QByteArray tail = errors;
        const int lastNewLine = tail.lastIndexOf('\n');
        if (lastNewLine >= 0) {
            tail = tail.mid(lastNewLine + 1);
        }
        m_processLog = tail.trimmed();
        if (m_processLog.size() > 300) {
            m_processLog = m_processLog.right(300);
        }
    }
    if (!output.isEmpty()) {
        m_frameBuffer.append(output);
        while (m_frameBuffer.size() >= kOutputFrameBytes * (kMaxBufferedFrames + 1)) {
            m_frameBuffer.remove(0, kOutputFrameBytes);
        }
    }
    if (m_frameBuffer.size() < kOutputFrameBytes) {
        return false;
    }

    frame = cv::Mat(kOutputHeight, kOutputWidth, CV_8UC3, m_frameBuffer.data()).clone();
    m_frameBuffer.remove(0, kOutputFrameBytes);
    return true;
}

bool RtspSource::captureIsOpen() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void RtspSource::releaseCapture()
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(3000);
        }
        delete m_process;
        m_process = nullptr;
    }
    m_frameBuffer.clear();
    m_lastActivityMsecs = 0;
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

    QString detail = m_lastError;
    if (detail.isEmpty()) {
        detail = QString::fromUtf8(m_processLog);
    }
    const QString message = reconnecting
            ? (detail.isEmpty() ? QStringLiteral("RTSP 重连失败，将继续等待")
                                : QStringLiteral("RTSP 重连失败，将继续等待：%1").arg(detail))
            : (detail.isEmpty() ? QStringLiteral("无法使用 FFmpeg 连接 RTSP 视频源")
                                : QStringLiteral("无法使用 FFmpeg 连接 RTSP 视频源：%1").arg(detail));
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
