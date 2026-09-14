#ifndef PERFORMANCEMETRICS_H
#define PERFORMANCEMETRICS_H

#include <QVector>
#include <QtGlobal>

class PerformanceMetrics
{
public:
    static qint64 percentile95(QVector<qint64> samples);
    static double framesPerSecond(quint64 frames, qint64 intervalMilliseconds);
    static double writeFailureRatePercent(quint64 inserted, quint64 suppressed, quint64 failed);
};

#endif
