#ifndef CHECKOUTCONFIRMATION_H
#define CHECKOUTCONFIRMATION_H

#include <QDateTime>
#include <QString>

class CheckoutConfirmation
{
public:
    explicit CheckoutConfirmation(int requiredMilliseconds = 3000);

    void start(const QString &number, const QDateTime &startedAt);
    void reset();
    bool observe(const QString &number, const QDateTime &observedAt);
    bool isActive() const;
    int elapsedMilliseconds(const QDateTime &now) const;
    QString number() const;

private:
    int m_requiredMilliseconds;
    QString m_number;
    QDateTime m_startedAt;
};

#endif // CHECKOUTCONFIRMATION_H
