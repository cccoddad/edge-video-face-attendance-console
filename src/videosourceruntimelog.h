#ifndef VIDEOSOURCERUNTIMELOG_H
#define VIDEOSOURCERUNTIMELOG_H

#include "ivideosource.h"

#include <QDateTime>
#include <QList>

struct VideoSourceRuntimeEvent
{
    QDateTime occurredAt;
    QString sourceType;
    VideoSourceState state;
    QString detail;
};

class VideoSourceRuntimeLog
{
public:
    explicit VideoSourceRuntimeLog(int maximumEventCount = 50);

    void record(const QString &sourceType, VideoSourceState state, const QString &detail = QString(),
                const QDateTime &occurredAt = QDateTime::currentDateTime());
    QList<VideoSourceRuntimeEvent> events() const;

    static QString sourceTypeText(const QString &sourceType);
    static QString formatEvent(const VideoSourceRuntimeEvent &event);

private:
    int mMaximumEventCount;
    QList<VideoSourceRuntimeEvent> mEvents;
};

#endif // VIDEOSOURCERUNTIMELOG_H
