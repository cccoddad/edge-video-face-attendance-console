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
    //跟踪--当人脸检测到不连续（也就是不同人的时候返回true），如果一值是同一个人就返回false
    bool trackerface(const cv::Mat &faceMat);
signals:
    //当查询到人脸的时候把人脸id和相似度发送出来
    void sendQueryResult(int index, float similarity, quint64 requestId);
protected:
    FaceEngine  *mfaceEngine;
    FaceTracker *mfaceTracker;
    int mLastTrackedPid = -1;
};

#endif // QFACEOBJECT_H
