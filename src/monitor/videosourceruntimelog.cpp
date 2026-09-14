#include "videosourceruntimelog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

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

void VideoSourceRuntimeLog::clear()
{
    mEvents.clear();
}

bool VideoSourceRuntimeLog::saveToFile(const QString &filePath, QString *errorMessage) const
{
    if (filePath.isEmpty()) {
        return true;
    }
    const QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    for (const VideoSourceRuntimeEvent &event : mEvents) {
        stream << event.occurredAt.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))
               << QStringLiteral("|")
               << event.sourceType
               << QStringLiteral("|")
               << static_cast<int>(event.state)
               << QStringLiteral("|")
               << event.detail
               << QStringLiteral("\n");
    }
    return true;
}

bool VideoSourceRuntimeLog::loadFromFile(const QString &filePath, QString *errorMessage)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return true;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    mEvents.clear();
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() < 3) {
            continue;
        }
        VideoSourceRuntimeEvent event;
        event.occurredAt = QDateTime::fromString(parts[0], QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        event.sourceType = parts[1];
        event.state = static_cast<VideoSourceState>(parts[2].toInt());
        event.detail = parts.size() > 3 ? parts[3] : QString();
        if (event.occurredAt.isValid()) {
            mEvents.append(event);
        }
    }
    while (mEvents.size() > mMaximumEventCount) {
        mEvents.removeFirst();
    }
    return true;
}
