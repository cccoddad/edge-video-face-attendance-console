#ifndef ATTENDANCESTATEMACHINE_H
#define ATTENDANCESTATEMACHINE_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

struct AttendanceConfirmation
{
    QString number;
    float similarity = 0.0f;
    QDateTime timestamp;
};
Q_DECLARE_METATYPE(AttendanceConfirmation)

class AttendanceStateMachine
{
public:
    explicit AttendanceStateMachine(int requiredFrames = 3);

    bool observe(const QString &number, float similarity, const QDateTime &timestamp,
                 AttendanceConfirmation *confirmation = nullptr);
    void reset();
    int consecutiveFrames() const;
    int requiredFrames() const;

private:
    int m_requiredFrames;
    int m_consecutiveFrames;
    QString m_candidateNumber;
    bool m_confirmed;
};

#endif // ATTENDANCESTATEMACHINE_H
