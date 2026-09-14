#include "attendancestatemachine.h"

#include <QtGlobal>

AttendanceStateMachine::AttendanceStateMachine(int requiredFrames)
    : m_requiredFrames(qMax(1, requiredFrames))
    , m_consecutiveFrames(0)
    , m_confirmed(false)
{
}

bool AttendanceStateMachine::observe(const QString &number, float similarity, const QDateTime &timestamp,
                                     AttendanceConfirmation *confirmation)
{
    if (number.isEmpty() || !timestamp.isValid()) {
        reset();
        return false;
    }

    if (m_candidateNumber != number) {
        m_candidateNumber = number;
        m_consecutiveFrames = 1;
        m_confirmed = false;
    } else {
        ++m_consecutiveFrames;
    }

    if (m_confirmed || m_consecutiveFrames < m_requiredFrames) {
        return false;
    }

    m_confirmed = true;
    if (confirmation) {
        confirmation->number = number;
        confirmation->similarity = similarity;
        confirmation->timestamp = timestamp;
    }
    return true;
}

void AttendanceStateMachine::reset()
{
    m_candidateNumber.clear();
    m_consecutiveFrames = 0;
    m_confirmed = false;
}

int AttendanceStateMachine::consecutiveFrames() const
{
    return qMin(m_consecutiveFrames, m_requiredFrames);
}

int AttendanceStateMachine::requiredFrames() const
{
    return m_requiredFrames;
}
