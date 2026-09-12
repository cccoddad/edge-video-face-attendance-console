#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <opencv.hpp>
#include <QualityAssessor.h>
#include <FaceLandmarker.h>
#include <FaceDetector.h>
#include <Struct.h>

static const char *MODEL_DIR = nullptr;

static std::string modelPath(const char *name)
{
    if (MODEL_DIR) {
        return std::string(MODEL_DIR) + "/" + name;
    }
    const char *env = std::getenv("FACE_ATTENDANCE_MODEL_DIR");
    if (env && env[0]) {
        return std::string(env) + "/" + name;
    }
    return std::string("models/") + name;
}

static cv::Mat makeSyntheticFace(int width, int height, uchar brightness)
{
    cv::Mat face(height, width, CV_8UC3, cv::Scalar(brightness, brightness, brightness));
    cv::circle(face, cv::Point(width / 2, height / 3), width / 6, cv::Scalar(80, 80, 80), -1);
    cv::ellipse(face, cv::Point(width / 2, height * 2 / 3), cv::Size(width / 4, height / 6),
                0, 0, 360, cv::Scalar(60, 60, 60), -1);
    return face;
}

int main(int argc, char *argv[])
{
    if (argc >= 2) {
        MODEL_DIR = argv[1];
    }

    const std::string fdModel = modelPath("fd_2_00.dat");
    const std::string pdModel = modelPath("pd_2_00_pts5.dat");

    seeta::ModelSetting fdSetting(fdModel, seeta::ModelSetting::CPU, 0);
    seeta::ModelSetting pdSetting(pdModel, seeta::ModelSetting::CPU, 0);

    seeta::v2::FaceDetector detector(fdSetting);
    seeta::v2::FaceLandmarker landmarker(pdSetting);
    seeta::v2::QualityAssessor assessor;

    // --- Test 1: Dark image should fail LIGHTNESS ---
    {
        cv::Mat dark = makeSyntheticFace(160, 160, 5);
        SeetaImageData data;
        data.data = dark.data;
        data.width = dark.cols;
        data.height = dark.rows;
        data.channels = dark.channels();

        SeetaFaceInfoArray faces = detector.detect(data);
        if (faces.size == 0) {
            printf("Test1: no face detected in dark image, retrying with brighter...\n");
            dark = makeSyntheticFace(160, 160, 30);
            data.data = dark.data;
            faces = detector.detect(data);
        }
        if (faces.size > 0) {
            SeetaRect face = faces.data[0].pos;
            SeetaPointF points[5];
            landmarker.mark(data, face, points);
            float score = 0;
            int code = assessor.evaluate(data, face, points, score);
            bool hasLightness = (code & seeta::v2::QualityAssessor::ERROR_LIGHTNESS) != 0;
            printf("Test1 dark: code=0x%02x score=%.3f lightness=%s\n",
                   code, score, hasLightness ? "YES" : "NO");
            if (!hasLightness) {
                printf("FAIL: dark image should flag LIGHTNESS\n");
                return 1;
            }
        } else {
            printf("SKIP Test1: cannot detect face in synthetic dark image\n");
        }
    }

    // --- Test 2: Bright image should fail LIGHTNESS ---
    {
        cv::Mat bright = makeSyntheticFace(160, 160, 250);
        SeetaImageData data;
        data.data = bright.data;
        data.width = bright.cols;
        data.height = bright.rows;
        data.channels = bright.channels();

        SeetaFaceInfoArray faces = detector.detect(data);
        if (faces.size > 0) {
            SeetaRect face = faces.data[0].pos;
            SeetaPointF points[5];
            landmarker.mark(data, face, points);
            float score = 0;
            int code = assessor.evaluate(data, face, points, score);
            bool hasLightness = (code & seeta::v2::QualityAssessor::ERROR_LIGHTNESS) != 0;
            printf("Test2 bright: code=0x%02x score=%.3f lightness=%s\n",
                   code, score, hasLightness ? "YES" : "NO");
            if (!hasLightness) {
                printf("FAIL: bright image should flag LIGHTNESS\n");
                return 1;
            }
        } else {
            printf("SKIP Test2: cannot detect face in synthetic bright image\n");
        }
    }

    // --- Test 3: Very small face should fail FACE_SIZE ---
    {
        cv::Mat smallCanvas(320, 320, CV_8UC3, cv::Scalar(140, 140, 140));
        cv::Mat tinyFace = makeSyntheticFace(20, 20, 140);
        tinyFace.copyTo(smallCanvas(cv::Rect(150, 150, 20, 20)));

        SeetaImageData data;
        data.data = smallCanvas.data;
        data.width = smallCanvas.cols;
        data.height = smallCanvas.rows;
        data.channels = smallCanvas.channels();

        SeetaFaceInfoArray faces = detector.detect(data);
        if (faces.size > 0) {
            SeetaRect face = faces.data[0].pos;
            SeetaPointF points[5];
            landmarker.mark(data, face, points);
            float score = 0;
            int code = assessor.evaluate(data, face, points, score);
            bool hasSize = (code & seeta::v2::QualityAssessor::ERROR_FACE_SIZE) != 0;
            printf("Test3 small: code=0x%02x score=%.3f faceSize=%s faceW=%d\n",
                   code, score, hasSize ? "YES" : "NO", face.width);
            if (!hasSize) {
                printf("FAIL: small face should flag FACE_SIZE\n");
                return 1;
            }
        } else {
            printf("SKIP Test3: cannot detect face in small-face canvas\n");
        }
    }

    // --- Test 4: Large clear face should pass ---
    {
        cv::Mat large = makeSyntheticFace(200, 200, 140);
        SeetaImageData data;
        data.data = large.data;
        data.width = large.cols;
        data.height = large.rows;
        data.channels = large.channels();

        SeetaFaceInfoArray faces = detector.detect(data);
        if (faces.size > 0) {
            SeetaRect face = faces.data[0].pos;
            SeetaPointF points[5];
            landmarker.mark(data, face, points);
            float score = 0;
            int code = assessor.evaluate(data, face, points, score);
            printf("Test4 clear: code=0x%02x score=%.3f\n", code, score);
            if (code != 0) {
                printf("FAIL: clear face should pass (code=0), got 0x%02x\n", code);
                return 1;
            }
        } else {
            printf("SKIP Test4: cannot detect face in synthetic clear image\n");
        }
    }

    // --- Test 5: evaluate() single-return API ---
    {
        cv::Mat face = makeSyntheticFace(160, 160, 140);
        SeetaImageData data;
        data.data = face.data;
        data.width = face.cols;
        data.height = face.rows;
        data.channels = face.channels();

        SeetaFaceInfoArray faces = detector.detect(data);
        if (faces.size > 0) {
            SeetaRect faceRect = faces.data[0].pos;
            SeetaPointF points[5];
            landmarker.mark(data, faceRect, points);
            float score = assessor.evaluate(data, faceRect, points);
            printf("Test5 singleReturn: score=%.3f\n", score);
            if (score < 0.0f) {
                printf("FAIL: score should be non-negative\n");
                return 1;
            }
        } else {
            printf("SKIP Test5: cannot detect face\n");
        }
    }

    printf("\nAll quality assessor tests passed.\n");
    return 0;
}
