#ifndef RTSPSCONFIGURATION_H
#define RTSPSCONFIGURATION_H

#include <QString>

class RtspConfiguration
{
public:
    explicit RtspConfiguration(const QString &url = QString(), int reconnectIntervalMilliseconds = 3000);

    bool setUrl(const QString &url, QString *errorMessage = nullptr);
    bool setReconnectIntervalMilliseconds(int milliseconds, QString *errorMessage = nullptr);

    QString url() const;
    QString displayName() const;
    int reconnectIntervalMilliseconds() const;
    bool isConfigured() const;

    static bool isValidUrl(const QString &url);

private:
    QString mUrl;
    int mReconnectIntervalMilliseconds;
};

#endif // RTSPSCONFIGURATION_H
