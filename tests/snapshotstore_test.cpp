#include "../src/appconfig.h"
#include "../src/snapshotstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) {
        std::fprintf(stderr, "Usage: SnapshotStoreTest <data-directory>\n");
        return 2;
    }

    const QString dataDirectory = application.arguments().at(1);
    qputenv("FACE_ATTENDANCE_DATA_DIR", dataDirectory.toUtf8());
    const QDateTime now = QDateTime::currentDateTime();
    const cv::Mat frame(16, 16, CV_8UC3, cv::Scalar(20, 40, 60));

    QString snapshotPath;
    QString errorMessage;
    if (!SnapshotStore::save(frame, "E001", now, "E001:20260828:签到", &snapshotPath, &errorMessage)) {
        std::fprintf(stderr, "Cannot save snapshot: %s\n", qPrintable(errorMessage));
        return 3;
    }
    if (!QFileInfo::exists(snapshotPath) || cv::imread(snapshotPath.toUtf8().constData()).empty()) {
        std::fprintf(stderr, "Saved snapshot was not readable\n");
        return 4;
    }

    const QString oldPath = QDir(AppConfig::snapshotDirectory()).filePath("old.jpg");
    if (!cv::imwrite(oldPath.toUtf8().constData(), frame)) {
        std::fprintf(stderr, "Cannot create expired snapshot fixture\n");
        return 5;
    }
    QFile oldFile(oldPath);
    if (!oldFile.open(QIODevice::ReadWrite)
            || !oldFile.setFileTime(now.addDays(-31), QFileDevice::FileModificationTime)) {
        std::fprintf(stderr, "Cannot set expired snapshot timestamp\n");
        return 6;
    }
    oldFile.close();

    const int removedCount = SnapshotStore::removeExpired(30, &errorMessage);
    if (removedCount != 1 || QFileInfo::exists(oldPath) || !QFileInfo::exists(snapshotPath)) {
        std::fprintf(stderr, "Expired snapshot cleanup did not preserve the current snapshot\n");
        return 7;
    }
    if (SnapshotStore::removeExpired(0, &errorMessage) != -1 || errorMessage.isEmpty()) {
        std::fprintf(stderr, "Invalid retention period was not rejected\n");
        return 8;
    }
    if (SnapshotStore::removeSnapshot(QDir(dataDirectory).filePath("not-a-snapshot.jpg"))) {
        std::fprintf(stderr, "Snapshot deletion accepted a path outside the snapshot directory\n");
        return 9;
    }

    std::fprintf(stdout, "Snapshot store test passed: save, cleanup, retention and path validation verified\n");
    return 0;
}
