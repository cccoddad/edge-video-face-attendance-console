#ifndef LOCALCAMERASOURCE_H
#define LOCALCAMERASOURCE_H

#include "ivideosource.h"

class LocalCameraSource : public IVideoSource
{
public:
    LocalCameraSource();
    ~LocalCameraSource() override;

    bool open(const QString &location, QString *errorMessage = nullptr) override;
    bool read(cv::Mat &frame) override;
    void close() override;
    VideoSourceState state() const override;
    QString lastError() const override;
    QString displayName() const override;

private:
    bool openCapture(int cameraIndex);
    bool reconnect();
    bool scheduleReconnect(const QString &reason);
    void setError(const QString &message);

    cv::VideoCapture m_capture;
    int m_cameraIndex;
    int m_consecutiveReadFailures;
    int m_consecutiveBlackFrames;
    int m_reconnectAttempts;
    qint64 m_reconnectAtMilliseconds;
    QString m_reconnectReason;
    QString m_lastError;
    VideoSourceState m_state;
};

#endif // LOCALCAMERASOURCE_H
