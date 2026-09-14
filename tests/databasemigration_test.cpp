#include "../src/storage/databasemigration.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryFile>
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

static bool hasColumn(QSqlDatabase db, const QString &table, const QString &column)
{
    QSqlQuery q(db);
    if (!q.exec(QString("PRAGMA table_info(%1)").arg(table))) {
        return false;
    }
    while (q.next()) {
        if (q.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}

static bool tableExists(QSqlDatabase db, const QString &table)
{
    QSqlQuery q(db);
    q.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name=?");
    q.addBindValue(table);
    return q.exec() && q.next();
}

static int testFreshDatabase()
{
    std::fprintf(stdout, "Test: fresh database gets version 1 schema\n");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "fresh-test");
    db.setDatabaseName(":memory:");
    if (!db.open()) {
        std::fprintf(stderr, "Cannot open in-memory SQLite\n");
        return 1;
    }

    QSqlQuery create(db);
    create.exec("CREATE TABLE recorduser(id integer primary key autoincrement, "
                "number varchar(32), checktime text)");

    QString error;
    bool ok = DatabaseMigration::migrate(db, &error);
    expect(ok, "migrate returns true");
    expect(error.isEmpty(), "no error message");
    expect(DatabaseMigration::currentVersion(db) == 1, "version is 1");
    expect(tableExists(db, "schema_version"), "schema_version table created");
    expect(hasColumn(db, "recorduser", "event_type"), "event_type column added");
    expect(hasColumn(db, "recorduser", "event_key"), "event_key column added");
    expect(hasColumn(db, "recorduser", "similarity"), "similarity column added");
    expect(hasColumn(db, "recorduser", "source_type"), "source_type column added");
    expect(hasColumn(db, "recorduser", "snapshot_path"), "snapshot_path column added");

    db.close();
    return 0;
}

static int testIdempotentMigration()
{
    std::fprintf(stdout, "Test: migration is idempotent\n");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "idempotent-test");
    db.setDatabaseName(":memory:");
    if (!db.open()) {
        std::fprintf(stderr, "Cannot open in-memory SQLite\n");
        return 1;
    }

    QSqlQuery create(db);
    create.exec("CREATE TABLE recorduser(id integer primary key autoincrement, "
                "number varchar(32), checktime text)");

    QString error;
    DatabaseMigration::migrate(db, &error);
    int v1 = DatabaseMigration::currentVersion(db);

    DatabaseMigration::migrate(db, &error);
    int v2 = DatabaseMigration::currentVersion(db);

    expect(v1 == 1, "first migration sets version 1");
    expect(v2 == 1, "second migration keeps version 1");
    expect(error.isEmpty(), "no error on second migration");

    db.close();
    return 0;
}

static int testUpgradeFromLegacy()
{
    std::fprintf(stdout, "Test: upgrade from legacy database (no extra columns)\n");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "legacy-test");
    db.setDatabaseName(":memory:");
    if (!db.open()) {
        std::fprintf(stderr, "Cannot open in-memory SQLite\n");
        return 1;
    }

    QSqlQuery create(db);
    create.exec("CREATE TABLE recorduser(id integer primary key autoincrement, "
                "number varchar(32), checktime text)");
    create.exec("INSERT INTO recorduser(number, checktime) VALUES('E001', '2026-09-12 08:00:00')");

    expect(!hasColumn(db, "recorduser", "event_type"), "no event_type before migration");

    QString error;
    bool ok = DatabaseMigration::migrate(db, &error);
    expect(ok, "migration succeeds");
    expect(DatabaseMigration::currentVersion(db) == 1, "version upgraded to 1");
    expect(hasColumn(db, "recorduser", "event_type"), "event_type added");
    expect(hasColumn(db, "recorduser", "event_key"), "event_key added");

    QSqlQuery check(db);
    check.exec("SELECT number, checktime FROM recorduser WHERE id = 1");
    expect(check.next(), "original data preserved");
    expect(check.value(0).toString() == "E001", "number preserved");
    expect(check.value(1).toString() == "2026-09-12 08:00:00", "checktime preserved");

    db.close();
    return 0;
}

static int testClosedDatabase()
{
    std::fprintf(stdout, "Test: migrate on closed database fails gracefully\n");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "closed-test");
    db.setDatabaseName(":memory:");

    QString error;
    bool ok = DatabaseMigration::migrate(db, &error);
    expect(!ok, "migrate returns false on closed database");
    expect(!error.isEmpty(), "error message provided");

    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testFreshDatabase();
    testIdempotentMigration();
    testUpgradeFromLegacy();
    testClosedDatabase();

    std::fprintf(stdout, "\nDatabaseMigrationTest: %d passed, %d failed\n",
                 s_passCount, s_failCount);
    return s_failCount > 0 ? 1 : 0;
}
