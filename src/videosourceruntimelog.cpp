#include "videosourceruntimelog.h"

VideoSourceRuntimeLog::VideoSourceRuntimeLog(int maximumEventCount)
    : mMaximumEventCount(qMax(1, maximumEventCount))
{
}

void VideoSourceRuntimeLog::record(const QString &sourceType, VideoSourceState state,
                                   const QString &detail, const QDateTime &occurredAt)
{
    VideoSourceRuntimeEvent event;
    event.occurredAt = occurredAt.isValid() ? occurredAt : QDateTime::currentDateTime();
    event.sourceType = sourceType;
    event.state = state;
    event.detail = detail;
    event.detail.replace('\r', QLatin1Char(' '));
    event.detail.replace('\n', QLatin1Char(' '));
    mEvents.append(event);
    while (mEvents.size() > mMaximumEventCount) {
        mEvents.removeFirst();
    }
}

QList<VideoSourceRuntimeEvent> VideoSourceRuntimeLog::events() const
{
    return mEvents;
}

QString VideoSourceRuntimeLog::sourceTypeText(const QString &sourceType)
{
    if (sourceType == QStringLiteral("video-file")) {
        return QStringLiteral("本地视频");
    }
    if (sourceType == QStringLiteral("local-camera")) {
        return QStringLiteral("本机摄像头");
    }
    if (sourceType == QStringLiteral("rtsp")) {
        return QStringLiteral("RTSP");
    }
    return sourceType.isEmpty() ? QStringLiteral("未知来源") : sourceType;
}

QString VideoSourceRuntimeLog::formatEvent(const VideoSourceRuntimeEvent &event)
{
    QString text = QStringLiteral("%1  %2  %3")
            .arg(event.occurredAt.toString(QStringLiteral("hh:mm:ss")))
            .arg(sourceTypeText(event.sourceType))
            .arg(IVideoSource::stateText(event.state));
    if (!event.detail.isEmpty()) {
        text.append(QStringLiteral("：%1").arg(event.detail));
    }
    return text;
}
