#include "facerecognitionwin.h"
#include "qquerywidget.h"
#include "qregisterwidget.h"
#include "appconfig.h"
#include "snapshotstore.h"
#include "videofilesource.h"
#include "ui_facerecognitionwin.h"
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
FaceRecognitionWin::FaceRecognitionWin(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::FaceRecognitionWin),
       win(nullptr),
       timerid(0),
       mVideoSource(new VideoFileSource),
       mRecognitionInputActive(false),
       mRecognitionRequestPending(false),
       mRecognitionRequestId(0),
       mTrackerRequestPending(false),
       mTrackerRequestId(0),
       mthread(new QThread(this)),
       mAttendanceStateMachine(AppConfig::recognitionConfirmationFrames()),
       mAttendanceRepository(QSqlDatabase::database())
{
    ui->setupUi(this);
    ui->videoLb->setAlignment(Qt::AlignCenter);
    ui->videoLb->setText(QStringLiteral("请选择本地视频文件"));
    updateVideoSourceStatus();
    updateAttendanceStatus(QStringLiteral("等待本地视频"));
    //初始化线程
    //把人脸识别对象移动到线程中
    mfaceObject.moveToThread(mthread);
    //启动线程
    mthread->start();
    //由于movetothread线程里面的任务函数必须有信号启动，所以用信号关联任务函数
    connect(this, &FaceRecognitionWin::sendQueryCmd, &mfaceObject, &QFaceObject::queryface, Qt::QueuedConnection);
    connect(this, &FaceRecognitionWin::sendTrackerCmd, &mfaceObject, &QFaceObject::trackerface,
            Qt::QueuedConnection);
    //当查询到结果通过信号发送
    connect(&mfaceObject,&QFaceObject::sendQueryResult,this, &FaceRecognitionWin::recvQueryResult);
    connect(&mfaceObject, &QFaceObject::sendTrackerResult, this,
            &FaceRecognitionWin::recvTrackerResult);
}

//查询结果
void FaceRecognitionWin::recvQueryResult(int index, float similarty, quint64 requestId)
{
    if (!mRecognitionInputActive || requestId != mRecognitionRequestId) {
        return;
    }
    mRecognitionRequestPending = false;
    //打包查询质料
    qDebug()<<index<<similarty;
    if(index < 0 || similarty < AppConfig::recognitionThreshold()) {
        mAttendanceStateMachine.reset();
        updateAttendanceStatus(QStringLiteral("未通过识别阈值"));
        showUnknownPerson();
        return;
    }

    //根据id查询user表，并且把当前时间人员数据插入到考勤数据表
    QSqlQuery query;
    query.prepare("SELECT number, name, partment, facepictrue FROM user WHERE faceid = ?");
    query.addBindValue(index);
    if (!query.exec() || !query.next()) {
        qWarning() << "employee lookup failed:" << query.lastError().text();
        mAttendanceStateMachine.reset();
        updateAttendanceStatus(QStringLiteral("查询人员信息失败"), true);
        showUnknownPerson();
        return;
    }
    //查询到用户
    //获取，工号number，name，partment，facepictrue，显示界面上
    const QString number = query.value("number").toString();
    ui->RnumberLb->setText(number);
    ui->RnameLb->setText(query.value("name").toString());
    ui->RpartmentLb->setText(query.value("partment").toString());
    const QString facepictrue = query.value("facepictrue").toString();
    //显示图片
    const QString sty = QString("border:1px solid #123456;border-radius:80px;border-image: url(%1);")
                            .arg(QUrl::fromLocalFile(facepictrue).toString());
    ui->RheadLb->setStyleSheet(sty);
    const QDateTime now = QDateTime::currentDateTime();
    ui->RtimeLb->setText(now.toString("hh:mm:ss"));

    AttendanceConfirmation confirmation;
    if (!mAttendanceStateMachine.observe(number, similarty, now, &confirmation)) {
        updateAttendanceStatus(QStringLiteral("连续确认：%1/%2")
                               .arg(mAttendanceStateMachine.consecutiveFrames())
                               .arg(mAttendanceStateMachine.requiredFrames()));
        return;
    }

    const QDateTime previousConfirmation = mLastAttendanceConfirmationByNumber.value(number);
    if (previousConfirmation.isValid()
            && previousConfirmation.secsTo(now) < AppConfig::attendanceCooldownSeconds()) {
        updateAttendanceStatus(QStringLiteral("考勤冷却中，请勿重复识别"));
        return;
    }

    const AttendanceWriteResult writeResult = mAttendanceRepository.record(
                confirmation, AppConfig::minimumCheckoutIntervalSeconds(), QStringLiteral("video-file"));
    if (writeResult.status == AttendanceWriteStatus::Failed) {
        qWarning() << "attendance write failed:" << writeResult.message;
        updateAttendanceStatus(writeResult.message, true);
        return;
    }
    mLastAttendanceConfirmationByNumber.insert(number, now);
    QString statusMessage = writeResult.message;
    if (writeResult.status == AttendanceWriteStatus::Inserted) {
        QString snapshotPath;
        QString snapshotError;
        if (!SnapshotStore::save(mPendingRecognitionFrame, number, confirmation.timestamp,
                                 writeResult.eventKey, &snapshotPath, &snapshotError)) {
            qWarning() << "attendance snapshot failed:" << snapshotError;
            statusMessage.append(QStringLiteral("；抓拍保存失败"));
        } else if (!mAttendanceRepository.updateSnapshotPath(writeResult.eventKey, snapshotPath,
                                                              &snapshotError)) {
            qWarning() << "attendance snapshot path update failed:" << snapshotError;
            SnapshotStore::removeSnapshot(snapshotPath);
            statusMessage.append(QStringLiteral("；抓拍关联失败"));
        }
    }
    updateAttendanceStatus(statusMessage);
}

void FaceRecognitionWin::timerEvent(QTimerEvent *)
{
    if (!mVideoSource || !mVideoSource->read(videoImage)) {
        mRecognitionInputActive = false;
        if (timerid != 0) {
            killTimer(timerid);
            timerid = 0;
        }
        updateVideoSourceStatus();
        mRecognitionRequestPending = false;
        ++mRecognitionRequestId;
        mPendingRecognitionFrame.release();
        mTrackerRequestPending = false;
        ++mTrackerRequestId;
        mPendingTrackerFrame.release();
        mAttendanceStateMachine.reset();
        updateAttendanceStatus(QStringLiteral("视频输入已停止"));
        return;
    }

    {
        //把Mat数据转换为RGB
        cv::Mat rgbImage;
        cv::cvtColor(videoImage,rgbImage,cv::COLOR_BGR2RGB);
        //把Mat数据转QImage
        QImage image(rgbImage.data,rgbImage.cols,rgbImage.rows,rgbImage.step,QImage::Format_RGB888);
        //在Qt中显示
        ui->videoLb->setPixmap(QPixmap::fromImage(image).scaled(ui->videoLb->size(),
                                                                  Qt::KeepAspectRatio,
                                                                  Qt::SmoothTransformation));

        if (!mTrackerRequestPending && !mRecognitionRequestPending) {
            mTrackerRequestPending = true;
            ++mTrackerRequestId;
            mPendingTrackerFrame = videoImage.clone();
            emit sendTrackerCmd(mPendingTrackerFrame, mTrackerRequestId);
        }
    }
}

FaceRecognitionWin::~FaceRecognitionWin()
{
    stopVideoSource();
    mthread->quit();
    mthread->wait(3000);
    delete ui;
}

void FaceRecognitionWin::on_openVideoBt_clicked()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                           QStringLiteral("打开本地视频文件"),
                                                           QString(),
                                                           QStringLiteral("视频文件 (*.mp4 *.avi *.mkv *.mov *.wmv);;所有文件 (*.*)"));
    if (!filePath.isEmpty()) {
        openVideoFile(filePath);
    }
}

void FaceRecognitionWin::on_stopVideoBt_clicked()
{
    stopVideoSource();
    updateVideoSourceStatus();
}

void FaceRecognitionWin::openVideoFile(const QString &filePath)
{
    stopVideoSource();

    VideoFileSource *videoFileSource = dynamic_cast<VideoFileSource *>(mVideoSource.get());
    if (videoFileSource) {
        videoFileSource->setLoopEnabled(AppConfig::localVideoLoopEnabled());
    }

    QString errorMessage;
    if (!mVideoSource->open(filePath, &errorMessage)) {
        updateVideoSourceStatus();
        QMessageBox::warning(this, QStringLiteral("打开视频失败"), errorMessage);
        return;
    }

    mRecognitionInputActive = true;
    timerid = startTimer(33);
    updateVideoSourceStatus();
}

void FaceRecognitionWin::stopVideoSource()
{
    mRecognitionInputActive = false;
    mRecognitionRequestPending = false;
    ++mRecognitionRequestId;
    mPendingRecognitionFrame.release();
    mTrackerRequestPending = false;
    ++mTrackerRequestId;
    mPendingTrackerFrame.release();
    mAttendanceStateMachine.reset();
    if (timerid != 0) {
        killTimer(timerid);
        timerid = 0;
    }
    if (mVideoSource) {
        mVideoSource->close();
    }
}

void FaceRecognitionWin::updateAttendanceStatus(const QString &message, bool failed)
{
    ui->attendanceStatusLb->setText(QStringLiteral("考勤状态：%1").arg(message));
    ui->attendanceStatusLb->setStyleSheet(failed
                                          ? QStringLiteral("color: rgb(180, 30, 30);")
                                          : QStringLiteral("color: rgb(30, 90, 30);"));
}

void FaceRecognitionWin::updateVideoSourceStatus()
{
    if (!mVideoSource) {
        ui->videoStatusLb->setText(QStringLiteral("视频状态：未初始化"));
        return;
    }

    QString status = QStringLiteral("视频状态：%1").arg(IVideoSource::stateText(mVideoSource->state()));
    if (mVideoSource->state() == VideoSourceState::Error && !mVideoSource->lastError().isEmpty()) {
        status.append(QStringLiteral("（%1）").arg(mVideoSource->lastError()));
    } else if (mVideoSource->state() == VideoSourceState::Playing && !mVideoSource->displayName().isEmpty()) {
        status.append(QStringLiteral("：%1").arg(mVideoSource->displayName()));
    }
    ui->videoStatusLb->setText(status);
}

void FaceRecognitionWin::showUnknownPerson()
{
    ui->RnumberLb->setText("--");
    ui->RnameLb->setText("陌生人");
    ui->RpartmentLb->setText("未通过识别阈值");
    ui->RtimeLb->setText(QTime::currentTime().toString("hh:mm:ss"));
}

void FaceRecognitionWin::on_recognitionRb_clicked()
{
    if(win==nullptr) //判断是否显示其他两个界面
    {
        return ; //说明当前显示的就是
    }else
    {
        ui->recognitionWidget->show();//显示识别界面
        if(win->inherits("QRegisterWidget"))
        {
            QRegisterWidget *twin = (QRegisterWidget*)this->win;
            disconnect(twin, &QRegisterWidget::requestPhotoCapture,
                       this, &FaceRecognitionWin::recvName);
            disconnect(this, &FaceRecognitionWin::registrationPhotoCaptured,
                       twin, &QRegisterWidget::handlePhotoCaptureResult);
        }
        delete win; //删除其他界面（注册，查询）
        win = nullptr; //指向nullptr为了是后面判断
    }
}

void FaceRecognitionWin::on_registerRb_clicked()
{
    if(this->win != nullptr) //判断注册， 查询界面是否创建，如果创建就销毁
    {
        if(this->win->inherits("QRegisterWidget"))
        {
            QRegisterWidget *twin = (QRegisterWidget*)this->win;
            disconnect(twin, &QRegisterWidget::requestPhotoCapture,
                       this, &FaceRecognitionWin::recvName);
            disconnect(this, &FaceRecognitionWin::registrationPhotoCaptured,
                       twin, &QRegisterWidget::handlePhotoCaptureResult);
        }
        delete this->win;
        this->win = nullptr;
    }
    QRegisterWidget *rwin = new  QRegisterWidget(this);//创建注册界面
    rwin->setFaceObject(&mfaceObject);
    connect(rwin, &QRegisterWidget::requestPhotoCapture,
            this, &FaceRecognitionWin::recvName);
    connect(this, &FaceRecognitionWin::registrationPhotoCaptured,
            rwin, &QRegisterWidget::handlePhotoCaptureResult);
    this->win = rwin;
    rwin->setGeometry(ui->recognitionWidget->geometry());//设置显示位置
    ui->recognitionWidget->hide();//隐藏识别界面
    rwin->show();//显示注册界面
}

void FaceRecognitionWin::recvName(const QString &name)
{
    captureRegistrationPhoto(name);
}

void FaceRecognitionWin::captureRegistrationPhoto(const QString &photoPath)
{
    if (!mRecognitionInputActive || videoImage.empty()) {
        emit registrationPhotoCaptured(false, QStringLiteral("视频已停止，请重新打开视频后再拍照"));
        return;
    }
    if (!cv::imwrite(photoPath.toUtf8().constData(), videoImage)) {
        emit registrationPhotoCaptured(false, QStringLiteral("无法写入注册照片"));
        return;
    }
    emit registrationPhotoCaptured(true, QStringLiteral("拍照成功"));
}

void FaceRecognitionWin::recvTrackerResult(bool hasSingleFace, quint64 requestId)
{
    if (!mRecognitionInputActive || requestId != mTrackerRequestId) {
        return;
    }
    mTrackerRequestPending = false;
    if (!hasSingleFace) {
        mAttendanceStateMachine.reset();
        updateAttendanceStatus(QStringLiteral("等待单人正脸"));
        return;
    }
    if (mRecognitionRequestPending || mPendingTrackerFrame.empty()) {
        return;
    }

    mRecognitionRequestPending = true;
    ++mRecognitionRequestId;
    mPendingRecognitionFrame = mPendingTrackerFrame.clone();
    emit sendQueryCmd(mPendingRecognitionFrame, mRecognitionRequestId);
}

void FaceRecognitionWin::on_queryRb_clicked()
{
    if(win != nullptr)//判断注册， 查询界面是否创建，如果创建就销毁
    {
        if(win->inherits("QRegisterWidget"))
        {
            QRegisterWidget *twin = (QRegisterWidget*)this->win;
            disconnect(twin, &QRegisterWidget::requestPhotoCapture,
                       this, &FaceRecognitionWin::recvName);
            disconnect(this, &FaceRecognitionWin::registrationPhotoCaptured,
                       twin, &QRegisterWidget::handlePhotoCaptureResult);
        }
        delete win;
        win = nullptr;
    }
    win = new  QqueryWidget(this);//创建查询界面
    win->setGeometry(ui->recognitionWidget->geometry());//设置显示位置
    ui->recognitionWidget->hide();//隐藏识别界面
    win->show();//显示查询界面
}
