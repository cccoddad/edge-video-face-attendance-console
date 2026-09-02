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
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QScrollBar>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyle>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
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
       mLastPerformanceSampleMilliseconds(0),
       mFramesRead(0),
       mRecognitionRequests(0),
       mRecognitionResults(0),
       mRecognitionLatencyTotalMilliseconds(0),
       mRecognitionLatencyMaximumMilliseconds(0),
       mAttendanceInserted(0),
       mAttendanceSuppressed(0),
       mAttendanceFailed(0),
       mthread(new QThread(this)),
       mAttendanceStateMachine(AppConfig::recognitionConfirmationFrames()),
       mAttendanceRepository(QSqlDatabase::database()),
       mVideoSourceType(QStringLiteral("video-file")),
       mRtspConfiguration(AppConfig::rtspUrl(), AppConfig::rtspReconnectIntervalMilliseconds()),
       mVideoSourceRuntimeLog(50),
       mModeTabs(nullptr),
       mRecognitionTab(nullptr),
       mRegisterTab(nullptr),
       mQueryTab(nullptr)
       , mCheckoutBt(nullptr)
       , mConfigureRtspBt(nullptr)
       , mSourceEventView(nullptr)
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
    appendVideoSourceEvent(QStringLiteral("等待选择视频输入"));
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
    initializePerformanceLog();

    const QString automaticVideoPath = AppConfig::automaticVideoPath();
    if (!automaticVideoPath.isEmpty()) {
        QTimer::singleShot(0, this, [this, automaticVideoPath]() {
            openVideoFile(automaticVideoPath);
        });
    } else if (AppConfig::automaticLocalCameraEnabled()) {
        QTimer::singleShot(0, this, [this]() {
            openLocalCamera();
        });
    }
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
    mConfigureRtspBt = new QPushButton(QStringLiteral("RTSP 配置"), ui->videoWidget);
    mConfigureRtspBt->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    mConfigureRtspBt->setToolTip(QStringLiteral("校验 RTSP 地址和重连等待时间；不会发起网络连接"));
    mediaActions->addWidget(mConfigureRtspBt);
    mediaActions->addStretch();
    mediaActions->addWidget(ui->stopVideoBt);
    videoLayout->addLayout(mediaActions);
    connect(mConfigureRtspBt, &QPushButton::clicked, this,
            &FaceRecognitionWin::on_configureRtspBt_clicked);
    auto *sourceEventTitle = new QLabel(QStringLiteral("运行事件"), ui->videoWidget);
    sourceEventTitle->setObjectName(QStringLiteral("sourceEventTitle"));
    videoLayout->addWidget(sourceEventTitle);
    mSourceEventView = new QPlainTextEdit(ui->videoWidget);
    mSourceEventView->setObjectName(QStringLiteral("sourceEventView"));
    mSourceEventView->setReadOnly(true);
    mSourceEventView->setUndoRedoEnabled(false);
    mSourceEventView->setMaximumBlockCount(50);
    mSourceEventView->setFixedHeight(112);
    videoLayout->addWidget(mSourceEventView);
    refreshVideoSourceEventView();

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
    const bool hasRequestStart = mRecognitionRequestStartMilliseconds.contains(requestId);
    const qint64 requestStartedAt = mRecognitionRequestStartMilliseconds.take(requestId);
    if (hasRequestStart && mPerformanceTimer.isValid()) {
        const qint64 latencyMilliseconds = mPerformanceTimer.elapsed() - requestStartedAt;
        ++mRecognitionResults;
        mRecognitionLatencyTotalMilliseconds += latencyMilliseconds;
        mRecognitionLatencyMaximumMilliseconds = qMax(mRecognitionLatencyMaximumMilliseconds,
                                                       latencyMilliseconds);
    }
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
    recordAttendanceWriteResult(writeResult.status);
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
        if (mVideoSource && mVideoSource->state() == VideoSourceState::Playing) {
            return;
        }
        mRecognitionInputActive = false;
        writePerformanceSample(true);
        if (timerid != 0) {
            killTimer(timerid);
            timerid = 0;
        }
        updateVideoSourceStatus();
        appendVideoSourceEvent(QStringLiteral("读取已停止"));
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

    ++mFramesRead;
    writePerformanceSample();

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
    writePerformanceSample(true);
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

void FaceRecognitionWin::on_configureRtspBt_clicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("RTSP 配置（未连接）"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *formLayout = new QFormLayout;
    auto *urlEdit = new QLineEdit(mRtspConfiguration.url(), &dialog);
    urlEdit->setPlaceholderText(QStringLiteral("rtsp://<host>:8554/live"));
    urlEdit->setClearButtonEnabled(true);
    urlEdit->setToolTip(QStringLiteral("仅校验地址格式，不会建立网络连接"));
    auto *reconnectSpin = new QSpinBox(&dialog);
    reconnectSpin->setRange(500, 60000);
    reconnectSpin->setSingleStep(500);
    reconnectSpin->setSuffix(QStringLiteral(" ms"));
    reconnectSpin->setValue(mRtspConfiguration.reconnectIntervalMilliseconds());
    formLayout->addRow(QStringLiteral("RTSP 地址"), urlEdit);
    formLayout->addRow(QStringLiteral("重连等待"), reconnectSpin);
    layout->addLayout(formLayout);
    auto *validationLabel = new QLabel(&dialog);
    validationLabel->setWordWrap(true);
    layout->addWidget(validationLabel);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    const auto validate = [urlEdit, validationLabel, buttons]() {
        QString errorMessage;
        RtspConfiguration configuration;
        const bool valid = configuration.setUrl(urlEdit->text(), &errorMessage);
        validationLabel->setText(valid
                                 ? QStringLiteral("地址格式有效；保存仅更新当前程序会话，不会连接或探测网络。")
                                 : errorMessage);
        buttons->button(QDialogButtonBox::Save)->setEnabled(valid);
    };
    connect(urlEdit, &QLineEdit::textChanged, &dialog, validate);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    validate();

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString errorMessage;
    RtspConfiguration configuration;
    if (!configuration.setUrl(urlEdit->text(), &errorMessage)
            || !configuration.setReconnectIntervalMilliseconds(reconnectSpin->value(), &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("RTSP 配置无效"), errorMessage);
        return;
    }
    mRtspConfiguration = configuration;
    appendRuntimeEvent(QStringLiteral("rtsp"), VideoSourceState::Closed,
                       QStringLiteral("地址格式校验通过，等待明确联调确认"));
    updateAttendanceStatus(QStringLiteral("RTSP 已配置，尚未连接"));
}

void FaceRecognitionWin::openVideoFile(const QString &filePath)
{
    stopVideoSource();

    std::unique_ptr<VideoFileSource> videoFileSource(new VideoFileSource);
    videoFileSource->setLoopEnabled(AppConfig::localVideoLoopEnabled());
    mVideoSource = std::move(videoFileSource);
    mVideoSourceType = QStringLiteral("video-file");
    appendVideoSourceEvent(QStringLiteral("准备打开：%1").arg(QFileInfo(filePath).fileName()));

    QString errorMessage;
    if (!mVideoSource->open(filePath, &errorMessage)) {
        appendVideoSourceEvent(QStringLiteral("打开失败：%1").arg(errorMessage));
        updateVideoSourceStatus();
        writePerformanceSample(true);
        QMessageBox::warning(this, QStringLiteral("打开视频失败"), errorMessage);
        return;
    }

    mRecognitionInputActive = true;
    timerid = startTimer(40);
    appendVideoSourceEvent(QStringLiteral("已开始读取视频帧"));
    updateVideoSourceStatus();
    writePerformanceSample(true);
}

void FaceRecognitionWin::openLocalCamera()
{
    stopVideoSource();

    mVideoSource.reset(new LocalCameraSource);
    mVideoSourceType = QStringLiteral("local-camera");
    const QString cameraIndex = QString::number(AppConfig::localCameraIndex());
    appendVideoSourceEvent(QStringLiteral("准备打开摄像头 #%1").arg(cameraIndex));
    QString errorMessage;
    if (!mVideoSource->open(cameraIndex, &errorMessage)) {
        appendVideoSourceEvent(QStringLiteral("打开失败：%1").arg(errorMessage));
        updateVideoSourceStatus();
        writePerformanceSample(true);
        QMessageBox::warning(this, QStringLiteral("打开本机摄像头失败"), errorMessage);
        return;
    }

    mRecognitionInputActive = true;
    timerid = startTimer(40);
    appendVideoSourceEvent(QStringLiteral("已开始读取视频帧"));
    updateVideoSourceStatus();
    writePerformanceSample(true);
}

void FaceRecognitionWin::stopVideoSource()
{
    const bool hadActiveSource = mVideoSource
            && mVideoSource->state() != VideoSourceState::Closed
            && mVideoSource->state() != VideoSourceState::Stopped;
    mRecognitionInputActive = false;
    mRecognitionRequestPending = false;
    ++mRecognitionRequestId;
    mPendingRecognitionFrame.release();
    mRecognitionRequestStartMilliseconds.clear();
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
    if (hadActiveSource) {
        appendVideoSourceEvent(QStringLiteral("已停止当前视频输入"));
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

void FaceRecognitionWin::appendVideoSourceEvent(const QString &detail)
{
    if (!mVideoSource) {
        return;
    }
    appendRuntimeEvent(mVideoSourceType, mVideoSource->state(), detail);
}

void FaceRecognitionWin::appendRuntimeEvent(const QString &sourceType, VideoSourceState state,
                                            const QString &detail)
{
    mVideoSourceRuntimeLog.record(sourceType, state, detail);
    refreshVideoSourceEventView();
}

void FaceRecognitionWin::refreshVideoSourceEventView()
{
    if (!mSourceEventView) {
        return;
    }
    QStringList lines;
    const QList<VideoSourceRuntimeEvent> events = mVideoSourceRuntimeLog.events();
    for (const VideoSourceRuntimeEvent &event : events) {
        lines.append(VideoSourceRuntimeLog::formatEvent(event));
    }
    mSourceEventView->setPlainText(lines.join(QLatin1Char('\n')));
    QScrollBar *scrollBar = mSourceEventView->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
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
    if (mPerformanceTimer.isValid()) {
        mRecognitionRequestStartMilliseconds.insert(mRecognitionRequestId, mPerformanceTimer.elapsed());
    }
    ++mRecognitionRequests;
    emit sendQueryCmd(mPendingRecognitionFrame, mRecognitionRequestId);
}

void FaceRecognitionWin::initializePerformanceLog()
{
    const QString path = AppConfig::performanceLogPath();
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        qWarning() << "cannot create performance log directory:" << fileInfo.absolutePath();
        return;
    }
    mPerformanceLog.setFileName(path);
    if (!mPerformanceLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "cannot open performance log:" << path << mPerformanceLog.errorString();
        return;
    }

    mPerformanceLog.write("timestamp,elapsed_seconds,frames_read,recognition_requests,recognition_results,"
                          "average_recognition_latency_ms,max_recognition_latency_ms,attendance_inserted,"
                          "attendance_suppressed,attendance_failed,source_state,source_error\n");
    mPerformanceLog.flush();
    mPerformanceTimer.start();
}

void FaceRecognitionWin::writePerformanceSample(bool force)
{
    if (!mPerformanceLog.isOpen() || !mPerformanceTimer.isValid()) {
        return;
    }

    const qint64 elapsedMilliseconds = mPerformanceTimer.elapsed();
    if (!force && elapsedMilliseconds - mLastPerformanceSampleMilliseconds
            < AppConfig::performanceLogIntervalMilliseconds()) {
        return;
    }

    const double averageLatency = mRecognitionResults == 0 ? 0.0
            : static_cast<double>(mRecognitionLatencyTotalMilliseconds) / mRecognitionResults;
    const QString sourceState = mVideoSource
            ? IVideoSource::stateText(mVideoSource->state()).replace(',', QStringLiteral(" "))
            : QStringLiteral("未初始化");
    QString sourceError = mVideoSource ? mVideoSource->lastError() : QString();
    sourceError.replace(',', QStringLiteral(" "));
    sourceError.replace('\r', QStringLiteral(" "));
    sourceError.replace('\n', QStringLiteral(" "));
    const QString line = QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
            .arg(elapsedMilliseconds / 1000.0, 0, 'f', 3)
            .arg(mFramesRead)
            .arg(mRecognitionRequests)
            .arg(mRecognitionResults)
            .arg(averageLatency, 0, 'f', 3)
            .arg(mRecognitionLatencyMaximumMilliseconds)
            .arg(mAttendanceInserted)
            .arg(mAttendanceSuppressed)
            .arg(mAttendanceFailed)
            .arg(sourceState)
            .arg(sourceError);
    mPerformanceLog.write(line.toUtf8());
    mPerformanceLog.flush();
    mLastPerformanceSampleMilliseconds = elapsedMilliseconds;
}

void FaceRecognitionWin::recordAttendanceWriteResult(AttendanceWriteStatus status)
{
    if (status == AttendanceWriteStatus::Inserted) {
        ++mAttendanceInserted;
    } else if (status == AttendanceWriteStatus::Suppressed) {
        ++mAttendanceSuppressed;
    } else {
        ++mAttendanceFailed;
    }
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
