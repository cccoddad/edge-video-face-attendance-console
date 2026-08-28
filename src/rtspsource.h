#ifndef RTSPSOURCE_H
#define RTSPSOURCE_H

#include "ivideosource.h"

#include <QDateTime>

class RtspSource : public IVideoSource
{
public:
    explicit RtspSource(int reconnectIntervalMilliseconds);
    ~RtspSource() override;

    bool open(const QString &location, QString *errorMessage = nullptr) override;
    bool read(cv::Mat &frame) override;
    void close() override;
    VideoSourceState state() const override;
    QString lastError() const override;
    QString displayName() const override;

    static bool isValidRtspUrl(const QString &location);

private:
    bool connectToStream(bool reconnecting, QString *errorMessage = nullptr);
    void setInterrupted(const QString &message);

    cv::VideoCapture m_capture;
    QString m_location;
    QString m_lastError;
    VideoSourceState m_state;
    QDateTime m_nextReconnectAt;
    int m_reconnectIntervalMilliseconds;
};

#endif // RTSPSOURCE_H
