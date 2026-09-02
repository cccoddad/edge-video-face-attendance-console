#include "rtspreconnectscheduler.h"

RtspReconnectScheduler::RtspReconnectScheduler(int intervalMilliseconds)
    : mIntervalMilliseconds(qMax(0, intervalMilliseconds))
{
}

void RtspReconnectScheduler::schedule(const QDateTime &currentTime)
{
    mNextAttemptAt = currentTime.addMSecs(mIntervalMilliseconds);
}

void RtspReconnectScheduler::clear()
{
    mNextAttemptAt = QDateTime();
}

bool RtspReconnectScheduler::isScheduled() const
{
    return mNextAttemptAt.isValid();
}

bool RtspReconnectScheduler::isDue(const QDateTime &currentTime) const
{
    return isScheduled() && currentTime >= mNextAttemptAt;
}

QDateTime RtspReconnectScheduler::nextAttemptAt() const
{
    return mNextAttemptAt;
}
