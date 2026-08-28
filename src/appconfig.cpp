#include "appconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace {
QString configuredPath(const char *name, const QString &fallback)
{
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? fallback : QDir::cleanPath(value);
}
}

QString AppConfig::dataDirectory()
{
    const QString fallback = QDir::home().filePath("FaceAttendance/data");
    const QString path = configuredPath("FACE_ATTENDANCE_DATA_DIR", fallback);
    QDir().mkpath(path);
    return path;
}

QString AppConfig::photoDirectory()
{
    const QString path = QDir(dataDirectory()).filePath("photos");
    QDir().mkpath(path);
    return path;
}

QString AppConfig::snapshotDirectory()
{
    const QString path = QDir(dataDirectory()).filePath("snapshots");
    QDir().mkpath(path);
    return path;
}

QString AppConfig::databasePath()
{
    return QDir(dataDirectory()).filePath("attendance.db");
}

QString AppConfig::faceDatabasePath()
{
    return QDir(dataDirectory()).filePath("faces.db");
}

QString AppConfig::modelDirectory()
{
    const QString fallback = QDir(QCoreApplication::applicationDirPath()).filePath("models");
    return configuredPath("FACE_ATTENDANCE_MODEL_DIR", fallback);
}

QString AppConfig::modelPath(const QString &fileName)
{
    return QDir(modelDirectory()).filePath(fileName);
}

float AppConfig::recognitionThreshold()
{
    bool ok = false;
    const float value = qEnvironmentVariable("FACE_ATTENDANCE_SIMILARITY_THRESHOLD").toFloat(&ok);
    return ok && value > 0.0f && value < 1.0f ? value : 0.70f;
}

int AppConfig::recognitionConfirmationFrames()
{
    bool ok = false;
    const int value = qEnvironmentVariable("FACE_ATTENDANCE_CONFIRMATION_FRAMES").toInt(&ok);
    return ok && value >= 1 && value <= 10 ? value : 3;
}

int AppConfig::attendanceCooldownSeconds()
{
    bool ok = false;
    const int value = qEnvironmentVariable("FACE_ATTENDANCE_COOLDOWN_SECONDS").toInt(&ok);
    return ok && value >= 0 ? value : 30;
}

int AppConfig::minimumCheckoutIntervalSeconds()
{
    bool ok = false;
    const int value = qEnvironmentVariable("FACE_ATTENDANCE_MINIMUM_CHECKOUT_INTERVAL_SECONDS").toInt(&ok);
    return ok && value >= 0 ? value : 4 * 60 * 60;
}

int AppConfig::snapshotRetentionDays()
{
    bool ok = false;
    const int value = qEnvironmentVariable("FACE_ATTENDANCE_SNAPSHOT_RETENTION_DAYS").toInt(&ok);
    return ok && value >= 1 && value <= 3650 ? value : 30;
}

QString AppConfig::rtspUrl()
{
    return qEnvironmentVariable("FACE_ATTENDANCE_RTSP_URL").trimmed();
}

int AppConfig::rtspReconnectIntervalMilliseconds()
{
    bool ok = false;
    const int value = qEnvironmentVariable("FACE_ATTENDANCE_RTSP_RECONNECT_INTERVAL_MS").toInt(&ok);
    return ok && value >= 500 && value <= 60000 ? value : 3000;
}

bool AppConfig::localVideoLoopEnabled()
{
    const QString value = qEnvironmentVariable("FACE_ATTENDANCE_LOCAL_VIDEO_LOOP").trimmed();
    return value == QStringLiteral("1") || value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

int AppConfig::localCameraIndex()
{
    bool ok = false;
    const int value = qEnvironmentVariable("FACE_ATTENDANCE_LOCAL_CAMERA_INDEX").toInt(&ok);
    return ok && value >= 0 && value <= 15 ? value : 0;
}

bool AppConfig::hasRequiredModels(QString *errorMessage)
{
    const QStringList requiredModels = {
        "fd_2_00.dat",
        "pd_2_00_pts5.dat",
        "fr_2_10.dat"
    };

    QStringList missingModels;
    for (const QString &model : requiredModels) {
        if (!QFileInfo::exists(modelPath(model))) {
            missingModels.append(model);
        }
    }

    if (!missingModels.isEmpty() && errorMessage) {
        *errorMessage = QString("未找到模型文件：%1\n模型目录：%2\n"
                                "可通过 FACE_ATTENDANCE_MODEL_DIR 指定模型目录。")
                            .arg(missingModels.join(", "), modelDirectory());
    }
    return missingModels.isEmpty();
}
