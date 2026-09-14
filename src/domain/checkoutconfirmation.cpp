#include "checkoutconfirmation.h"

CheckoutConfirmation::CheckoutConfirmation(int requiredMilliseconds)
    : m_requiredMilliseconds(qMax(1, requiredMilliseconds))
{
}

void CheckoutConfirmation::start(const QString &number, const QDateTime &startedAt)
{
    m_number = number;
    m_startedAt = startedAt;
}

void CheckoutConfirmation::reset()
{
    m_number.clear();
    m_startedAt = QDateTime();
}

bool CheckoutConfirmation::observe(const QString &number, const QDateTime &observedAt)
{
    if (!isActive() || number != m_number || !observedAt.isValid()) {
        reset();
        return false;
    }
    return elapsedMilliseconds(observedAt) >= m_requiredMilliseconds;
}

bool CheckoutConfirmation::isActive() const
{
    return !m_number.isEmpty() && m_startedAt.isValid();
}

int CheckoutConfirmation::elapsedMilliseconds(const QDateTime &now) const
{
    if (!isActive() || !now.isValid()) {
        return 0;
    }
    return qMax(0, static_cast<int>(m_startedAt.msecsTo(now)));
}

QString CheckoutConfirmation::number() const
{
    return m_number;
}
