#include "facerecognitionwin.h"
#include "appconfig.h"
#include "attendancerepository.h"
#include "snapshotstore.h"
#include "theme.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

namespace {
void writeDatabaseAudit(const QSqlDatabase &database, const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(path);
    QDir().mkpath(fileInfo.absolutePath());
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "cannot write database audit:" << path << output.errorString();
        return;
    }

    int totalEvents = -1;
    int duplicateEventKeys = -1;
    int missingEventKeys = -1;
    QSqlQuery query(database);
    if (query.exec("SELECT COUNT(*) FROM recorduser") && query.next()) {
        totalEvents = query.value(0).toInt();
    }
    if (query.exec("SELECT COUNT(*) FROM (SELECT event_key FROM recorduser "
                   "WHERE event_key IS NOT NULL GROUP BY event_key HAVING COUNT(*) > 1)") && query.next()) {
        duplicateEventKeys = query.value(0).toInt();
    }
    if (query.exec("SELECT COUNT(*) FROM recorduser WHERE event_key IS NULL OR event_key = ''") && query.next()) {
        missingEventKeys = query.value(0).toInt();
    }

    output.write(QStringLiteral("total_events=%1\nduplicate_event_keys=%2\nmissing_event_keys=%3\n")
                 .arg(totalEvents)
                 .arg(duplicateEventKeys)
                 .arg(missingEventKeys)
                 .toUtf8());
}
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    Theme::apply(&application);
    qRegisterMetaType<cv::Mat>("cv::Mat");

    QString modelError;
    if (!AppConfig::hasRequiredModels(&modelError)) {
        QMessageBox::critical(nullptr, "模型配置错误", modelError);
        return -1;
    }

    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(AppConfig::databasePath());
    if (!database.open()) {
        qDebug() << database.lastError().text();
        return -1;
    }

    const QString userTableSql = "create table if not exists user (number varchar(32) primary key, "
                                 "name text, partment text,"
                                 "faceid int, "
                                 "facepictrue text,"
                                 "entertime text)";
    QSqlQuery query;
    if (!query.exec(userTableSql)) {
        qDebug() << query.lastError().text();
    }
    query.exec("PRAGMA foreign_keys = ON");

    const QString recordTableSql = "create table if not exists recorduser(id integer primary key autoincrement, "
                                   "number varchar(32), "
                                   "checktime text)";
    if (!query.exec(recordTableSql)) {
        qDebug() << query.lastError().text();
    }

    AttendanceRepository attendanceRepository(database);
    QString attendanceSchemaError;
    if (!attendanceRepository.ensureSchema(&attendanceSchemaError)) {
        QMessageBox::critical(nullptr, "考勤数据库错误", attendanceSchemaError);
        return -1;
    }

    QString snapshotCleanupError;
    const int removedSnapshots = SnapshotStore::removeExpired(AppConfig::snapshotRetentionDays(),
                                                               &snapshotCleanupError);
    if (removedSnapshots < 0 || !snapshotCleanupError.isEmpty()) {
        qWarning() << "snapshot cleanup failed:" << snapshotCleanupError;
    } else if (removedSnapshots > 0) {
        qInfo() << "removed expired snapshots:" << removedSnapshots;
    }

    FaceRecognitionWin window;
    window.show();

    const QString databaseAuditPath = qEnvironmentVariable("FACE_ATTENDANCE_DATABASE_AUDIT_PATH").trimmed();
    if (!databaseAuditPath.isEmpty()) {
        QObject::connect(&application, &QCoreApplication::aboutToQuit, [&database, databaseAuditPath]() {
            writeDatabaseAudit(database, databaseAuditPath);
        });
    }

    bool hasTestExitDelay = false;
    const int testExitDelayMs = qEnvironmentVariableIntValue("FACE_ATTENDANCE_TEST_EXIT_AFTER_MS",
                                                               &hasTestExitDelay);
    if (hasTestExitDelay && testExitDelayMs >= 0) {
        QTimer::singleShot(testExitDelayMs, &application, &QCoreApplication::quit);
    }
    return application.exec();
}
