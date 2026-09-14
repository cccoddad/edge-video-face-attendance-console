#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <opencv.hpp>
#include <opencv2/videoio.hpp>
#include <QualityAssessor.h>
#include <FaceLandmarker.h>
#include <FaceDetector.h>
#include <Struct.h>
#include "../src/vision/facequalitypolicy.h"

static std::string g_modelRoot;
static int g_failures = 0;

static std::string modelPath(const char *name)
{
    return g_modelRoot + "/" + name;
}

static std::string fixturePath(int argc, char *argv[])
{
    if (argc >= 3 && argv[2][0]) {
        return argv[2];
    }
    const char *env = std::getenv("FACE_ATTENDANCE_DATA_DIR");
    if (env && env[0]) {
        return std::string(env) + "/test-media/local-face-fixture.avi";
    }
    return "runtime-data/test-media/local-face-fixture.avi";
}

static void expect(bool condition, const char *message)
{
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

// --- 策略单元测试：纯逻辑，不依赖模型和测试视频 ---
static void testQualityPolicy()
{
    using seeta::v2::QualityAssessor;
    const int ok = QualityAssessor::ERROR_OK;
    const int lightness = QualityAssessor::ERROR_LIGHTNESS;
    const int size = QualityAssessor::ERROR_FACE_SIZE;
    const int pose = QualityAssessor::ERROR_FACE_POSE;
    const int clarity = QualityAssessor::ERROR_CLARITY;

    // 识别流程忽略姿态告警
    expect(FaceQualityPolicy::passesRecognition(ok), "recognition should pass clean frame");
    expect(FaceQualityPolicy::passesRecognition(pose), "recognition should ignore pose warning");
    expect(!FaceQualityPolicy::passesRecognition(lightness), "recognition should block lightness");
    expect(!FaceQualityPolicy::passesRecognition(size), "recognition should block small face");
    expect(!FaceQualityPolicy::passesRecognition(clarity), "recognition should block blur");
    expect(!FaceQualityPolicy::passesRecognition(pose | clarity),
           "recognition should block combined pose+blur");

    // 注册流程要求全部质量项通过
    expect(FaceQualityPolicy::passesRegistration(ok), "registration should pass clean frame");
    expect(!FaceQualityPolicy::passesRegistration(pose), "registration should block pose");
    expect(!FaceQualityPolicy::passesRegistration(lightness | pose | clarity),
           "registration should block combined failures");

    // 失败描述文本
    expect(FaceQualityPolicy::failureText(ok).isEmpty(), "ok code should have empty failure text");
    expect(FaceQualityPolicy::failureText(lightness | pose) == QStringLiteral("亮度异常、姿态偏转"),
           "failure text should list lightness and pose");
    expect(FaceQualityPolicy::failureText(clarity | size) == QStringLiteral("人脸过小、清晰度不足"),
           "failure text should list size and clarity");

    std::printf("policy tests done\n");
}

struct BaselineFrame {
    cv::Mat image;
    SeetaRect face;
    SeetaPointF points[5];
    float score = 0.0f;
    int qualityCode = -1;
    bool valid = false;
};

static bool loadBaselineFromVideo(const std::string &path, seeta::v2::FaceDetector &detector,
                                  seeta::v2::FaceLandmarker &landmarker,
                                  seeta::v2::QualityAssessor &assessor,
                                  BaselineFrame &baseline)
{
    cv::VideoCapture capture(path);
    if (!capture.isOpened()) {
        std::printf("diagnostic: cannot open video: %s\n", path.c_str());
        return false;
    }

    cv::Mat frame;
    int inspected = 0;
    while (capture.read(frame) && inspected < 200) {
        ++inspected;
        if (frame.empty()) {
            continue;
        }
        SeetaImageData data;
        data.data = frame.data;
        data.width = frame.cols;
        data.height = frame.rows;
        data.channels = frame.channels();

        SeetaFaceInfoArray faces = detector.detect(data);
        if (faces.size != 1) {
            continue;
        }

        const SeetaRect face = faces.data[0].pos;
        SeetaPointF points[5];
        landmarker.mark(data, face, points);
        float score = 0;
        const int code = assessor.evaluate(data, face, points, score);

        baseline.image = frame.clone();
        baseline.face = face;
        std::memcpy(baseline.points, points, sizeof(points));
        baseline.score = score;
        baseline.qualityCode = code;
        baseline.valid = true;
        std::printf("baseline frame after %d frames: face=(%d,%d,%d,%d) score=%.3f quality=0x%02x\n",
                    inspected, face.x, face.y, face.width, face.height, score, code);
        return true;
    }
    std::printf("diagnostic: inspected %d frames without a single-face frame\n", inspected);
    return false;
}

static SeetaImageData toSeetaData(const cv::Mat &image)
{
    SeetaImageData data;
    data.data = const_cast<uchar *>(image.data);
    data.width = image.cols;
    data.height = image.rows;
    data.channels = image.channels();
    return data;
}

int main(int argc, char *argv[])
{
    if (argc >= 2 && argv[1][0]) {
        g_modelRoot = argv[1];
    } else {
        const char *env = std::getenv("FACE_ATTENDANCE_MODEL_DIR");
        g_modelRoot = env && env[0] ? env : "models";
    }

    // 策略单元测试始终执行
    testQualityPolicy();

    const std::string fdModel = modelPath("fd_2_00.dat");
    const std::string pdModel = modelPath("pd_2_00_pts5.dat");

    seeta::ModelSetting fdSetting(fdModel, seeta::ModelSetting::CPU, 0);
    seeta::ModelSetting pdSetting(pdModel, seeta::ModelSetting::CPU, 0);

    seeta::v2::FaceDetector detector(fdSetting);
    seeta::v2::FaceLandmarker landmarker(pdSetting);
    seeta::v2::QualityAssessor assessor;

    const std::string fixture = fixturePath(argc, argv);
    BaselineFrame baseline;
    if (!loadBaselineFromVideo(fixture, detector, landmarker, assessor, baseline)) {
        std::printf("SKIP: no usable face fixture at '%s'\n", fixture.c_str());
        std::printf("      set FACE_ATTENDANCE_DATA_DIR or pass a face video as argv[2]\n");
        if (g_failures != 0) {
            std::printf("\nFaceQualityAssessorTest FAILED with %d policy failure(s).\n", g_failures);
            return 1;
        }
        std::printf("FaceQualityAssessorTest passed (policy only, fixture skipped).\n");
        return 0;
    }

    expect(baseline.score >= 0.0f, "baseline score should be non-negative");
    expect(baseline.qualityCode != -1, "baseline quality code should be evaluated");

    // 真实人脸基线应至少通过识别策略（允许姿态告警）
    {
        const bool recognitionOk = FaceQualityPolicy::passesRecognition(baseline.qualityCode);
        std::printf("baseline recognition policy: %s (quality=0x%02x)\n",
                    recognitionOk ? "PASS" : "BLOCKED", baseline.qualityCode);
        expect(recognitionOk, "real face baseline should pass recognition policy");
    }

    // --- 暗光帧：期望亮度告警 ---
    {
        cv::Mat dark = baseline.image * 0.03;
        SeetaImageData data = toSeetaData(dark);
        float score = 0;
        const int code = assessor.evaluate(data, baseline.face, baseline.points, score);
        const bool flagged = (code & seeta::v2::QualityAssessor::ERROR_LIGHTNESS) != 0;
        std::printf("dark  : code=0x%02x score=%.3f lightness=%s\n",
                    code, score, flagged ? "YES" : "NO");
        expect(flagged, "dark frame should flag lightness");
        expect(!FaceQualityPolicy::passesRecognition(code),
               "dark frame should be blocked by recognition policy");
    }

    // --- 过亮帧：期望亮度告警 ---
    {
        cv::Mat bright;
        cv::add(baseline.image, cv::Scalar(230, 230, 230), bright);
        SeetaImageData data = toSeetaData(bright);
        float score = 0;
        const int code = assessor.evaluate(data, baseline.face, baseline.points, score);
        const bool flagged = (code & seeta::v2::QualityAssessor::ERROR_LIGHTNESS) != 0;
        std::printf("bright: code=0x%02x score=%.3f lightness=%s\n",
                    code, score, flagged ? "YES" : "NO");
        expect(flagged, "bright frame should flag lightness");
        expect(!FaceQualityPolicy::passesRecognition(code),
               "bright frame should be blocked by recognition policy");
    }

    // --- 模糊帧：期望清晰度告警 ---
    {
        cv::Mat blurred;
        cv::GaussianBlur(baseline.image, blurred, cv::Size(31, 31), 0);
        SeetaImageData data = toSeetaData(blurred);
        float score = 0;
        const int code = assessor.evaluate(data, baseline.face, baseline.points, score);
        const bool flagged = (code & seeta::v2::QualityAssessor::ERROR_CLARITY) != 0;
        std::printf("blur  : code=0x%02x score=%.3f clarity=%s\n",
                    code, score, flagged ? "YES" : "NO");
        expect(flagged, "blurred frame should flag clarity");
        expect(!FaceQualityPolicy::passesRecognition(code),
               "blurred frame should be blocked by recognition policy");
    }

    // --- 小人脸矩形：期望尺寸告警 ---
    {
        SeetaRect tiny = baseline.face;
        tiny.width = 16;
        tiny.height = 16;
        tiny.x = baseline.face.x + baseline.face.width / 2 - 8;
        tiny.y = baseline.face.y + baseline.face.height / 2 - 8;
        SeetaImageData data = toSeetaData(baseline.image);
        float score = 0;
        const int code = assessor.evaluate(data, tiny, baseline.points, score);
        const bool flagged = (code & seeta::v2::QualityAssessor::ERROR_FACE_SIZE) != 0;
        std::printf("size  : code=0x%02x score=%.3f faceSize=%s\n",
                    code, score, flagged ? "YES" : "NO");
        expect(flagged, "tiny face rect should flag face size");
        expect(!FaceQualityPolicy::passesRecognition(code),
               "tiny face should be blocked by recognition policy");
    }

    if (g_failures != 0) {
        std::printf("\nFaceQualityAssessorTest FAILED with %d failure(s).\n", g_failures);
        return 1;
    }
    std::printf("\nFaceQualityAssessorTest passed.\n");
    return 0;
}
