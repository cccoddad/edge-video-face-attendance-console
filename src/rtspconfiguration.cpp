#include "rtspconfiguration.h"

#include <QUrl>

namespace {
const int kMinimumReconnectIntervalMilliseconds = 500;
const int kMaximumReconnectIntervalMilliseconds = 60000;
}

RtspConfiguration::RtspConfiguration(const QString &url, int reconnectIntervalMilliseconds)
    : mReconnectIntervalMilliseconds(3000)
{
    setReconnectIntervalMilliseconds(reconnectIntervalMilliseconds);
    if (!url.trimmed().isEmpty()) {
        setUrl(url);
    }
}

bool RtspConfiguration::setUrl(const QString &url, QString *errorMessage)
{
    const QString normalizedUrl = url.trimmed();
    if (!isValidUrl(normalizedUrl)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("RTSP 地址无效：必须包含 rtsp 协议和主机名");
        }
        return false;
    }
    mUrl = normalizedUrl;
    return true;
}

bool RtspConfiguration::setReconnectIntervalMilliseconds(int milliseconds, QString *errorMessage)
{
    if (milliseconds < kMinimumReconnectIntervalMilliseconds
            || milliseconds > kMaximumReconnectIntervalMilliseconds) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("重连等待时间必须在 %1 至 %2 毫秒之间")
                    .arg(kMinimumReconnectIntervalMilliseconds)
                    .arg(kMaximumReconnectIntervalMilliseconds);
        }
        return false;
    }
    mReconnectIntervalMilliseconds = milliseconds;
    return true;
}

QString RtspConfiguration::url() const
{
    return mUrl;
}

QString RtspConfiguration::displayName() const
{
    return QUrl(mUrl).toDisplayString(QUrl::RemoveUserInfo);
}

int RtspConfiguration::reconnectIntervalMilliseconds() const
{
    return mReconnectIntervalMilliseconds;
}

bool RtspConfiguration::isConfigured() const
{
    return !mUrl.isEmpty();
}

bool RtspConfiguration::isValidUrl(const QString &url)
{
    const QUrl parsedUrl(url.trimmed());
    return parsedUrl.isValid()
            && parsedUrl.scheme().compare(QStringLiteral("rtsp"), Qt::CaseInsensitive) == 0
            && !parsedUrl.host().isEmpty();
}
