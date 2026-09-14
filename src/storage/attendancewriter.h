#ifndef ATTENDANCEWRITER_H
#define ATTENDANCEWRITER_H

#include "attendancerepository.h"
#include "attendancestatemachine.h"
#include "ivideosource.h"

#include <QObject>
#include <QSqlDatabase>
#include <memory>

class AttendanceWriter : public QObject
{
    Q_OBJECT
public:
    explicit AttendanceWriter(QObject *parent = nullptr);
    ~AttendanceWriter() override;

public slots:
    void initialize();
    void shutdown();
    void record(const AttendanceConfirmation &confirmation,
                int minimumCheckoutIntervalSeconds, const QString &sourceType,
                const cv::Mat &snapshotFrame, quint64 requestId);
    void recordCheckOut(const AttendanceConfirmation &confirmation,
                        const QString &sourceType, const cv::Mat &snapshotFrame,
                        quint64 requestId);

signals:
    void ready(bool success, const QString &errorMessage);
    void writeFinished(const AttendanceWriteResult &result,
                       const AttendanceConfirmation &confirmation, quint64 requestId);

private:
    void attachSnapshot(AttendanceWriteResult *result,
                        const AttendanceConfirmation &confirmation,
                        const cv::Mat &snapshotFrame);

    QSqlDatabase mDatabase;
    std::unique_ptr<AttendanceRepository> mRepository;
};

#endif // ATTENDANCEWRITER_H
