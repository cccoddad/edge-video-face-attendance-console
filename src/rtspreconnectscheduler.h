#ifndef RTSPRECONNECTSCHEDULER_H
#define RTSPRECONNECTSCHEDULER_H

#include <QDateTime>

class RtspReconnectScheduler
{
public:
    explicit RtspReconnectScheduler(int intervalMilliseconds);

    void schedule(const QDateTime &currentTime);
    void clear();
    bool isScheduled() const;
    bool isDue(const QDateTime &currentTime) const;
    QDateTime nextAttemptAt() const;

private:
    int mIntervalMilliseconds;
    QDateTime mNextAttemptAt;
};

#endif // RTSPRECONNECTSCHEDULER_H
