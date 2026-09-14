#ifndef ATTENDANCEREPORT_H
#define ATTENDANCEREPORT_H

#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

struct AttendanceReportFilter
{
    QString number;
    QDate startDate;
    QDate endDate;
    QString eventType;
};

class AttendanceReport
{
public:
    static bool query(const QSqlDatabase &database, const AttendanceReportFilter &filter,
                      QSqlQuery *query, QString *errorMessage = nullptr);
    static bool exportCsv(const QSqlDatabase &database, const AttendanceReportFilter &filter,
                          const QString &filePath, QString *errorMessage = nullptr);
};

#endif // ATTENDANCEREPORT_H
