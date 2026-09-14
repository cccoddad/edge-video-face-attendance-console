#include "attendancereport.h"

#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QTextStream>
#include <QVariant>

namespace {
QString filterSql(const AttendanceReportFilter &filter)
{
    QString sql;
    if (!filter.number.trimmed().isEmpty()) {
        sql += " AND number = ?";
    }
    if (filter.startDate.isValid()) {
        sql += " AND checktime >= ?";
    }
    if (filter.endDate.isValid()) {
        sql += " AND checktime < ?";
    }
    if (!filter.eventType.isEmpty()) {
        sql += " AND event_type = ?";
    }
    return sql;
}

void bindFilter(const AttendanceReportFilter &filter, QSqlQuery *query)
{
    if (!filter.number.trimmed().isEmpty()) {
        query->addBindValue(filter.number.trimmed());
    }
    if (filter.startDate.isValid()) {
        query->addBindValue(QDateTime(filter.startDate, QTime(0, 0)).toString("yyyy-MM-dd hh:mm:ss"));
    }
    if (filter.endDate.isValid()) {
        query->addBindValue(QDateTime(filter.endDate.addDays(1), QTime(0, 0)).toString("yyyy-MM-dd hh:mm:ss"));
    }
    if (!filter.eventType.isEmpty()) {
        query->addBindValue(filter.eventType);
    }
}

QString csvField(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    return QString("\"%1\"").arg(escaped);
}

void writeCsvRow(QTextStream *stream, const QStringList &values)
{
    QStringList fields;
    for (const QString &value : values) {
        fields.append(csvField(value));
    }
    *stream << fields.join(',') << "\r\n";
}
}

bool AttendanceReport::query(const QSqlDatabase &database, const AttendanceReportFilter &filter,
                             QSqlQuery *query, QString *errorMessage)
{
    if (!database.isOpen() || !query) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SQLite 数据库未打开");
        }
        return false;
    }

    QString sql = "SELECT number, checktime, event_type, similarity, source_type "
                  "FROM recorduser WHERE 1 = 1";
    sql += filterSql(filter);
    sql += " ORDER BY checktime DESC, id DESC";
    QSqlQuery result(database);
    result.prepare(sql);
    bindFilter(filter, &result);
    if (!result.exec()) {
        if (errorMessage) {
            *errorMessage = result.lastError().text();
        }
        return false;
    }
    *query = result;
    return true;
}

bool AttendanceReport::exportCsv(const QSqlDatabase &database, const AttendanceReportFilter &filter,
                                 const QString &filePath, QString *errorMessage)
{
    QSqlQuery query;
    if (!AttendanceReport::query(database, filter, &query, errorMessage)) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream.setGenerateByteOrderMark(true);
    writeCsvRow(&stream, { QStringLiteral("工号"), QStringLiteral("时间"), QStringLiteral("事件"),
                            QStringLiteral("相似度"), QStringLiteral("来源") });
    while (query.next()) {
        writeCsvRow(&stream, { query.value(0).toString(), query.value(1).toString(),
                                query.value(2).toString(), query.value(3).toString(),
                                query.value(4).toString() });
    }
    if (stream.status() != QTextStream::Ok) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写入 CSV 文件失败");
        }
        return false;
    }
    return true;
}
