#include "../src/monitor/performancemetrics.h"

#include <QVector>

#include <cstdio>

static int s_passCount = 0;
static int s_failCount = 0;

static void expect(bool condition, const char *label)
{
    if (condition) {
        ++s_passCount;
        std::printf("PASS  %s\n", label);
    } else {
        ++s_failCount;
        std::printf("FAIL  %s\n", label);
    }
}

int main()
{
    expect(PerformanceMetrics::percentile95(QVector<qint64>()) == 0,
           "empty samples return 0");

    QVector<qint64> single;
    single << 42;
    expect(PerformanceMetrics::percentile95(single) == 42, "single sample is p95");

    QVector<qint64> hundred;
    for (int i = 1; i <= 100; ++i) {
        hundred << i;
    }
    expect(PerformanceMetrics::percentile95(hundred) == 95, "1..100 ascending p95 is 95");

    QVector<qint64> twenty;
    twenty << 200 << 10 << 90 << 150 << 20 << 30 << 40 << 50 << 60 << 70
           << 80 << 100 << 110 << 120 << 130 << 140 << 160 << 170 << 180 << 190;
    expect(PerformanceMetrics::percentile95(twenty) == 190,
           "20 unsorted samples p95 is second largest");

    expect(PerformanceMetrics::percentile95(hundred) == 95,
           "percentile95 does not modify caller expectations on reuse");

    expect(PerformanceMetrics::framesPerSecond(50, 2000) == 25.0,
           "50 frames in 2000 ms is 25 fps");
    expect(PerformanceMetrics::framesPerSecond(0, 5000) == 0.0,
           "zero frames is 0 fps");
    expect(PerformanceMetrics::framesPerSecond(10, 0) == 0.0,
           "zero interval is 0 fps");

    expect(PerformanceMetrics::writeFailureRatePercent(9, 0, 1) == 10.0,
           "1 of 10 writes failed is 10 percent");
    expect(PerformanceMetrics::writeFailureRatePercent(0, 0, 0) == 0.0,
           "no writes is 0 percent");
    expect(PerformanceMetrics::writeFailureRatePercent(0, 0, 2) == 100.0,
           "all failed is 100 percent");
    expect(PerformanceMetrics::writeFailureRatePercent(0, 3, 0) == 0.0,
           "suppressed writes are not failures");

    std::printf("\nPerformanceMetricsTest: %d passed, %d failed\n", s_passCount, s_failCount);
    return s_failCount > 0 ? 1 : 0;
}
