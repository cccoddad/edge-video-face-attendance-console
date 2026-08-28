#ifndef QFACEOBJECT_H
#define QFACEOBJECT_H

#include <QObject>
#include <FaceEngine.h>
#include <FaceTracker.h>
#include <FaceRecognizer.h>
#include <FaceDetector.h>
#include <FaceDatabase.h>
#include <FaceLandmarker.h>
#include <Struct.h>
#include <opencv.hpp>

Q_DECLARE_METATYPE(cv::Mat)

using namespace  seeta;
class QFaceObject : public QObject
{
    Q_OBJECT
public:
    explicit QFaceObject(QObject *parent = nullptr);
    ~QFaceObject();
    //导入数据
    void load(QString dbstr);
    void save(QString dbstr);
    bool delID(int faceid);
public slots:
    //查询人脸
    void queryface(const cv::Mat &faceMat, quint64 requestId);
    //注册人脸
    int registerface(const cv::Mat &faceMat);
    //跟踪--当前帧恰好检测到一张人脸时返回 true，供连续帧确认逻辑使用
    bool trackerface(const cv::Mat &faceMat);
signals:
    //当查询到人脸的时候把人脸id和相似度发送出来
    void sendQueryResult(int index, float similarity, quint64 requestId);
protected:
    FaceEngine  *mfaceEngine;
    FaceTracker *mfaceTracker;
};

#endif // QFACEOBJECT_H
