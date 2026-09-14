#include "databasemigration.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

bool ensureVersionTable(QSqlDatabase database, QString *errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec("CREATE TABLE IF NOT EXISTS schema_version "
                    "(version INTEGER NOT NULL)")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    if (!query.exec("INSERT OR IGNORE INTO schema_version(version) VALUES(0)")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool ensureColumn(QSqlDatabase database, const QString &table,
                  const QString &column, const QString &definition,
                  QString *errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(table))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == column) {
            return true;
        }
    }
    if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                    .arg(table, column, definition))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool upgradeToV1(QSqlDatabase database, QString *errorMessage)
{
    const char *columns[][2] = {
        {"checktime",    "TEXT"},
        {"event_type",   "TEXT"},
        {"event_key",    "TEXT"},
        {"similarity",   "REAL"},
        {"source_type",  "TEXT"},
        {"snapshot_path","TEXT"},
    };
    for (const auto &col : columns) {
        if (!ensureColumn(database, "recorduser", col[0], col[1], errorMessage)) {
            return false;
        }
    }

    QSqlQuery query(database);
    if (!query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_recorduser_event_key "
                    "ON recorduser(event_key) WHERE event_key IS NOT NULL")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

}

namespace DatabaseMigration {

bool migrate(QSqlDatabase database, QString *errorMessage)
{
    if (!database.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据库未打开");
        }
        return false;
    }

    if (!ensureVersionTable(database, errorMessage)) {
        return false;
    }

    int version = currentVersion(database);

    if (version < 1) {
        if (!upgradeToV1(database, errorMessage)) {
            return false;
        }
        QSqlQuery query(database);
        if (!query.exec("UPDATE schema_version SET version = 1")) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
        version = 1;
    }

    Q_UNUSED(version);
    return true;
}

int currentVersion(QSqlDatabase database)
{
    QSqlQuery query(database);
    if (!query.exec("SELECT version FROM schema_version LIMIT 1") || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

}
