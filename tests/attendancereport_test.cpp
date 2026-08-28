#include "../src/attendancereport.h"

#include <QCoreApplication>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <cstdio>

namespace {
bool insertRecord(QSqlDatabase database, const QString &number, const QString &time,
                  const QString &eventType)
{
    QSqlQuery query(database);
    query.prepare("INSERT INTO recorduser(number, checktime, event_type, event_key, similarity, source_type) "
                  "VALUES(?, ?, ?, ?, ?, ?)");
    query.addBindValue(number);
    query.addBindValue(time);
    query.addBindValue(eventType);
    query.addBindValue(number + time + eventType);
    query.addBindValue(0.90);
    query.addBindValue("video-file");
    return query.exec();
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) {
        std::fprintf(stderr, "Usage: AttendanceReportTest <csv-output-path>\n");
        return 2;
    }

    QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "attendance-report-test");
    database.setDatabaseName(":memory:");
    if (!database.open()) {
        std::fprintf(stderr, "Cannot open in-memory SQLite: %s\n",
                     qPrintable(database.lastError().text()));
        return 3;
    }

    QSqlQuery createQuery(database);
    if (!createQuery.exec("CREATE TABLE recorduser(id integer primary key autoincrement, number text, "
                          "checktime text, event_type text, event_key text, similarity real, source_type text)")) {
        std::fprintf(stderr, "Cannot create report table\n");
        return 4;
    }
    if (!insertRecord(database, "E001", "2026-08-28 08:00:00", "签到")
            || !insertRecord(database, "E001", "2026-08-28 16:00:00", "签退")
            || !insertRecord(database, "E002", "2026-08-28 08:10:00", "签到")
            || !insertRecord(database, "E001", "2026-08-29 08:00:00", "签到")) {
        std::fprintf(stderr, "Cannot insert report fixtures\n");
        return 5;
    }

    AttendanceReportFilter filter;
    filter.number = "E001";
    filter.startDate = QDate(2026, 8, 28);
    filter.endDate = QDate(2026, 8, 28);
    filter.eventType = "签到";

    QSqlQuery filteredQuery;
    QString errorMessage;
    if (!AttendanceReport::query(database, filter, &filteredQuery, &errorMessage)) {
        std::fprintf(stderr, "Filtered query failed: %s\n", qPrintable(errorMessage));
        return 6;
    }
    if (!filteredQuery.next() || filteredQuery.value(0).toString() != "E001"
            || filteredQuery.value(2).toString() != "签到" || filteredQuery.next()) {
        std::fprintf(stderr, "Filtered query returned unexpected rows\n");
        return 7;
    }

    const QString outputPath = application.arguments().at(1);
    if (!AttendanceReport::exportCsv(database, filter, outputPath, &errorMessage)) {
        std::fprintf(stderr, "CSV export failed: %s\n", qPrintable(errorMessage));
        return 8;
    }

    QFile output(outputPath);
    if (!output.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "Cannot read exported CSV\n");
        return 9;
    }
    const QByteArray content = output.readAll();
    if (!content.startsWith("\xEF\xBB\xBF") || !content.contains("E001")
            || content.contains("E002") || content.count("\r\n") != 2) {
        std::fprintf(stderr, "CSV content did not match the selected filter\n");
        return 10;
    }

    std::fprintf(stdout, "Attendance report test passed: filtered query and CSV export verified\n");
    return 0;
}
