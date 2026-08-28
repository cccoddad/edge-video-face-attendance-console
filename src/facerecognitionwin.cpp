#include "facerecognitionwin.h"
#include "qquerywidget.h"
#include "qregisterwidget.h"
#include "appconfig.h"
#include "snapshotstore.h"
#include "localcamerasource.h"
#include "theme.h"
#include "videofilesource.h"
#include "ui_facerecognitionwin.h"
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
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
       mAttendanceRepository(QSqlDatabase::database()),
       mVideoSourceType(QStringLiteral("video-file")),
       mModeTabs(nullptr),
       mRecognitionTab(nullptr),
       mRegisterTab(nullptr),
       mQueryTab(nullptr)
       , mCheckoutBt(nullptr)
{
    ui->setupUi(this);
    setupModernLayout();
    ui->videoLb->setAlignment(Qt::AlignCenter);
    ui->videoLb->setText(QStringLiteral("请选择本地视频文件"));
    ui->RnumberLb->setText(QStringLiteral("--"));
    ui->RnameLb->setText(QStringLiteral("未识别"));
    ui->RpartmentLb->setText(QStringLiteral("等待本地视频"));
    ui->RtimeLb->setText(QStringLiteral("--:--:--"));
    setRecognitionAvatar(QString());
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

void FaceRecognitionWin::setupModernLayout()
{
    setWindowTitle(QStringLiteral("人脸考勤控制台"));
    setMinimumSize(1100, 640);
    resize(1280, 720);

    auto *centralLayout = new QHBoxLayout(ui->centralwidget);
    centralLayout->setContentsMargins(20, 20, 20, 20);
    centralLayout->setSpacing(18);

    ui->videoWidget->setObjectName(QStringLiteral("mediaCard"));
    ui->recognitionWidget->setObjectName(QStringLiteral("recognitionPage"));
    ui->videoWidget->setStyleSheet(QString());
    ui->recognitionWidget->setStyleSheet(QString());
    ui->videoLb->setStyleSheet(QString());
    ui->RheadLb->setStyleSheet(QString());
    ui->recognitionRb->hide();
    ui->registerRb->hide();
    ui->queryRb->hide();

    auto *videoLayout = new QVBoxLayout(ui->videoWidget);
    videoLayout->setContentsMargins(20, 20, 20, 20);
    videoLayout->setSpacing(12);
    auto *mediaTitle = new QLabel(QStringLiteral("实时视频输入"), ui->videoWidget);
    mediaTitle->setObjectName(QStringLiteral("pageTitle"));
    videoLayout->addWidget(mediaTitle);
    ui->videoLb->setObjectName(QStringLiteral("videoLb"));
    ui->videoLb->setMinimumSize(560, 400);
    ui->videoLb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoLayout->addWidget(ui->videoLb, 1);
    ui->videoStatusLb->setObjectName(QStringLiteral("videoStatusLb"));
    videoLayout->addWidget(ui->videoStatusLb);

    ui->openVideoBt->setText(QStringLiteral("视频文件"));
    ui->openVideoBt->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->openVideoBt->setToolTip(QStringLiteral("选择本地视频文件作为输入"));
    ui->openLocalCameraBt->setText(QStringLiteral("本机摄像头"));
    ui->openLocalCameraBt->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    ui->openLocalCameraBt->setToolTip(QStringLiteral("打开 Windows 本机摄像头进行独立开发测试"));
    ui->stopVideoBt->setText(QStringLiteral("停止"));
    ui->stopVideoBt->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    ui->stopVideoBt->setToolTip(QStringLiteral("停止当前视频输入并暂停识别"));
    auto *mediaActions = new QHBoxLayout;
    mediaActions->addWidget(ui->openVideoBt);
    mediaActions->addWidget(ui->openLocalCameraBt);
    mediaActions->addStretch();
    mediaActions->addWidget(ui->stopVideoBt);
    videoLayout->addLayout(mediaActions);

    mModeTabs = new QTabWidget(ui->centralwidget);
    mModeTabs->setDocumentMode(true);
    mRecognitionTab = new QWidget(mModeTabs);
    mRegisterTab = new QWidget(mModeTabs);
    mQueryTab = new QWidget(mModeTabs);
    auto *recognitionLayout = new QVBoxLayout(mRecognitionTab);
    recognitionLayout->setContentsMargins(0, 12, 0, 0);
    recognitionLayout->addWidget(ui->recognitionWidget);
    mModeTabs->addTab(mRecognitionTab, QStringLiteral("识别"));
    mModeTabs->addTab(mRegisterTab, QStringLiteral("注册"));
    mModeTabs->addTab(mQueryTab, QStringLiteral("查询"));

    auto *resultLayout = new QVBoxLayout(ui->recognitionWidget);
    resultLayout->setContentsMargins(24, 24, 24, 24);
    resultLayout->setSpacing(12);
    ui->RtitleLb->setObjectName(QStringLiteral("pageTitle"));
    ui->RtitleLb->setFont(font());
    ui->RtitleLb->setText(QStringLiteral("识别结果"));
    resultLayout->addWidget(ui->RtitleLb);
    ui->RheadLb->setObjectName(QStringLiteral("avatar"));
    ui->RheadLb->setFont(font());
    ui->RheadLb->setFixedSize(128, 128);
    ui->RheadLb->setAlignment(Qt::AlignCenter);
    ui->RheadLb->style()->unpolish(ui->RheadLb);
    ui->RheadLb->style()->polish(ui->RheadLb);
    auto *avatarRow = new QHBoxLayout;
    avatarRow->addStretch();
    avatarRow->addWidget(ui->RheadLb);
    avatarRow->addStretch();
    resultLayout->addLayout(avatarRow);
    ui->RnameLb->setObjectName(QStringLiteral("sectionTitle"));
    ui->RnameLb->setFont(font());
    ui->RnameLb->setAlignment(Qt::AlignCenter);
    resultLayout->addWidget(ui->RnameLb);
    auto *details = new QGridLayout;
    details->setHorizontalSpacing(12);
    details->setVerticalSpacing(8);
    details->addWidget(new QLabel(QStringLiteral("员工编号"), ui->recognitionWidget), 0, 0);
    details->addWidget(ui->RnumberLb, 0, 1);
    details->addWidget(new QLabel(QStringLiteral("部门"), ui->recognitionWidget), 1, 0);
    details->addWidget(ui->RpartmentLb, 1, 1);
    details->addWidget(new QLabel(QStringLiteral("识别时间"), ui->recognitionWidget), 2, 0);
    details->addWidget(ui->RtimeLb, 2, 1);
    details->setColumnStretch(1, 1);
    resultLayout->addLayout(details);
    ui->RnumberLb->setWordWrap(true);
    ui->RnumberLb->setFont(font());
    ui->RpartmentLb->setWordWrap(true);
    ui->RpartmentLb->setFont(font());
    ui->RtimeLb->setWordWrap(true);
    ui->RtimeLb->setFont(font());
    ui->attendanceStatusLb->setObjectName(QStringLiteral("attendanceStatusLb"));
    resultLayout->addWidget(ui->attendanceStatusLb);
    mCheckoutBt = new QPushButton(QStringLiteral("签退（确认 3 秒）"), ui->recognitionWidget);
    mCheckoutBt->setObjectName(QStringLiteral("primaryAction"));
    mCheckoutBt->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    mCheckoutBt->setToolTip(QStringLiteral("对当前已识别人员发起连续 3 秒正脸签退确认"));
    resultLayout->addWidget(mCheckoutBt);
    connect(mCheckoutBt, &QPushButton::clicked, this,
            &FaceRecognitionWin::on_requestCheckout_clicked);
    resultLayout->addStretch();

    centralLayout->addWidget(ui->videoWidget, 7);
    centralLayout->addWidget(mModeTabs, 5);
    connect(mModeTabs, &QTabWidget::currentChanged, this,
            &FaceRecognitionWin::on_modeTabs_currentChanged);
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
        resetCheckoutConfirmation(QStringLiteral("签退确认已中断：未通过识别"));
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
        resetCheckoutConfirmation(QStringLiteral("签退确认已中断：查询人员信息失败"));
        updateAttendanceStatus(QStringLiteral("查询人员信息失败"), true);
        showUnknownPerson();
        return;
    }
    //查询到用户
    //获取，工号number，name，partment，facepictrue，显示界面上
    const QString number = query.value("number").toString();
    const QString name = query.value("name").toString();
    ui->RnumberLb->setText(number);
    ui->RnameLb->setText(name);
    ui->RpartmentLb->setText(query.value("partment").toString());
    setRecognitionAvatar(query.value("facepictrue").toString());
    const QDateTime now = QDateTime::currentDateTime();
    ui->RtimeLb->setText(now.toString("hh:mm:ss"));
    mLastRecognizedNumber = number;
    mTrackedFaceLabel = QStringLiteral("%1  %2").arg(name, number);
    updateMediaControls();

    if (mCheckoutConfirmation.isActive()) {
        if (mCheckoutConfirmation.number() != number) {
            resetCheckoutConfirmation(QStringLiteral("签退确认已中断：人员不一致"));
            return;
        }
        if (!mCheckoutConfirmation.observe(number, now)) {
            const int elapsed = mCheckoutConfirmation.elapsedMilliseconds(now);
            const int seconds = qMin(3, qMax(1, (elapsed + 999) / 1000));
            updateAttendanceStatus(QStringLiteral("签退确认中：%1/3 秒").arg(seconds));
            return;
        }

        AttendanceConfirmation checkoutConfirmation;
        checkoutConfirmation.number = number;
        checkoutConfirmation.similarity = similarty;
        checkoutConfirmation.timestamp = now;
        const AttendanceWriteResult checkoutResult = mAttendanceRepository.recordCheckOut(
                    checkoutConfirmation, mVideoSourceType);
        resetCheckoutConfirmation();
        finishAttendanceWrite(checkoutResult, checkoutConfirmation);
        return;
    }

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
                confirmation, AppConfig::minimumCheckoutIntervalSeconds(), mVideoSourceType);
    finishAttendanceWrite(writeResult, confirmation);
}

void FaceRecognitionWin::finishAttendanceWrite(const AttendanceWriteResult &writeResult,
                                               const AttendanceConfirmation &confirmation)
{
    if (writeResult.status == AttendanceWriteStatus::Failed) {
        qWarning() << "attendance write failed:" << writeResult.message;
        updateAttendanceStatus(writeResult.message, true);
        return;
    }
    mLastAttendanceConfirmationByNumber.insert(confirmation.number, confirmation.timestamp);
    QString statusMessage = writeResult.message;
    if (writeResult.status == AttendanceWriteStatus::Inserted) {
        QString snapshotPath;
        QString snapshotError;
        if (!SnapshotStore::save(mPendingRecognitionFrame, confirmation.number, confirmation.timestamp,
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
        mTrackedFaceRect = QRect();
        mTrackedFaceLabel.clear();
        mAttendanceStateMachine.reset();
        resetCheckoutConfirmation();
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
        QPixmap displayPixmap = QPixmap::fromImage(image).scaled(ui->videoLb->size(),
                                                                   Qt::KeepAspectRatio,
                                                                   Qt::FastTransformation);
        updateFaceOverlay(&displayPixmap, image.size());
        ui->videoLb->setPixmap(displayPixmap);

        const bool recognitionIntervalElapsed = !mRecognitionDispatchTimer.isValid()
                || mRecognitionDispatchTimer.elapsed() >= AppConfig::recognitionIntervalMilliseconds();
        if (recognitionIntervalElapsed && !mTrackerRequestPending && !mRecognitionRequestPending) {
            mRecognitionDispatchTimer.restart();
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
    clearSidePage();
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

void FaceRecognitionWin::on_modeTabs_currentChanged(int index)
{
    if (index == 0) {
        showRecognitionPage();
    } else if (index == 1) {
        showRegisterPage();
    } else if (index == 2) {
        showQueryPage();
    }
}

void FaceRecognitionWin::on_openLocalCameraBt_clicked()
{
    openLocalCamera();
}

void FaceRecognitionWin::openVideoFile(const QString &filePath)
{
    stopVideoSource();

    std::unique_ptr<VideoFileSource> videoFileSource(new VideoFileSource);
    videoFileSource->setLoopEnabled(AppConfig::localVideoLoopEnabled());
    mVideoSource = std::move(videoFileSource);
    mVideoSourceType = QStringLiteral("video-file");

    QString errorMessage;
    if (!mVideoSource->open(filePath, &errorMessage)) {
        updateVideoSourceStatus();
        QMessageBox::warning(this, QStringLiteral("打开视频失败"), errorMessage);
        return;
    }

    mRecognitionInputActive = true;
    timerid = startTimer(40);
    updateVideoSourceStatus();
}

void FaceRecognitionWin::openLocalCamera()
{
    stopVideoSource();

    mVideoSource.reset(new LocalCameraSource);
    mVideoSourceType = QStringLiteral("local-camera");
    const QString cameraIndex = QString::number(AppConfig::localCameraIndex());
    QString errorMessage;
    if (!mVideoSource->open(cameraIndex, &errorMessage)) {
        updateVideoSourceStatus();
        QMessageBox::warning(this, QStringLiteral("打开本机摄像头失败"), errorMessage);
        return;
    }

    mRecognitionInputActive = true;
    timerid = startTimer(40);
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
    mTrackedFaceRect = QRect();
    mTrackedFaceLabel.clear();
    mAttendanceStateMachine.reset();
    mLastRecognizedNumber.clear();
    resetCheckoutConfirmation();
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
    ui->attendanceStatusLb->setProperty("failed", failed);
    ui->attendanceStatusLb->style()->unpolish(ui->attendanceStatusLb);
    ui->attendanceStatusLb->style()->polish(ui->attendanceStatusLb);
}

void FaceRecognitionWin::updateVideoSourceStatus()
{
    if (!mVideoSource) {
        ui->videoStatusLb->setText(QStringLiteral("视频状态：未初始化"));
        updateMediaControls();
        return;
    }

    QString status = QStringLiteral("视频状态：%1").arg(IVideoSource::stateText(mVideoSource->state()));
    if (mVideoSource->state() == VideoSourceState::Error && !mVideoSource->lastError().isEmpty()) {
        status.append(QStringLiteral("（%1）").arg(mVideoSource->lastError()));
    } else if (mVideoSource->state() == VideoSourceState::Playing && !mVideoSource->displayName().isEmpty()) {
        status.append(QStringLiteral("：%1").arg(mVideoSource->displayName()));
    }
    ui->videoStatusLb->setText(status);
    updateMediaControls();
}

void FaceRecognitionWin::updateMediaControls()
{
    const bool isPlaying = mVideoSource && mVideoSource->state() == VideoSourceState::Playing;
    ui->openVideoBt->setEnabled(!isPlaying);
    ui->openLocalCameraBt->setEnabled(!isPlaying);
    ui->stopVideoBt->setEnabled(isPlaying);
    if (mCheckoutBt) {
        mCheckoutBt->setEnabled(isPlaying && !mLastRecognizedNumber.isEmpty()
                                && !mCheckoutConfirmation.isActive());
    }
}

void FaceRecognitionWin::updateFaceOverlay(QPixmap *pixmap, const QSize &sourceSize) const
{
    if (!pixmap || pixmap->isNull() || mTrackedFaceRect.isEmpty() || sourceSize.isEmpty()) {
        return;
    }

    const qreal scale = qMin(static_cast<qreal>(pixmap->width()) / sourceSize.width(),
                             static_cast<qreal>(pixmap->height()) / sourceSize.height());
    const qreal offsetX = (pixmap->width() - sourceSize.width() * scale) / 2.0;
    const qreal offsetY = (pixmap->height() - sourceSize.height() * scale) / 2.0;
    QRect faceRect(qRound(offsetX + mTrackedFaceRect.x() * scale),
                   qRound(offsetY + mTrackedFaceRect.y() * scale),
                   qRound(mTrackedFaceRect.width() * scale),
                   qRound(mTrackedFaceRect.height() * scale));
    faceRect = faceRect.intersected(pixmap->rect());
    if (faceRect.isEmpty()) {
        return;
    }

    const QString label = mTrackedFaceLabel.isEmpty()
            ? QStringLiteral("正在识别") : mTrackedFaceLabel;
    QPainter painter(pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor boxColor(QStringLiteral("#7ee6a2"));
    painter.setPen(QPen(boxColor, 3));
    painter.drawRect(faceRect);
    QFont labelFont = painter.font();
    labelFont.setPointSize(qMax(9, labelFont.pointSize()));
    painter.setFont(labelFont);
    const QFontMetrics metrics(labelFont);
    const QRect textRect = metrics.boundingRect(label).adjusted(-8, -4, 8, 4);
    QRect labelRect(faceRect.left(), faceRect.top() - textRect.height(), textRect.width(), textRect.height());
    if (labelRect.top() < 0) {
        labelRect.moveTop(faceRect.top());
    }
    if (labelRect.right() > pixmap->width()) {
        labelRect.moveRight(pixmap->width());
    }
    painter.fillRect(labelRect, boxColor);
    painter.setPen(QColor(QStringLiteral("#173b29")));
    painter.drawText(labelRect, Qt::AlignCenter, label);
}

void FaceRecognitionWin::on_requestCheckout_clicked()
{
    if (!mRecognitionInputActive || mLastRecognizedNumber.isEmpty()) {
        updateAttendanceStatus(QStringLiteral("请先保持一名已登记人员在画面中"), true);
        return;
    }
    mCheckoutConfirmation.start(mLastRecognizedNumber, QDateTime::currentDateTime());
    updateAttendanceStatus(QStringLiteral("签退确认已开始，请保持正脸 3 秒"));
    updateMediaControls();
}

void FaceRecognitionWin::resetCheckoutConfirmation(const QString &message)
{
    const bool wasActive = mCheckoutConfirmation.isActive();
    mCheckoutConfirmation.reset();
    if (wasActive && !message.isEmpty()) {
        updateAttendanceStatus(message);
    }
    updateMediaControls();
}

void FaceRecognitionWin::showUnknownPerson()
{
    ui->RnumberLb->setText("--");
    ui->RnameLb->setText("陌生人");
    ui->RpartmentLb->setText("未通过识别阈值");
    ui->RtimeLb->setText(QTime::currentTime().toString("hh:mm:ss"));
    mLastRecognizedNumber.clear();
    mTrackedFaceLabel.clear();
    updateMediaControls();
    setRecognitionAvatar(QString());
}

void FaceRecognitionWin::setRecognitionAvatar(const QString &photoPath)
{
    const QPixmap photo(photoPath);
    if (photo.isNull()) {
        ui->RheadLb->setPixmap(QPixmap());
        ui->RheadLb->setText(QStringLiteral("未识别"));
        return;
    }

    ui->RheadLb->setText(QString());
    ui->RheadLb->setPixmap(Theme::circularAvatar(photo, ui->RheadLb->width()));
}

void FaceRecognitionWin::on_recognitionRb_clicked()
{
    if (mModeTabs) {
        mModeTabs->setCurrentIndex(0);
        showRecognitionPage();
    }
}

void FaceRecognitionWin::on_registerRb_clicked()
{
    if (mModeTabs) {
        mModeTabs->setCurrentIndex(1);
        showRegisterPage();
    }
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

void FaceRecognitionWin::recvTrackerResult(bool hasSingleFace, const QRect &faceRect, quint64 requestId)
{
    if (!mRecognitionInputActive || requestId != mTrackerRequestId) {
        return;
    }
    mTrackerRequestPending = false;
    if (!hasSingleFace) {
        mTrackedFaceRect = QRect();
        mTrackedFaceLabel.clear();
        mAttendanceStateMachine.reset();
        resetCheckoutConfirmation(QStringLiteral("签退确认已中断：请保持单人正脸"));
        updateAttendanceStatus(QStringLiteral("等待单人正脸"));
        return;
    }
    mTrackedFaceRect = faceRect;
    if (mTrackedFaceLabel.isEmpty()) {
        mTrackedFaceLabel = QStringLiteral("正在识别");
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
    if (mModeTabs) {
        mModeTabs->setCurrentIndex(2);
        showQueryPage();
    }
}

void FaceRecognitionWin::showRecognitionPage()
{
    clearSidePage();
}

void FaceRecognitionWin::showRegisterPage()
{
    if (win && win->inherits("QRegisterWidget")) {
        return;
    }

    clearSidePage();
    auto *layout = new QVBoxLayout(mRegisterTab);
    layout->setContentsMargins(0, 12, 0, 0);
    QRegisterWidget *registerWidget = new QRegisterWidget(mRegisterTab);
    registerWidget->setFaceObject(&mfaceObject);
    connect(registerWidget, &QRegisterWidget::requestPhotoCapture,
            this, &FaceRecognitionWin::recvName);
    connect(this, &FaceRecognitionWin::registrationPhotoCaptured,
            registerWidget, &QRegisterWidget::handlePhotoCaptureResult);
    layout->addWidget(registerWidget);
    win = registerWidget;
}

void FaceRecognitionWin::showQueryPage()
{
    if (win && win->inherits("QqueryWidget")) {
        return;
    }

    clearSidePage();
    auto *layout = new QVBoxLayout(mQueryTab);
    layout->setContentsMargins(0, 12, 0, 0);
    win = new QqueryWidget(mQueryTab);
    layout->addWidget(win);
}

void FaceRecognitionWin::clearSidePage()
{
    if (win) {
        delete win;
        win = nullptr;
    }
    if (mRegisterTab && mRegisterTab->layout()) {
        delete mRegisterTab->layout();
    }
    if (mQueryTab && mQueryTab->layout()) {
        delete mQueryTab->layout();
    }
}
