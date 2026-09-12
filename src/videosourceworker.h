#ifndef VIDEOSOURCEWORKER_H
#define VIDEOSOURCEWORKER_H

#include "ivideosource.h"

#include <QObject>
#include <QString>
#include <memory>

class QTimer;

class VideoSourceWorker : public QObject
{
    Q_OBJECT
public:
    explicit VideoSourceWorker(QObject *parent = nullptr);
    ~VideoSourceWorker() override;

public slots:
    void openFile(const QString &filePath, bool loopEnabled);
    void openCamera(int cameraIndex);
    void openRtsp(const QString &url, int reconnectIntervalMilliseconds);
    void stop();

signals:
    void sourceOpened(const QString &sourceType, const QString &displayName, int state);
    void sourceOpenFailed(const QString &sourceType, const QString &errorMessage);
    void frameReady(const cv::Mat &frame);
    void sourceStateChanged(int state, const QString &errorDetail);
    void sourceReadFinished(int state);

private slots:
    void poll();

private:
    void startSource(std::unique_ptr<IVideoSource> source, const QString &sourceType,
                     const QString &location);

    std::unique_ptr<IVideoSource> mSource;
    QString mSourceType;
    QTimer *mPollTimer;
    VideoSourceState mLastReportedState;
    QString mLastReportedError;
};

#endif // VIDEOSOURCEWORKER_H
