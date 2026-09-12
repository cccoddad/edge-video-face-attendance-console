#include "facequalitypolicy.h"

#include <QStringList>

bool FaceQualityPolicy::passesRecognition(int qualityCode)
{
    const int blockingMask = seeta::v2::QualityAssessor::ERROR_LIGHTNESS
            | seeta::v2::QualityAssessor::ERROR_FACE_SIZE
            | seeta::v2::QualityAssessor::ERROR_CLARITY;
    return (qualityCode & blockingMask) == 0;
}

bool FaceQualityPolicy::passesRegistration(int qualityCode)
{
    return qualityCode == seeta::v2::QualityAssessor::ERROR_OK;
}

QString FaceQualityPolicy::failureText(int qualityCode)
{
    if (qualityCode == seeta::v2::QualityAssessor::ERROR_OK) {
        return QString();
    }
    QStringList failures;
    if (qualityCode & seeta::v2::QualityAssessor::ERROR_LIGHTNESS) {
        failures.append(QStringLiteral("亮度异常"));
    }
    if (qualityCode & seeta::v2::QualityAssessor::ERROR_FACE_SIZE) {
        failures.append(QStringLiteral("人脸过小"));
    }
    if (qualityCode & seeta::v2::QualityAssessor::ERROR_FACE_POSE) {
        failures.append(QStringLiteral("姿态偏转"));
    }
    if (qualityCode & seeta::v2::QualityAssessor::ERROR_CLARITY) {
        failures.append(QStringLiteral("清晰度不足"));
    }
    return failures.join(QStringLiteral("、"));
}
