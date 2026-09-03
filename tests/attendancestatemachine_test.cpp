#include "../src/attendancerepository.h"
#include "../src/attendancestatemachine.h"
#include "../src/checkoutconfirmation.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <cstdio>

namespace {
bool confirm(AttendanceStateMachine *machine, const QString &number,
             const QDateTime &timestamp, AttendanceConfirmation *confirmation)
{
    return !machine->observe(number, 0.91f, timestamp, confirmation)
            && !machine->observe(number, 0.92f, timestamp.addSecs(1), confirmation)
            && machine->observe(number, 0.93f, timestamp.addSecs(2), confirmation);
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "attendance-state-test");
    database.setDatabaseName(":memory:");
    if (!database.open()) {
        std::fprintf(stderr, "Cannot open in-memory SQLite: %s\n",
                     qPrintable(database.lastError().text()));
        return 2;
    }

    QSqlQuery createQuery(database);
    if (!createQuery.exec("CREATE TABLE recorduser(id integer primary key autoincrement, "
                          "number varchar(32), checktime text)")) {
        std::fprintf(stderr, "Cannot create record table: %s\n",
                     qPrintable(createQuery.lastError().text()));
        return 3;
    }

    AttendanceRepository repository(database);
    QString schemaError;
    if (!repository.ensureSchema(&schemaError)) {
        std::fprintf(stderr, "Schema migration failed: %s\n", qPrintable(schemaError));
        return 4;
    }

    AttendanceStateMachine machine(3);
    const QDateTime checkInTime(QDate(2026, 8, 28), QTime(8, 0));
    AttendanceConfirmation confirmation;
    if (!confirm(&machine, "E001", checkInTime, &confirmation)
            || confirmation.number != "E001") {
        std::fprintf(stderr, "Three-frame check-in confirmation failed\n");
        return 5;
    }

    AttendanceWriteResult result = repository.record(confirmation, 4 * 60 * 60, "video-file");
    if (result.status != AttendanceWriteStatus::Inserted
            || result.eventType != AttendanceEventType::CheckIn || result.eventKey.isEmpty()) {
        std::fprintf(stderr, "Check-in write failed: %s\n", qPrintable(result.message));
        return 6;
    }

    QString snapshotError;
    if (!repository.updateSnapshotPath(result.eventKey, "C:/test/snapshot.jpg", &snapshotError)) {
        std::fprintf(stderr, "Snapshot path update failed: %s\n", qPrintable(snapshotError));
        return 14;
    }
    QSqlQuery snapshotQuery(database);
    snapshotQuery.prepare("SELECT snapshot_path FROM recorduser WHERE event_key = ?");
    snapshotQuery.addBindValue(result.eventKey);
    if (!snapshotQuery.exec() || !snapshotQuery.next()
            || snapshotQuery.value(0).toString() != "C:/test/snapshot.jpg") {
        std::fprintf(stderr, "Snapshot path was not stored\n");
        return 15;
    }

    machine.reset();
    if (!confirm(&machine, "E001", checkInTime.addSecs(60), &confirmation)) {
        std::fprintf(stderr, "Repeated confirmation setup failed\n");
        return 7;
    }
    result = repository.record(confirmation, 4 * 60 * 60, "video-file");
    if (result.status != AttendanceWriteStatus::Suppressed) {
        std::fprintf(stderr, "Short-interval duplicate was not suppressed\n");
        return 8;
    }

    CheckoutConfirmation checkoutConfirmation;
    const QDateTime checkoutStartedAt = checkInTime.addSecs(5 * 60 * 60 + 10);
    checkoutConfirmation.start("E002", checkoutStartedAt);
    if (checkoutConfirmation.observe("E002", checkoutStartedAt.addMSecs(2999))
            || !checkoutConfirmation.observe("E002", checkoutStartedAt.addMSecs(3000))) {
        std::fprintf(stderr, "Three-second checkout confirmation failed\n");
        return 16;
    }
    checkoutConfirmation.start("E002", checkoutStartedAt);
    if (checkoutConfirmation.observe("E003", checkoutStartedAt.addMSecs(3000))
            || checkoutConfirmation.isActive()) {
        std::fprintf(stderr, "Checkout confirmation did not reset on person change\n");
        return 17;
    }
    checkoutConfirmation.start("E002", checkoutStartedAt);
    if (checkoutConfirmation.observe(QString(), checkoutStartedAt.addMSecs(3000))
            || checkoutConfirmation.isActive()) {
        std::fprintf(stderr, "Checkout confirmation did not reset when no face was recognized\n");
        return 21;
    }
    checkoutConfirmation.start("E002", checkoutStartedAt);
    checkoutConfirmation.reset();
    if (checkoutConfirmation.isActive()
            || checkoutConfirmation.observe("E002", checkoutStartedAt.addMSecs(3000))) {
        std::fprintf(stderr, "Checkout confirmation survived an explicit video-stop reset\n");
        return 22;
    }

    AttendanceConfirmation manualCheckout;
    manualCheckout.number = "E002";
    manualCheckout.similarity = 0.95f;
    manualCheckout.timestamp = checkoutStartedAt;
    result = repository.recordCheckOut(manualCheckout, "local-camera");
    if (result.status != AttendanceWriteStatus::Suppressed) {
        std::fprintf(stderr, "Check-out without check-in was not suppressed\n");
        return 18;
    }
    result = repository.record(manualCheckout, 4 * 60 * 60, "local-camera");
    if (result.status != AttendanceWriteStatus::Inserted
            || result.eventType != AttendanceEventType::CheckIn) {
        std::fprintf(stderr, "Manual checkout test setup failed\n");
        return 19;
    }
    manualCheckout.timestamp = checkoutStartedAt.addSecs(3);
    result = repository.recordCheckOut(manualCheckout, "local-camera");
    if (result.status != AttendanceWriteStatus::Inserted
            || result.eventType != AttendanceEventType::CheckOut) {
        std::fprintf(stderr, "Confirmed manual check-out write failed\n");
        return 20;
    }
    manualCheckout.timestamp = checkoutStartedAt.addSecs(6);
    result = repository.recordCheckOut(manualCheckout, "local-camera");
    if (result.status != AttendanceWriteStatus::Suppressed) {
        std::fprintf(stderr, "Repeated manual check-out was not suppressed\n");
        return 23;
    }

    machine.reset();
    if (!confirm(&machine, "E001", checkInTime.addSecs(4 * 60 * 60), &confirmation)) {
        std::fprintf(stderr, "Check-out confirmation setup failed\n");
        return 9;
    }
    result = repository.record(confirmation, 4 * 60 * 60, "video-file");
    if (result.status != AttendanceWriteStatus::Inserted
            || result.eventType != AttendanceEventType::CheckOut) {
        std::fprintf(stderr, "Check-out write failed: %s\n", qPrintable(result.message));
        return 10;
    }

    machine.reset();
    if (!confirm(&machine, "E001", checkInTime.addSecs(5 * 60 * 60), &confirmation)) {
        std::fprintf(stderr, "Completed-day confirmation setup failed\n");
        return 11;
    }
    result = repository.record(confirmation, 4 * 60 * 60, "video-file");
    if (result.status != AttendanceWriteStatus::Suppressed) {
        std::fprintf(stderr, "Completed-day event was not suppressed\n");
        return 12;
    }

    QSqlQuery countQuery(database);
    if (!countQuery.exec("SELECT COUNT(*) FROM recorduser") || !countQuery.next()
            || countQuery.value(0).toInt() != 4) {
        std::fprintf(stderr, "Unexpected attendance row count\n");
        return 13;
    }

    std::fprintf(stdout,
                 "Attendance state machine test passed: confirmation, check-in, check-out, "
                 "person-change, no-face, video-stop reset and idempotency verified\n");
    database.close();
    QSqlDatabase::removeDatabase("attendance-state-test");
    return 0;
}
