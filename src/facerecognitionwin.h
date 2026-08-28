#ifndef FACERECOGNITIONWIN_H
#define FACERECOGNITIONWIN_H

#include <QMainWindow>
#include <QThread>
#include <QHash>
#include <memory>
#include "attendancerepository.h"
#include "attendancestatemachine.h"
#include "ivideosource.h"
#include "qfaceobject.h"

QT_BEGIN_NAMESPACE
namespace Ui { class FaceRecognitionWin; }
QT_END_NAMESPACE

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
    void on_stopVideoBt_clicked();

protected slots:
    void recvName(const QString &name);
    //接收查询结果
    void recvQueryResult(int index, float similarty, quint64 requestId);
signals:
    //发送图片给，给到人脸识别对象，（线程来查询-识别）
    void sendQueryCmd(const cv::Mat &faceMat, quint64 requestId);
private:
    void openVideoFile(const QString &filePath);
    void stopVideoSource();
    void updateVideoSourceStatus();
    void updateAttendanceStatus(const QString &message, bool failed = false);
    void showUnknownPerson();
    Ui::FaceRecognitionWin *ui;
    QWidget *win;
    int timerid;
    cv::Mat videoImage;
    std::unique_ptr<IVideoSource> mVideoSource;
    bool mRecognitionInputActive;
    bool mRecognitionRequestPending;
    quint64 mRecognitionRequestId;
    //定义一个人脸识别对象
    QFaceObject mfaceObject;
    //定义一个线程用来识别
    QThread *mthread;
    AttendanceStateMachine mAttendanceStateMachine;
    AttendanceRepository mAttendanceRepository;
    QHash<QString, QDateTime> mLastAttendanceConfirmationByNumber;
};
#endif // FACERECOGNITIONWIN_H
