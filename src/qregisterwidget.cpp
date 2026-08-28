#include "qregisterwidget.h"
#include "appconfig.h"
#include "theme.h"
#include "ui_qregisterwidget.h"

#include <QDateTime>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QSqlQuery>
#include <QStyle>
#include <QVBoxLayout>
#include <QDebug>
#include <QSqlError>
#include <QMetaObject>
#include <QUrl>

namespace {
QString photoPathForNumber(const QString &number)
{
    return QDir(AppConfig::photoDirectory()).filePath(number.toUtf8().toHex() + ".jpg");
}
}

QRegisterWidget::QRegisterWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QRegisterWidget),
    mFaceObject(nullptr),
    m_registrationRequestId(0),
    m_photoCaptured(false),
    m_registrationPending(false)
{
    ui->setupUi(this);
    setupModernLayout();
    AppConfig::photoDirectory();
    const auto invalidatePhoto = [this](const QString &) {
        invalidateCapturedPhoto();
    };
    connect(ui->GnumberLe, &QLineEdit::textChanged, this, invalidatePhoto);
    connect(ui->GnameLe, &QLineEdit::textChanged, this, invalidatePhoto);
    connect(ui->GpartmentLe, &QLineEdit::textChanged, this, invalidatePhoto);
}

QRegisterWidget::~QRegisterWidget()
{
    delete ui;
}

void QRegisterWidget::setFaceObject(QFaceObject *mfaceObject)
{
    mFaceObject = mfaceObject;
    if (!mFaceObject) {
        return;
    }
    connect(this, &QRegisterWidget::requestFaceRegistration, mFaceObject,
            &QFaceObject::registerface, Qt::QueuedConnection);
    connect(this, &QRegisterWidget::requestFaceDeletion, mFaceObject,
            &QFaceObject::deleteface, Qt::QueuedConnection);
    connect(mFaceObject, &QFaceObject::sendRegistrationResult, this,
            &QRegisterWidget::handleRegistrationResult);
}

void QRegisterWidget::setupModernLayout()
{
    setObjectName(QStringLiteral("QRegisterWidget"));
    ui->GheadLb->setStyleSheet(QString());
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);

    ui->GtitleLb->setObjectName(QStringLiteral("pageTitle"));
    ui->GtitleLb->setText(QStringLiteral("登记人员"));
    layout->addWidget(ui->GtitleLb);

    ui->GheadLb->setObjectName(QStringLiteral("avatar"));
    ui->GheadLb->setFont(font());
    ui->GheadLb->setFixedSize(128, 128);
    ui->GheadLb->setText(QStringLiteral("待拍照"));
    ui->GheadLb->setAlignment(Qt::AlignCenter);
    ui->GheadLb->style()->unpolish(ui->GheadLb);
    ui->GheadLb->style()->polish(ui->GheadLb);
    auto *avatarRow = new QHBoxLayout;
    avatarRow->addStretch();
    avatarRow->addWidget(ui->GheadLb);
    avatarRow->addStretch();
    layout->addLayout(avatarRow);

    auto *form = new QGridLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    form->addWidget(new QLabel(QStringLiteral("员工编号"), this), 0, 0);
    form->addWidget(ui->GnumberLe, 0, 1);
    form->addWidget(new QLabel(QStringLiteral("姓名"), this), 1, 0);
    form->addWidget(ui->GnameLe, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("部门"), this), 2, 0);
    form->addWidget(ui->GpartmentLe, 2, 1);
    form->setColumnStretch(1, 1);
    layout->addLayout(form);

    ui->GcameraBt->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->GcameraBt->setToolTip(QStringLiteral("保存当前视频帧作为注册头像"));
    ui->GregisterBt->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    ui->GregisterBt->setToolTip(QStringLiteral("保存人员信息和人脸特征"));
    ui->GregisterBt->setObjectName(QStringLiteral("primaryAction"));
    auto *actionRow = new QHBoxLayout;
    actionRow->addWidget(ui->GcameraBt);
    actionRow->addWidget(ui->GregisterBt);
    layout->addLayout(actionRow);
    layout->addStretch();
}

void QRegisterWidget::on_GcameraBt_clicked()
{
    //检测信息是否输入
    if(ui->GnameLe->text().isEmpty() ||
       ui->GnumberLe->text().isEmpty()||
       ui->GpartmentLe->text().isEmpty())
    {
        QMessageBox::warning(this,"输入提示","请输入员工信息");
        return ;
    }
    m_photoPath = photoPathForNumber(ui->GnumberLe->text());
    m_photoCaptured = false;
    ui->GcameraBt->setText(QStringLiteral("正在保存当前视频帧..."));
    ui->GcameraBt->setEnabled(false);
    emit requestPhotoCapture(m_photoPath);
}

void QRegisterWidget::on_GregisterBt_clicked()
{

    if (!mFaceObject) {
        QMessageBox::warning(this, "注册提示", "人脸识别服务未初始化");
        return;
    }

    if (!m_photoCaptured || m_photoPath != photoPathForNumber(ui->GnumberLe->text())) {
        QMessageBox::warning(this, "注册提示", "请先拍照，再执行注册");
        return;
    }
    const cv::Mat faceImage = cv::imread(m_photoPath.toUtf8().constData());
    if (faceImage.empty()) {
        m_photoCaptured = false;
        QMessageBox::warning(this, "注册提示", "拍照文件无法读取，请重新拍照");
        return;
    }

    m_registrationPending = true;
    ui->GregisterBt->setEnabled(false);
    ui->GregisterBt->setText(QStringLiteral("正在注册..."));
    ++m_registrationRequestId;
    emit requestFaceRegistration(faceImage, m_registrationRequestId);
}

void QRegisterWidget::handlePhotoCaptureResult(bool success, const QString &message)
{
    if (!success) {
        m_photoCaptured = false;
        ui->GcameraBt->setEnabled(true);
        ui->GcameraBt->setText(QStringLiteral("拍照"));
        QMessageBox::warning(this, QStringLiteral("拍照失败"), message);
        return;
    }

    m_photoCaptured = true;
    ui->GcameraBt->setEnabled(true);
    ui->GcameraBt->setText(QStringLiteral("已拍照，可注册"));
    updatePhotoPreview();
}

void QRegisterWidget::updatePhotoPreview()
{
    const QPixmap photo(m_photoPath);
    if (photo.isNull()) {
        ui->GheadLb->setPixmap(QPixmap());
        ui->GheadLb->setText(QStringLiteral("照片不可用"));
        return;
    }

    ui->GheadLb->setText(QString());
    ui->GheadLb->setPixmap(Theme::circularAvatar(photo, ui->GheadLb->width()));
}

void QRegisterWidget::invalidateCapturedPhoto()
{
    if (!m_photoCaptured) {
        return;
    }

    m_photoCaptured = false;
    m_photoPath.clear();
    ui->GcameraBt->setText(QStringLiteral("拍照"));
    ui->GheadLb->setPixmap(QPixmap());
    ui->GheadLb->setText(QStringLiteral("待拍照"));
}

void QRegisterWidget::handleRegistrationResult(int faceid, quint64 requestId,
                                                const QString &errorMessage)
{
    if (requestId != m_registrationRequestId) {
        return;
    }
    m_registrationPending = false;
    ui->GregisterBt->setEnabled(true);
    ui->GregisterBt->setText(QStringLiteral("注册"));
    if (faceid < 0) {
        QMessageBox::warning(this, QStringLiteral("注册提示"),
                             QStringLiteral("注册失败：%1").arg(errorMessage));
        return;
    }

    QSqlDatabase database = QSqlDatabase::database();
    if (!database.transaction()) {
        emit requestFaceDeletion(faceid);
        QMessageBox::warning(this, "注册提示", "无法开始人员数据事务");
        return;
    }
    QSqlQuery query;
    query.prepare("INSERT INTO user(number, name, partment, faceid, facepictrue, entertime) "
                  "VALUES(?, ?, ?, ?, ?, ?)");
    query.addBindValue(ui->GnumberLe->text());
    query.addBindValue(ui->GnameLe->text());
    query.addBindValue(ui->GpartmentLe->text());
    query.addBindValue(faceid);
    query.addBindValue(m_photoPath);
    query.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    if (!query.exec() || !database.commit())
    {
        database.rollback();
        qDebug()<<query.lastError().text();
        QMessageBox::warning(this,"注册提示","注册失败");
        emit requestFaceDeletion(faceid);
    }else
    {
        QMessageBox::warning(this,"注册提示","注册成功");
    }
}
