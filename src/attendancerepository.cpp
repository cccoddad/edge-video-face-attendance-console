#include "attendancerepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
bool ensureColumn(const QSqlDatabase &database, const QString &columnName,
                  const QString &definition, QString *errorMessage)
{
    QSqlQuery query(database);
    if (!query.exec("PRAGMA table_info(recorduser)")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    while (query.next()) {
        if (query.value(1).toString() == columnName) {
            return true;
        }
    }

    if (!query.exec(QString("ALTER TABLE recorduser ADD COLUMN %1 %2")
                    .arg(columnName, definition))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

QString eventKeyFor(const QString &number, const QDate &date, AttendanceEventType type)
{
    return QString("%1:%2:%3:%4")
            .arg(number.size())
            .arg(number)
            .arg(date.toString("yyyyMMdd"))
            .arg(AttendanceRepository::eventTypeText(type));
}
}

AttendanceRepository::AttendanceRepository(const QSqlDatabase &database)
    : m_database(database)
{
}

bool AttendanceRepository::ensureSchema(QString *errorMessage) const
{
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SQLite 数据库未打开");
        }
        return false;
    }

    if (!ensureColumn(m_database, "checktime", "TEXT", errorMessage)
            || !ensureColumn(m_database, "event_type", "TEXT", errorMessage)
            || !ensureColumn(m_database, "event_key", "TEXT", errorMessage)
            || !ensureColumn(m_database, "similarity", "REAL", errorMessage)
            || !ensureColumn(m_database, "source_type", "TEXT", errorMessage)) {
        return false;
    }

    QSqlQuery query(m_database);
    if (!query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_recorduser_event_key "
                    "ON recorduser(event_key) WHERE event_key IS NOT NULL")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

AttendanceWriteResult AttendanceRepository::record(const AttendanceConfirmation &confirmation,
                                                    int minimumCheckoutIntervalSeconds,
                                                    const QString &sourceType)
{
    AttendanceWriteResult result;
    if (!m_database.isOpen() || confirmation.number.isEmpty() || !confirmation.timestamp.isValid()) {
        result.message = QStringLiteral("考勤写入参数或数据库状态无效");
        return result;
    }

    const QDate date = confirmation.timestamp.date();
    const QDateTime dayStart(date, QTime(0, 0));
    const QDateTime dayEnd = dayStart.addDays(1);
    QSqlQuery previousQuery(m_database);
    previousQuery.prepare("SELECT event_type, checktime FROM recorduser "
                          "WHERE number = ? AND checktime >= ? AND checktime < ? "
                          "AND event_type IS NOT NULL ORDER BY checktime DESC, id DESC LIMIT 1");
    previousQuery.addBindValue(confirmation.number);
    previousQuery.addBindValue(dayStart.toString("yyyy-MM-dd hh:mm:ss"));
    previousQuery.addBindValue(dayEnd.toString("yyyy-MM-dd hh:mm:ss"));
    if (!previousQuery.exec()) {
        result.message = QStringLiteral("查询当天考勤状态失败：%1").arg(previousQuery.lastError().text());
        return result;
    }

    AttendanceEventType eventType = AttendanceEventType::CheckIn;
    if (previousQuery.next()) {
        const QString previousType = previousQuery.value(0).toString();
        if (previousType == eventTypeText(AttendanceEventType::CheckOut)) {
            result.status = AttendanceWriteStatus::Suppressed;
            result.eventType = AttendanceEventType::CheckOut;
            result.message = QStringLiteral("今日已完成签到和签退");
            return result;
        }

        const QDateTime previousTime = QDateTime::fromString(previousQuery.value(1).toString(),
                                                               "yyyy-MM-dd hh:mm:ss");
        const int elapsedSeconds = previousTime.secsTo(confirmation.timestamp);
        if (previousTime.isValid() && elapsedSeconds < minimumCheckoutIntervalSeconds) {
            result.status = AttendanceWriteStatus::Suppressed;
            result.eventType = AttendanceEventType::CheckIn;
            result.message = QStringLiteral("今日已签到，未到可签退时间");
            return result;
        }
        eventType = AttendanceEventType::CheckOut;
    }

    const QString eventKey = eventKeyFor(confirmation.number, date, eventType);
    if (!m_database.transaction()) {
        result.message = QStringLiteral("开始考勤事务失败：%1").arg(m_database.lastError().text());
        return result;
    }

    QSqlQuery insertQuery(m_database);
    insertQuery.prepare("INSERT INTO recorduser(number, checktime, event_type, event_key, similarity, source_type) "
                        "VALUES(?, ?, ?, ?, ?, ?)");
    insertQuery.addBindValue(confirmation.number);
    insertQuery.addBindValue(confirmation.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    insertQuery.addBindValue(eventTypeText(eventType));
    insertQuery.addBindValue(eventKey);
    insertQuery.addBindValue(confirmation.similarity);
    insertQuery.addBindValue(sourceType);
    if (!insertQuery.exec()) {
        const QString error = insertQuery.lastError().text();
        m_database.rollback();
        if (hasEventKey(eventKey)) {
            result.status = AttendanceWriteStatus::Suppressed;
            result.eventType = eventType;
            result.message = QStringLiteral("重复考勤事件已忽略");
        } else {
            result.message = QStringLiteral("写入考勤记录失败：%1").arg(error);
        }
        return result;
    }
    if (!m_database.commit()) {
        result.message = QStringLiteral("提交考勤事务失败：%1").arg(m_database.lastError().text());
        m_database.rollback();
        return result;
    }

    result.status = AttendanceWriteStatus::Inserted;
    result.eventType = eventType;
    result.message = QStringLiteral("%1成功").arg(eventTypeText(eventType));
    return result;
}

QString AttendanceRepository::eventTypeText(AttendanceEventType type)
{
    return type == AttendanceEventType::CheckIn ? QStringLiteral("签到") : QStringLiteral("签退");
}

bool AttendanceRepository::hasEventKey(const QString &eventKey) const
{
    QSqlQuery query(m_database);
    query.prepare("SELECT 1 FROM recorduser WHERE event_key = ? LIMIT 1");
    query.addBindValue(eventKey);
    return query.exec() && query.next();
}
