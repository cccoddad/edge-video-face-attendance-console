#include "facerecognitionwin.h"
#include "appconfig.h"
#include "attendancerepository.h"
#include "snapshotstore.h"

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
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

    bool hasTestExitDelay = false;
    const int testExitDelayMs = qEnvironmentVariableIntValue("FACE_ATTENDANCE_TEST_EXIT_AFTER_MS",
                                                               &hasTestExitDelay);
    if (hasTestExitDelay && testExitDelayMs >= 0) {
        QTimer::singleShot(testExitDelayMs, &application, &QCoreApplication::quit);
    }
    return application.exec();
}
