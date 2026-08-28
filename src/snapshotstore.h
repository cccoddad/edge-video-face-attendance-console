#ifndef SNAPSHOTSTORE_H
#define SNAPSHOTSTORE_H

#include <QDateTime>
#include <QString>

#include <opencv2/core.hpp>

class SnapshotStore
{
public:
    static bool save(const cv::Mat &frame, const QString &number, const QDateTime &timestamp,
                     const QString &eventKey, QString *snapshotPath,
                     QString *errorMessage = nullptr);
    static bool removeSnapshot(const QString &snapshotPath);
    static int removeExpired(int retentionDays, QString *errorMessage = nullptr);
};

#endif // SNAPSHOTSTORE_H
