#ifndef VIDEOFILESOURCE_H
#define VIDEOFILESOURCE_H

#include "ivideosource.h"

class VideoFileSource : public IVideoSource
{
public:
    VideoFileSource();
    ~VideoFileSource() override;

    bool open(const QString &location, QString *errorMessage = nullptr) override;
    bool read(cv::Mat &frame) override;
    void close() override;
    VideoSourceState state() const override;
    QString lastError() const override;
    QString displayName() const override;

    void setLoopEnabled(bool enabled);
    bool isLoopEnabled() const;

private:
    void setError(const QString &message);

    cv::VideoCapture m_capture;
    QString m_filePath;
    QString m_lastError;
    VideoSourceState m_state;
    bool m_loopEnabled;
};

#endif // VIDEOFILESOURCE_H
