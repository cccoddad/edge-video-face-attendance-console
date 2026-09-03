#include "../src/appconfig.h"
#include "../src/rtspconfiguration.h"
#include "../src/rtspreconnectscheduler.h"
#include "../src/rtspsource.h"

#include <QCoreApplication>

#include <cstdio>

class FakeRtspSource : public RtspSource
{
public:
    explicit FakeRtspSource(const QList<bool> &openResults, const QList<bool> &readResults)
        : RtspSource(0)
        , m_openResults(openResults)
        , m_readResults(readResults)
    {
    }

protected:
    bool openCapture(const QString &) override
    {
        return takeResult(m_openResults, m_openIndex);
    }

    bool readCapture(cv::Mat &frame) override
    {
        if (!takeResult(m_readResults, m_readIndex)) {
            return false;
        }
        frame = cv::Mat::ones(2, 2, CV_8UC3);
        return true;
    }

    bool captureIsOpen() const override
    {
        return m_isOpen;
    }

    void releaseCapture() override
    {
        m_isOpen = false;
    }

private:
    bool takeResult(const QList<bool> &results, int &index)
    {
        const bool result = index < results.size() && results.at(index++);
        m_isOpen = result;
        return result;
    }

    QList<bool> m_openResults;
    QList<bool> m_readResults;
    int m_openIndex{0};
    int m_readIndex{0};
    bool m_isOpen{false};
};

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    qputenv("FACE_ATTENDANCE_RTSP_URL", "rtsp://example.invalid:8554/live");
    qputenv("FACE_ATTENDANCE_RTSP_RECONNECT_INTERVAL_MS", "1500");
    if (AppConfig::rtspUrl() != QStringLiteral("rtsp://example.invalid:8554/live")
            || AppConfig::rtspReconnectIntervalMilliseconds() != 1500) {
        std::fprintf(stderr, "RTSP configuration was not read correctly\n");
        return 2;
    }
    if (!RtspSource::isValidRtspUrl(QStringLiteral("rtsp://example.invalid:8554/live"))
            || RtspSource::isValidRtspUrl(QStringLiteral("http://example.invalid/live"))
            || RtspSource::isValidRtspUrl(QStringLiteral("rtsp:///live"))) {
        std::fprintf(stderr, "RTSP URL validation returned an unexpected result\n");
        return 3;
    }
    RtspConfiguration configuration;
    if (!configuration.setUrl(QStringLiteral("  rtsp://user:secret@example.invalid:8554/live  "))
            || !configuration.isConfigured()
            || configuration.url() != QStringLiteral("rtsp://user:secret@example.invalid:8554/live")
            || configuration.displayName().contains(QStringLiteral("user"))
            || configuration.displayName().contains(QStringLiteral("secret"))) {
        std::fprintf(stderr, "RTSP configuration normalization or redaction failed\n");
        return 6;
    }
    QString configurationError;
    if (configuration.setUrl(QStringLiteral("https://example.invalid/live"), &configurationError)
            || configurationError.isEmpty()
            || configuration.setReconnectIntervalMilliseconds(499, &configurationError)
            || configurationError.isEmpty()
            || !configuration.setReconnectIntervalMilliseconds(1500)
            || configuration.reconnectIntervalMilliseconds() != 1500) {
        std::fprintf(stderr, "RTSP configuration validation failed\n");
        return 7;
    }
    const QDateTime reconnectStart(QDate(2026, 9, 2), QTime(9, 0, 0), Qt::UTC);
    RtspReconnectScheduler scheduler(1500);
    if (scheduler.isScheduled() || scheduler.isDue(reconnectStart)) {
        std::fprintf(stderr, "RTSP reconnect scheduler started in an unexpected state\n");
        return 8;
    }
    scheduler.schedule(reconnectStart);
    if (!scheduler.isScheduled()
            || scheduler.nextAttemptAt() != reconnectStart.addMSecs(1500)
            || scheduler.isDue(reconnectStart.addMSecs(1499))
            || !scheduler.isDue(reconnectStart.addMSecs(1500))) {
        std::fprintf(stderr, "RTSP reconnect scheduler did not honor the wait boundary\n");
        return 9;
    }
    scheduler.clear();
    if (scheduler.isScheduled() || scheduler.isDue(reconnectStart.addMSecs(2000))) {
        std::fprintf(stderr, "RTSP reconnect scheduler did not clear its pending attempt\n");
        return 10;
    }

    RtspSource source(AppConfig::rtspReconnectIntervalMilliseconds());
    QString errorMessage;
    if (source.open(QStringLiteral("http://example.invalid/live"), &errorMessage)
            || source.state() != VideoSourceState::Error || errorMessage.isEmpty()) {
        std::fprintf(stderr, "Invalid RTSP URL did not enter the error state\n");
        return 4;
    }
    if (IVideoSource::stateText(VideoSourceState::Interrupted).isEmpty()
            || IVideoSource::stateText(VideoSourceState::Reconnecting).isEmpty()) {
        std::fprintf(stderr, "RTSP interruption states have no visible text\n");
        return 5;
    }
    if (!IVideoSource::shouldKeepPolling(VideoSourceState::Playing)
            || !IVideoSource::shouldKeepPolling(VideoSourceState::Interrupted)
            || !IVideoSource::shouldKeepPolling(VideoSourceState::Reconnecting)
            || IVideoSource::shouldKeepPolling(VideoSourceState::Closed)
            || IVideoSource::shouldKeepPolling(VideoSourceState::Ended)
            || IVideoSource::shouldKeepPolling(VideoSourceState::Error)
            || IVideoSource::shouldKeepPolling(VideoSourceState::Stopped)) {
        std::fprintf(stderr, "Video source polling policy would stop RTSP recovery\n");
        return 11;
    }
    FakeRtspSource recoveringSource(QList<bool>() << true << false << true,
                                    QList<bool>() << true << false << true);
    if (!recoveringSource.open(QStringLiteral("rtsp://example.invalid:8554/live"))
            || recoveringSource.state() != VideoSourceState::Playing) {
        std::fprintf(stderr, "Fake RTSP source did not enter the playing state\n");
        return 12;
    }
    cv::Mat frame;
    if (!recoveringSource.read(frame) || frame.empty()) {
        std::fprintf(stderr, "Fake RTSP source did not read the initial frame\n");
        return 13;
    }
    if (recoveringSource.read(frame)
            || recoveringSource.state() != VideoSourceState::Interrupted
            || recoveringSource.lastError().isEmpty()) {
        std::fprintf(stderr, "RTSP read interruption did not enter the recoverable state\n");
        return 14;
    }
    if (recoveringSource.read(frame)
            || recoveringSource.state() != VideoSourceState::Interrupted) {
        std::fprintf(stderr, "RTSP reconnect failure did not remain recoverable\n");
        return 15;
    }
    if (!recoveringSource.read(frame)
            || recoveringSource.state() != VideoSourceState::Playing
            || frame.empty()) {
        std::fprintf(stderr, "RTSP reconnect did not recover the next frame\n");
        return 16;
    }

    std::fprintf(stdout, "RTSP source test passed: configuration, validation and local states verified\n");
    return 0;
}
