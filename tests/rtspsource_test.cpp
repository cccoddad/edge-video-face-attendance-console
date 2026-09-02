#include "../src/appconfig.h"
#include "../src/rtspconfiguration.h"
#include "../src/rtspsource.h"

#include <QCoreApplication>

#include <cstdio>

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

    std::fprintf(stdout, "RTSP source test passed: configuration, validation and local states verified\n");
    return 0;
}
