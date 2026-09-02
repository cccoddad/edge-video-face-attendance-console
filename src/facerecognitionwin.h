#ifndef FACERECOGNITIONWIN_H
#define FACERECOGNITIONWIN_H

#include <QMainWindow>
#include <QThread>
#include <QHash>
#include <QElapsedTimer>
#include <QFile>
#include <QRect>
#include <memory>
#include "attendancerepository.h"
#include "attendancestatemachine.h"
#include "checkoutconfirmation.h"
#include "ivideosource.h"
#include "qfaceobject.h"
#include "rtspconfiguration.h"
#include "videosourceruntimelog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class FaceRecognitionWin; }
QT_END_NAMESPACE
class QPushButton;
class QPlainTextEdit;

class FaceRecognitionWin : public QMainWindow
{
    Q_OBJECT
public:
    FaceRecognitionWin(QWidget *parent = nullptr);
    ~FaceRecognitionWin();
    virtual void timerEvent(QTimerEvent *e);

private slots:
    void on_recognitionRb_clicked();
    void on_registerRb_clicked();
    void on_queryRb_clicked();
    void on_openVideoBt_clicked();
    void on_openLocalCameraBt_clicked();
    void on_configureRtspBt_clicked();
    void on_stopVideoBt_clicked();
    void on_modeTabs_currentChanged(int index);
    void on_requestCheckout_clicked();

protected slots:
    void recvName(const QString &name);
    void recvTrackerResult(bool hasSingleFace, const QRect &faceRect, quint64 requestId);
    //接收查询结果
    void recvQueryResult(int index, float similarty, quint64 requestId);
signals:
    //发送图片给，给到人脸识别对象，（线程来查询-识别）
    void sendQueryCmd(const cv::Mat &faceMat, quint64 requestId);
    void sendTrackerCmd(const cv::Mat &faceMat, quint64 requestId);
    void registrationPhotoCaptured(bool success, const QString &message);
private:
    void setupModernLayout();
    void showRecognitionPage();
    void showRegisterPage();
    void showQueryPage();
    void clearSidePage();
    void openVideoFile(const QString &filePath);
    void openLocalCamera();
    void stopVideoSource();
    void updateVideoSourceStatus();
    void appendVideoSourceEvent(const QString &detail);
    void appendRuntimeEvent(const QString &sourceType, VideoSourceState state, const QString &detail);
    void refreshVideoSourceEventView();
    void updateMediaControls();
    void updateAttendanceStatus(const QString &message, bool failed = false);
    void updateFaceOverlay(QPixmap *pixmap, const QSize &sourceSize) const;
    void finishAttendanceWrite(const AttendanceWriteResult &writeResult,
                               const AttendanceConfirmation &confirmation);
    void resetCheckoutConfirmation(const QString &message = QString());
    void showUnknownPerson();
    void setRecognitionAvatar(const QString &photoPath);
    void captureRegistrationPhoto(const QString &photoPath);
    void initializePerformanceLog();
    void writePerformanceSample(bool force = false);
    void recordAttendanceWriteResult(AttendanceWriteStatus status);
    Ui::FaceRecognitionWin *ui;
    QWidget *win;
    int timerid;
    cv::Mat videoImage;
    std::unique_ptr<IVideoSource> mVideoSource;
    bool mRecognitionInputActive;
    bool mRecognitionRequestPending;
    quint64 mRecognitionRequestId;
    cv::Mat mPendingRecognitionFrame;
    bool mTrackerRequestPending;
    quint64 mTrackerRequestId;
    cv::Mat mPendingTrackerFrame;
    QRect mTrackedFaceRect;
    QString mTrackedFaceLabel;
    QElapsedTimer mRecognitionDispatchTimer;
    QElapsedTimer mPerformanceTimer;
    QFile mPerformanceLog;
    qint64 mLastPerformanceSampleMilliseconds;
    quint64 mFramesRead;
    quint64 mRecognitionRequests;
    quint64 mRecognitionResults;
    qint64 mRecognitionLatencyTotalMilliseconds;
    qint64 mRecognitionLatencyMaximumMilliseconds;
    QHash<quint64, qint64> mRecognitionRequestStartMilliseconds;
    quint64 mAttendanceInserted;
    quint64 mAttendanceSuppressed;
    quint64 mAttendanceFailed;
    //定义一个人脸识别对象
    QFaceObject mfaceObject;
    //定义一个线程用来识别
    QThread *mthread;
    AttendanceStateMachine mAttendanceStateMachine;
    CheckoutConfirmation mCheckoutConfirmation;
    AttendanceRepository mAttendanceRepository;
    QHash<QString, QDateTime> mLastAttendanceConfirmationByNumber;
    QString mLastRecognizedNumber;
    QString mVideoSourceType;
    RtspConfiguration mRtspConfiguration;
    VideoSourceRuntimeLog mVideoSourceRuntimeLog;
    QTabWidget *mModeTabs;
    QWidget *mRecognitionTab;
    QWidget *mRegisterTab;
    QWidget *mQueryTab;
    QPushButton *mCheckoutBt;
    QPushButton *mConfigureRtspBt;
    QPlainTextEdit *mSourceEventView;
};
#endif // FACERECOGNITIONWIN_H
