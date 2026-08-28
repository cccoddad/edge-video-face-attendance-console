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

int QFaceObject::registerface(const cv::Mat &faceMat)
{
    if (faceMat.empty()) {
        return -1;
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
        return -1;
    }
    return faceid;
}

//跟踪人脸，当前帧恰好有一张人脸时返回 true
bool QFaceObject::trackerface(const cv::Mat &faceMat)
{
    if (faceMat.empty()) {
        return false;
    }
    SeetaImageData seetaData ;
    seetaData.data = faceMat.data;
    seetaData.width = faceMat.cols;
    seetaData.height = faceMat.rows;
    seetaData.channels = faceMat.channels();
    SeetaTrackingFaceInfoArray faceArray = mfaceTracker->track(seetaData);//跟踪,返回人脸数据
    return faceArray.size == 1;
}
