#include "attendancewriter.h"

#include "appconfig.h"
#include "snapshotstore.h"

#include <QDebug>
#include <QSqlError>

namespace {
const char *kConnectionName = "attendance-writer";
}

AttendanceWriter::AttendanceWriter(QObject *parent)
    : QObject(parent)
{
}

AttendanceWriter::~AttendanceWriter() = default;

void AttendanceWriter::initialize()
{
    if (!QSqlDatabase::contains(QLatin1String(kConnectionName))) {
        mDatabase = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                              QLatin1String(kConnectionName));
    } else {
        mDatabase = QSqlDatabase::database(QLatin1String(kConnectionName), false);
    }
    mDatabase.setDatabaseName(AppConfig::databasePath());

    if (!mDatabase.open()) {
        emit ready(false, mDatabase.lastError().text());
        return;
    }

    mRepository.reset(new AttendanceRepository(mDatabase));
    QString schemaError;
    if (!mRepository->ensureSchema(&schemaError)) {
        mRepository.reset();
        emit ready(false, schemaError);
        return;
    }
    emit ready(true, QString());
}

void AttendanceWriter::shutdown()
{
    mRepository.reset();
    if (mDatabase.isOpen()) {
        mDatabase.close();
    }
    mDatabase = QSqlDatabase();
    if (QSqlDatabase::contains(QLatin1String(kConnectionName))) {
        QSqlDatabase::removeDatabase(QLatin1String(kConnectionName));
    }
}

void AttendanceWriter::record(const AttendanceConfirmation &confirmation,
                              int minimumCheckoutIntervalSeconds, const QString &sourceType,
                              const cv::Mat &snapshotFrame, quint64 requestId)
{
    AttendanceWriteResult result;
    if (!mRepository) {
        result.message = QStringLiteral("考勤存储线程未就绪");
        emit writeFinished(result, confirmation, requestId);
        return;
    }

    result = mRepository->record(confirmation, minimumCheckoutIntervalSeconds, sourceType);
    attachSnapshot(&result, confirmation, snapshotFrame);
    emit writeFinished(result, confirmation, requestId);
}

void AttendanceWriter::recordCheckOut(const AttendanceConfirmation &confirmation,
                                      const QString &sourceType, const cv::Mat &snapshotFrame,
                                      quint64 requestId)
{
    AttendanceWriteResult result;
    if (!mRepository) {
        result.message = QStringLiteral("考勤存储线程未就绪");
        emit writeFinished(result, confirmation, requestId);
        return;
    }

    result = mRepository->recordCheckOut(confirmation, sourceType);
    attachSnapshot(&result, confirmation, snapshotFrame);
    emit writeFinished(result, confirmation, requestId);
}

void AttendanceWriter::attachSnapshot(AttendanceWriteResult *result,
                                      const AttendanceConfirmation &confirmation,
                                      const cv::Mat &snapshotFrame)
{
    if (!result || result->status != AttendanceWriteStatus::Inserted
            || snapshotFrame.empty() || !mRepository) {
        return;
    }

    QString snapshotPath;
    QString snapshotError;
    if (!SnapshotStore::save(snapshotFrame, confirmation.number, confirmation.timestamp,
                             result->eventKey, &snapshotPath, &snapshotError)) {
        qWarning() << "attendance snapshot failed:" << snapshotError;
        result->message.append(QStringLiteral("；抓拍保存失败"));
        return;
    }
    if (!mRepository->updateSnapshotPath(result->eventKey, snapshotPath, &snapshotError)) {
        qWarning() << "attendance snapshot path update failed:" << snapshotError;
        SnapshotStore::removeSnapshot(snapshotPath);
        result->message.append(QStringLiteral("；抓拍关联失败"));
    }
}
