#include "performancemetrics.h"

#include <algorithm>
#include <cmath>

qint64 PerformanceMetrics::percentile95(QVector<qint64> samples)
{
    if (samples.isEmpty()) {
        return 0;
    }
    std::sort(samples.begin(), samples.end());
    const int rank = static_cast<int>(std::ceil(samples.size() * 0.95)) - 1;
    const int index = qBound(0, rank, samples.size() - 1);
    return samples.at(index);
}

double PerformanceMetrics::framesPerSecond(quint64 frames, qint64 intervalMilliseconds)
{
    if (intervalMilliseconds <= 0) {
        return 0.0;
    }
    return static_cast<double>(frames) * 1000.0 / static_cast<double>(intervalMilliseconds);
}

double PerformanceMetrics::writeFailureRatePercent(quint64 inserted, quint64 suppressed,
                                                    quint64 failed)
{
    const quint64 total = inserted + suppressed + failed;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(failed) * 100.0 / static_cast<double>(total);
}
