#include "../src/storage/attendancewriter.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <cstdio>
#include <opencv2/core.hpp>

Q_DECLARE_METATYPE(cv::Mat)

static int s_passCount = 0;
static int s_failCount = 0;

static void expect(bool cond, const char *label)
{
    if (cond) {
        ++s_passCount;
    } else {
        ++s_failCount;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

static cv::Mat makeFrame()
{
    cv::Mat frame(80, 80, CV_8UC3, cv::Scalar(120, 160, 200));
    return frame;
}

static AttendanceConfirmation makeConfirmation(const QString &number, const QDateTime &timestamp)
{
    AttendanceConfirmation confirmation;
    confirmation.number = number;
    confirmation.similarity = 0.93f;
    confirmation.timestamp = timestamp;
    return confirmation;
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<AttendanceConfirmation>("AttendanceConfirmation");
    qRegisterMetaType<AttendanceWriteResult>("AttendanceWriteResult");

    QTemporaryDir dataDir;
    if (!dataDir.isValid()) {
        std::fprintf(stderr, "Cannot create temporary data directory\n");
        return 2;
    }
    qputenv("FACE_ATTENDANCE_DATA_DIR", dataDir.path().toUtf8());

    // 模拟 main.cpp 启动时创建的基础表
    {
        QSqlDatabase setupDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                         QStringLiteral("writer-test-setup"));
        setupDb.setDatabaseName(QDir(dataDir.path()).filePath("attendance.db"));
        if (!setupDb.open()) {
            std::fprintf(stderr, "Cannot prepare base tables\n");
            return 2;
        }
        QSqlQuery setupQuery(setupDb);
        setupQuery.exec("CREATE TABLE IF NOT EXISTS user(number varchar(32) primary key, name text, "
                        "partment text, faceid int, facepictrue text, entertime text)");
        setupQuery.exec("CREATE TABLE IF NOT EXISTS recorduser(id integer primary key autoincrement, "
                        "number varchar(32), checktime text)");
        setupDb.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("writer-test-setup"));

    QThread thread;
    AttendanceWriter writer;
    writer.moveToThread(&thread);
    thread.start();

    auto shutdown = [&writer, &thread]() {
        QMetaObject::invokeMethod(&writer, "shutdown", Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait(3000);
    };

    bool readyOk = false;
    QString readyError;
    QObject::connect(&writer, &AttendanceWriter::ready, &application,
                     [&](bool success, const QString &errorMessage) {
        readyOk = success;
        readyError = errorMessage;
    });
    QMetaObject::invokeMethod(&writer, "initialize", Qt::BlockingQueuedConnection);
    QCoreApplication::processEvents();
    expect(readyOk, "writer initialized with independent connection");
    if (!readyOk) {
        std::fprintf(stderr, "writer init error: %s\n", qPrintable(readyError));
        shutdown();
        return 3;
    }

    AttendanceWriteResult lastResult;
    AttendanceConfirmation lastConfirmation;
    bool hasResult = false;
    QObject::connect(&writer, &AttendanceWriter::writeFinished, &application,
                     [&](const AttendanceWriteResult &result,
                         const AttendanceConfirmation &confirmation, quint64) {
        lastResult = result;
        lastConfirmation = confirmation;
        hasResult = true;
    });

    const QDateTime dayStart(QDate(2026, 9, 12), QTime(8, 0, 0));
    auto waitForResult = [&]() {
        hasResult = false;
        QEventLoop loop;
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        QMetaObject::Connection connection = QObject::connect(
                    &writer, &AttendanceWriter::writeFinished, &application,
                    [&loop](const AttendanceWriteResult &, const AttendanceConfirmation &, quint64) {
            loop.quit();
        });
        loop.exec();
        QObject::disconnect(connection);
        return hasResult;
    };

    std::fprintf(stdout, "Test: record writes check-in with snapshot\n");
    QMetaObject::invokeMethod(&writer, "record", Qt::QueuedConnection,
                              Q_ARG(AttendanceConfirmation, makeConfirmation("E001", dayStart)),
                              Q_ARG(int, 3600),
                              Q_ARG(QString, QStringLiteral("test")),
                              Q_ARG(cv::Mat, makeFrame()),
                              Q_ARG(quint64, quint64(1)));
    expect(waitForResult(), "write result received");
    expect(lastResult.status == AttendanceWriteStatus::Inserted, "check-in inserted");
    expect(lastResult.eventKey == QStringLiteral("4:E001:20260912:签到"), "event key format");

    QString snapshotPath;
    {
        QSqlDatabase checkDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                         QStringLiteral("writer-test-check"));
        checkDb.setDatabaseName(QDir(dataDir.path()).filePath("attendance.db"));
        if (!checkDb.open()) {
            std::fprintf(stderr, "Cannot open attendance db for verification\n");
            shutdown();
            return 4;
        }
        QSqlQuery query(checkDb);
        query.prepare("SELECT checktime, snapshot_path FROM recorduser WHERE event_key = ?");
        query.addBindValue(lastResult.eventKey);
        expect(query.exec() && query.next(), "row found in database");
        expect(query.value(0).toString() == "2026-09-12 08:00:00", "checktime stored");
        snapshotPath = query.value(1).toString();
        checkDb.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("writer-test-check"));
    expect(!snapshotPath.isEmpty(), "snapshot path recorded");
    expect(QFileInfo::exists(snapshotPath), "snapshot file exists on disk");

    std::fprintf(stdout, "Test: duplicate check-in suppressed inside interval\n");
    QMetaObject::invokeMethod(&writer, "record", Qt::QueuedConnection,
                              Q_ARG(AttendanceConfirmation,
                                    makeConfirmation("E001", dayStart.addSecs(60))),
                              Q_ARG(int, 3600),
                              Q_ARG(QString, QStringLiteral("test")),
                              Q_ARG(cv::Mat, makeFrame()),
                              Q_ARG(quint64, quint64(2)));
    expect(waitForResult(), "second write result received");
    expect(lastResult.status == AttendanceWriteStatus::Suppressed, "duplicate suppressed");

    std::fprintf(stdout, "Test: explicit check-out inserted after interval\n");
    QMetaObject::invokeMethod(&writer, "recordCheckOut", Qt::QueuedConnection,
                              Q_ARG(AttendanceConfirmation,
                                    makeConfirmation("E001", dayStart.addSecs(3600))),
                              Q_ARG(QString, QStringLiteral("test")),
                              Q_ARG(cv::Mat, makeFrame()),
                              Q_ARG(quint64, quint64(3)));
    expect(waitForResult(), "check-out result received");
    expect(lastResult.status == AttendanceWriteStatus::Inserted, "check-out inserted");
    expect(lastResult.eventKey == QStringLiteral("4:E001:20260912:签退"), "check-out event key");

    std::fprintf(stdout, "Test: record after check-out suppressed\n");
    QMetaObject::invokeMethod(&writer, "record", Qt::QueuedConnection,
                              Q_ARG(AttendanceConfirmation,
                                    makeConfirmation("E001", dayStart.addSecs(7200))),
                              Q_ARG(int, 0),
                              Q_ARG(QString, QStringLiteral("test")),
                              Q_ARG(cv::Mat, makeFrame()),
                              Q_ARG(quint64, quint64(4)));
    expect(waitForResult(), "fourth write result received");
    expect(lastResult.status == AttendanceWriteStatus::Suppressed, "post-checkout suppressed");

    shutdown();

    std::fprintf(stdout, "\nAttendanceWriterTest: %d passed, %d failed\n",
                 s_passCount, s_failCount);
    return s_failCount > 0 ? 1 : 0;
}
