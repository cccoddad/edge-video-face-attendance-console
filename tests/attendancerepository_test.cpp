#include "../src/attendancerepository.h"
#include "../src/attendancestatemachine.h"
#include "../src/databasemigration.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QVariant>
#include <cstdio>

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

static AttendanceConfirmation makeConfirmation(const QString &number,
                                               float similarity,
                                               const QDateTime &timestamp)
{
    AttendanceConfirmation c;
    c.number = number;
    c.similarity = similarity;
    c.timestamp = timestamp;
    return c;
}

static QSqlDatabase openTestDb(const QString &name)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(":memory:");
    db.open();
    QSqlQuery create(db);
    create.exec("CREATE TABLE user(number varchar(32) primary key, name text, "
                "partment text, faceid int, facepictrue text, entertime text)");
    create.exec("CREATE TABLE recorduser(id integer primary key autoincrement, "
                "number varchar(32), checktime text)");
    return db;
}

static int testSchemaVersion()
{
    std::fprintf(stdout, "Test: schema version is 1 after ensureSchema\n");
    QSqlDatabase db = openTestDb("schema-ver-test");
    AttendanceRepository repo(db);

    QString error;
    bool ok = repo.ensureSchema(&error);
    expect(ok, "ensureSchema succeeds");
    expect(error.isEmpty(), "no error");

    int ver = DatabaseMigration::currentVersion(db);
    expect(ver == 1, "schema version is 1");

    db.close();
    return 0;
}

static int testCheckIn()
{
    std::fprintf(stdout, "Test: first record() of the day is check-in\n");
    QSqlDatabase db = openTestDb("checkin-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    AttendanceWriteResult result = repo.record(c, 300, "test");

    expect(result.status == AttendanceWriteStatus::Inserted, "status is Inserted");
    expect(result.eventType == AttendanceEventType::CheckIn, "event type is CheckIn");
    expect(!result.eventKey.isEmpty(), "event key is not empty");

    db.close();
    return 0;
}

static int testSuppressBeforeCheckoutInterval()
{
    std::fprintf(stdout, "Test: second record() within interval is suppressed\n");
    QSqlDatabase db = openTestDb("suppress-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c1 = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    repo.record(c1, 300, "test");

    AttendanceConfirmation c2 = makeConfirmation("E001", 0.93f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 1, 0)));
    AttendanceWriteResult result = repo.record(c2, 300, "test");

    expect(result.status == AttendanceWriteStatus::Suppressed, "status is Suppressed");
    expect(result.eventType == AttendanceEventType::CheckIn, "event type is CheckIn (same as previous)");
    expect(result.message.contains("未到可签退时间"), "message mentions interval not elapsed");

    db.close();
    return 0;
}

static int testCheckOutAfterInterval()
{
    std::fprintf(stdout, "Test: record() after interval is check-out\n");
    QSqlDatabase db = openTestDb("checkout-interval-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c1 = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    repo.record(c1, 300, "test");

    AttendanceConfirmation c2 = makeConfirmation("E001", 0.93f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 10, 0)));
    AttendanceWriteResult result = repo.record(c2, 300, "test");

    expect(result.status == AttendanceWriteStatus::Inserted, "status is Inserted");
    expect(result.eventType == AttendanceEventType::CheckOut, "event type is CheckOut");

    db.close();
    return 0;
}

static int testExplicitCheckOut()
{
    std::fprintf(stdout, "Test: recordCheckOut() creates check-out\n");
    QSqlDatabase db = openTestDb("explicit-checkout-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c1 = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    repo.record(c1, 300, "test");

    AttendanceConfirmation c2 = makeConfirmation("E001", 0.93f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 5, 0)));
    AttendanceWriteResult result = repo.recordCheckOut(c2, "test");

    expect(result.status == AttendanceWriteStatus::Inserted, "status is Inserted");
    expect(result.eventType == AttendanceEventType::CheckOut, "event type is CheckOut");

    db.close();
    return 0;
}

static int testSuppressDuplicateCheckOut()
{
    std::fprintf(stdout, "Test: second recordCheckOut() is suppressed\n");
    QSqlDatabase db = openTestDb("dup-checkout-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c1 = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    repo.record(c1, 300, "test");

    AttendanceConfirmation c2 = makeConfirmation("E001", 0.93f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 5, 0)));
    repo.recordCheckOut(c2, "test");

    AttendanceConfirmation c3 = makeConfirmation("E001", 0.91f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 6, 0)));
    AttendanceWriteResult result = repo.recordCheckOut(c3, "test");

    expect(result.status == AttendanceWriteStatus::Suppressed, "second checkout suppressed");
    expect(result.message.contains("已完成签退"), "message mentions already checked out");

    db.close();
    return 0;
}

static int testEventKeyFormat()
{
    std::fprintf(stdout, "Test: event key format is correct\n");
    QSqlDatabase db = openTestDb("eventkey-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c1 = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    AttendanceWriteResult r1 = repo.record(c1, 300, "test");
    expect(r1.status == AttendanceWriteStatus::Inserted, "first insert succeeds");
    expect(r1.eventKey == "4:E001:20260912:签到", "check-in event key format correct");

    AttendanceConfirmation c2 = makeConfirmation("E001", 0.93f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 10, 0)));
    AttendanceWriteResult r2 = repo.record(c2, 300, "test");
    expect(r2.status == AttendanceWriteStatus::Inserted, "checkout succeeds");
    expect(r2.eventKey == "4:E001:20260912:签退", "check-out event key format correct");

    db.close();
    return 0;
}

static int testUpdateSnapshotPath()
{
    std::fprintf(stdout, "Test: updateSnapshotPath sets snapshot path\n");
    QSqlDatabase db = openTestDb("snapshot-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    AttendanceWriteResult r = repo.record(c, 300, "test");

    QString error;
    bool ok = repo.updateSnapshotPath(r.eventKey, "/tmp/snap.jpg", &error);
    expect(ok, "updateSnapshotPath succeeds");
    expect(error.isEmpty(), "no error");

    QSqlQuery check(db);
    check.prepare("SELECT snapshot_path FROM recorduser WHERE event_key = ?");
    check.addBindValue(r.eventKey);
    check.exec();
    expect(check.next(), "row found");
    expect(check.value(0).toString() == "/tmp/snap.jpg", "snapshot path correct");

    db.close();
    return 0;
}

static int testInvalidInputs()
{
    std::fprintf(stdout, "Test: invalid inputs return Failed\n");
    QSqlDatabase db = openTestDb("invalid-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation empty;
    AttendanceWriteResult r1 = repo.record(empty, 300, "test");
    expect(r1.status == AttendanceWriteStatus::Failed, "empty number fails");

    AttendanceConfirmation invalid = makeConfirmation("E001", 0.9f, QDateTime());
    AttendanceWriteResult r2 = repo.record(invalid, 300, "test");
    expect(r2.status == AttendanceWriteStatus::Failed, "invalid timestamp fails");

    db.close();
    return 0;
}

static int testMultipleDaysIndependent()
{
    std::fprintf(stdout, "Test: check-in on different days are independent\n");
    QSqlDatabase db = openTestDb("multiday-test");
    AttendanceRepository repo(db);
    repo.ensureSchema();

    AttendanceConfirmation c1 = makeConfirmation("E001", 0.95f,
        QDateTime(QDate(2026, 9, 11), QTime(8, 0, 0)));
    repo.record(c1, 300, "test");

    AttendanceConfirmation c2 = makeConfirmation("E001", 0.93f,
        QDateTime(QDate(2026, 9, 12), QTime(8, 0, 0)));
    AttendanceWriteResult r2 = repo.record(c2, 300, "test");

    expect(r2.status == AttendanceWriteStatus::Inserted, "new day gets Inserted");
    expect(r2.eventType == AttendanceEventType::CheckIn, "new day is CheckIn");

    db.close();
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testSchemaVersion();
    testCheckIn();
    testSuppressBeforeCheckoutInterval();
    testCheckOutAfterInterval();
    testExplicitCheckOut();
    testSuppressDuplicateCheckOut();
    testEventKeyFormat();
    testUpdateSnapshotPath();
    testInvalidInputs();
    testMultipleDaysIndependent();

    std::fprintf(stdout, "\nAttendanceRepositoryTest: %d passed, %d failed\n",
                 s_passCount, s_failCount);
    return s_failCount > 0 ? 1 : 0;
}
