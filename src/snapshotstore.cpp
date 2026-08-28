#include "snapshotstore.h"

#include "appconfig.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>

bool SnapshotStore::save(const cv::Mat &frame, const QString &number, const QDateTime &timestamp,
                         const QString &eventKey, QString *snapshotPath, QString *errorMessage)
{
    if (frame.empty() || number.isEmpty() || !timestamp.isValid() || eventKey.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("抓拍帧或考勤事件信息无效");
        }
        return false;
    }

    const QString dateDirectory = QDir(AppConfig::snapshotDirectory())
            .filePath(timestamp.date().toString("yyyy-MM-dd"));
    if (!QDir().mkpath(dateDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建抓拍目录：%1").arg(dateDirectory);
        }
        return false;
    }

    const QByteArray digest = QCryptographicHash::hash(
                QStringLiteral("%1|%2|%3").arg(number, timestamp.toString(Qt::ISODate), eventKey)
                .toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    const QString fileName = QStringLiteral("%1_%2.jpg")
            .arg(timestamp.toString("yyyyMMdd_hhmmss_zzz"), QString::fromLatin1(digest));
    const QString path = QDir(dateDirectory).filePath(fileName);
    if (!cv::imwrite(path.toUtf8().constData(), frame)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写入抓拍图片失败：%1").arg(path);
        }
        return false;
    }

    if (snapshotPath) {
        *snapshotPath = path;
    }
    return true;
}

bool SnapshotStore::removeSnapshot(const QString &snapshotPath)
{
    if (snapshotPath.isEmpty()) {
        return false;
    }

    const QString rootPath = QDir::cleanPath(QFileInfo(AppConfig::snapshotDirectory()).absoluteFilePath());
    const QString absolutePath = QDir::cleanPath(QFileInfo(snapshotPath).absoluteFilePath());
    const QString rootPrefix = rootPath + QDir::separator();
    if (!absolutePath.startsWith(rootPrefix, Qt::CaseInsensitive)) {
        return false;
    }
    return QFile::remove(absolutePath);
}

int SnapshotStore::removeExpired(int retentionDays, QString *errorMessage)
{
    if (retentionDays < 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("抓拍保留期必须至少为 1 天");
        }
        return -1;
    }

    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-retentionDays);
    QDirIterator iterator(AppConfig::snapshotDirectory(), QStringList() << "*.jpg",
                         QDir::Files, QDirIterator::Subdirectories);
    int removedCount = 0;
    QStringList failedPaths;
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (QFileInfo(path).lastModified() < cutoff) {
            if (QFile::remove(path)) {
                ++removedCount;
            } else {
                failedPaths.append(path);
            }
        }
    }
    if (!failedPaths.isEmpty() && errorMessage) {
        *errorMessage = QStringLiteral("无法清理过期抓拍：%1").arg(failedPaths.join(", "));
    }
    return removedCount;
}
