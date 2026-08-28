#include "qfaceobject.h"
#include "appconfig.h"

#include <QDebug>
#include <QFileInfo>

QFaceObject::QFaceObject(QObject *parent) : QObject(parent)
{
    //人脸检测模型
    const ModelSetting FDSetting(AppConfig::modelPath("fd_2_00.dat").toStdString(), seeta::ModelSetting::CPU, 0);
    //人脸标注模型
    const ModelSetting FLMSetting(AppConfig::modelPath("pd_2_00_pts5.dat").toStdString(), seeta::ModelSetting::CPU, 0);
    //人脸匹配模型（数据库）
    const ModelSetting FDBSetting(AppConfig::modelPath("fr_2_10.dat").toStdString(), seeta::ModelSetting::CPU, 0);
    mfaceEngine = new FaceEngine(FDSetting,FLMSetting, FDBSetting);
    const QString faceDatabasePath = AppConfig::faceDatabasePath();
    if (QFileInfo::exists(faceDatabasePath) && !mfaceEngine->Load(faceDatabasePath.toUtf8().constData()))
    {
       qWarning() << "load face database error:" << faceDatabasePath;
    }
    mfaceTracker = new FaceTracker(FDSetting);
}

QFaceObject::~QFaceObject()
{
    delete mfaceEngine;
    delete mfaceTracker;
}

void QFaceObject::load(QString dbstr)
{
    mfaceEngine->Load(dbstr.toUtf8().data());
}

void QFaceObject::save(QString dbstr)
{
    mfaceEngine->Save(dbstr.toUtf8().data());
}

bool QFaceObject::delID(int faceid)
{
    return mfaceEngine->Delete(faceid) > 0
            && mfaceEngine->Save(AppConfig::faceDatabasePath().toUtf8().constData());
}

void QFaceObject::queryface(const cv::Mat &faceMat, quint64 requestId)
{
    if (faceMat.empty()) {
        emit sendQueryResult(-1, 0.0f, requestId);
        return;
    }
    //保存查询到的faceid
    int64_t index = 0;
    //保存传入的人脸与当前id对应的人脸相似度
    float similarity = 0;
    //定义一个seeta的数据结构
    SeetaImageData seetaData ;
    seetaData.data = faceMat.data;
    seetaData.width = faceMat.cols;
    seetaData.height = faceMat.rows;
    seetaData.channels = faceMat.channels();

    //去人脸数据库里面查询
    int ret = mfaceEngine->QueryTop(seetaData,1,&index,&similarity);
    if(ret <= 0)
    {
        qDebug()<<"query error";
        emit sendQueryResult(-1, 0.0f, requestId);
    }else{
        //查询成功把人脸id和相似度通过信号出去
        emit sendQueryResult(static_cast<int>(index), similarity, requestId);
    }
}

void QFaceObject::registerface(const cv::Mat &faceMat, quint64 requestId)
{
    if (faceMat.empty()) {
        emit sendRegistrationResult(-1, requestId, QStringLiteral("注册照片为空"));
        return;
    }
    //定义一个seeta的数据结构
    SeetaImageData seetaData ;
    seetaData.data = faceMat.data;
    seetaData.width = faceMat.cols;
    seetaData.height = faceMat.rows;
    seetaData.channels = faceMat.channels();
    //把人脸注册到人脸数据库中
    int faceid = mfaceEngine->Register(seetaData);
    if (faceid < 0 || !mfaceEngine->Save(AppConfig::faceDatabasePath().toUtf8().constData())) {
        if (faceid >= 0) {
            mfaceEngine->Delete(faceid);
        }
        emit sendRegistrationResult(-1, requestId, QStringLiteral("人脸特征注册或保存失败"));
        return;
    }
    emit sendRegistrationResult(faceid, requestId, QString());
}

void QFaceObject::deleteface(int faceid)
{
    if (!delID(faceid)) {
        qWarning() << "delete face registration failed:" << faceid;
    }
}

//跟踪人脸，当前帧恰好检测到一张人脸时发送 true
void QFaceObject::trackerface(const cv::Mat &faceMat, quint64 requestId)
{
    if (faceMat.empty()) {
        emit sendTrackerResult(false, QRect(), requestId);
        return;
    }
    SeetaImageData seetaData ;
    seetaData.data = faceMat.data;
    seetaData.width = faceMat.cols;
    seetaData.height = faceMat.rows;
    seetaData.channels = faceMat.channels();
    SeetaTrackingFaceInfoArray faceArray = mfaceTracker->track(seetaData);//跟踪,返回人脸数据
    QRect faceRect;
    if (faceArray.size == 1) {
        const SeetaRect &position = faceArray.data[0].pos;
        faceRect = QRect(position.x, position.y, position.width, position.height);
    }
    emit sendTrackerResult(faceArray.size == 1, faceRect, requestId);
}
