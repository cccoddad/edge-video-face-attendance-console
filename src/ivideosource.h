#ifndef IVIDEOSOURCE_H
#define IVIDEOSOURCE_H

#include <QString>
#include <opencv.hpp>

enum class VideoSourceState
{
    Closed,
    Opening,
    Playing,
    Ended,
    Interrupted,
    Reconnecting,
    Error,
    Stopped
};

class IVideoSource
{
public:
    virtual ~IVideoSource() {}

    virtual bool open(const QString &location, QString *errorMessage = nullptr) = 0;
    virtual bool read(cv::Mat &frame) = 0;
    virtual void close() = 0;
    virtual VideoSourceState state() const = 0;
    virtual QString lastError() const = 0;
    virtual QString displayName() const = 0;

    static QString stateText(VideoSourceState state);
};

#endif // IVIDEOSOURCE_H
