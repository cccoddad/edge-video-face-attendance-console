#include "qregisterwidget.h"
#include "appconfig.h"
#include "ui_qregisterwidget.h"

#include <QDir>
#include <QMessageBox>
#include <QDateTime>
#include <QSqlQuery>
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
    ui(new Ui::QRegisterWidget)
{
    ui->setupUi(this);
    AppConfig::photoDirectory();
}

QRegisterWidget::~QRegisterWidget()
{
    delete ui;
}

void QRegisterWidget::setFaceObject(QFaceObject *mfaceObject)
{
    this->mFaceObject = mfaceObject;
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
    //获取名字,并且把转十六进制
    const QString name = photoPathForNumber(ui->GnumberLe->text());

    //把name通过信号发送出去
    emit sendName(name);  //照片存储在data目录下

    //把照片显示
    ui->GheadLb->setStyleSheet(QString("border:1px solid #123456; border-image: url(%1); border-radius:80px;")
                               .arg(QUrl::fromLocalFile(name).toString()));
}

void QRegisterWidget::on_GregisterBt_clicked()
{

    if (!mFaceObject) {
        QMessageBox::warning(this, "注册提示", "人脸识别服务未初始化");
        return;
    }

    const QString name = photoPathForNumber(ui->GnumberLe->text());

    //int faceid  = qrand()%100;  //这里的暂时用随机数，后期通过人脸模块得到人脸id
    cv::Mat faceImage = cv::imread(name.toUtf8().data());
    if (faceImage.empty()) {
        QMessageBox::warning(this, "注册提示", "请先拍照，再执行注册");
        return;
    }

    int faceid = -1;
    const bool invoked = QMetaObject::invokeMethod(mFaceObject, "registerface",
                                                    Qt::BlockingQueuedConnection,
                                                    Q_RETURN_ARG(int, faceid),
                                                    Q_ARG(cv::Mat, faceImage));
    if (!invoked) {
        QMessageBox::warning(this, "注册提示", "人脸识别服务调用失败");
        return;
    }
    if(faceid < 0)return ;//注册失败

    QSqlDatabase database = QSqlDatabase::database();
    database.transaction();
    QSqlQuery query;
    query.prepare("INSERT INTO user(number, name, partment, faceid, facepictrue, entertime) "
                  "VALUES(?, ?, ?, ?, ?, ?)");
    query.addBindValue(ui->GnumberLe->text());
    query.addBindValue(ui->GnameLe->text());
    query.addBindValue(ui->GpartmentLe->text());
    query.addBindValue(faceid);
    query.addBindValue(name);
    query.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    if (!query.exec() || !database.commit())
    {
        database.rollback();
        qDebug()<<query.lastError().text();
        QMessageBox::warning(this,"注册提示","注册失败");
        bool removed = false;
        QMetaObject::invokeMethod(mFaceObject, "delID", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(bool, removed), Q_ARG(int, faceid));
    }else
    {
        QMessageBox::warning(this,"注册提示","注册成功");
    }
}
