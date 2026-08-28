#ifndef ATTENDANCEREPOSITORY_H
#define ATTENDANCEREPOSITORY_H

#include "attendancestatemachine.h"

#include <QSqlDatabase>

enum class AttendanceEventType
{
    CheckIn,
    CheckOut
};

enum class AttendanceWriteStatus
{
    Inserted,
    Suppressed,
    Failed
};

struct AttendanceWriteResult
{
    AttendanceWriteStatus status = AttendanceWriteStatus::Failed;
    AttendanceEventType eventType = AttendanceEventType::CheckIn;
    QString message;
};

class AttendanceRepository
{
public:
    explicit AttendanceRepository(const QSqlDatabase &database);

    bool ensureSchema(QString *errorMessage = nullptr) const;
    AttendanceWriteResult record(const AttendanceConfirmation &confirmation,
                                 int minimumCheckoutIntervalSeconds,
                                 const QString &sourceType);

    static QString eventTypeText(AttendanceEventType type);

private:
    bool hasEventKey(const QString &eventKey) const;
    QSqlDatabase m_database;
};

#endif // ATTENDANCEREPOSITORY_H
