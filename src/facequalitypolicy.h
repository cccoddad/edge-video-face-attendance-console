#ifndef FACEQUALITYPOLICY_H
#define FACEQUALITYPOLICY_H

#include <QString>
#include <QualityAssessor.h>

// 人脸质量门控策略：
// - 识别流程忽略姿态告警，因为识别模型能容忍轻微歪头，避免正常用户被频繁拦截；
// - 注册流程要求全部质量项通过，特征库只收录高质量正脸照片。
class FaceQualityPolicy
{
public:
    static bool passesRecognition(int qualityCode);
    static bool passesRegistration(int qualityCode);
    static QString failureText(int qualityCode);
};

#endif // FACEQUALITYPOLICY_H
