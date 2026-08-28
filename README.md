# Edge Video Face Attendance Console

基于 **Qt 5、OpenCV 和 SeetaFace** 的 Windows 人脸考勤控制台。项目定位为 RK3568 音视频边缘网关的下游只读业务客户端：网关负责媒体采集与上游视频处理，控制台在 Windows PC 上完成人脸匹配、考勤记录和人员查询。

## 当前能力

- Qt 5.12.0 + MinGW 7.3 64 位构建基线。
- OpenCV 4.5.2 与 SeetaFace2 本地人脸检测、特征提取和相似度匹配。
- 可配置的模型目录、应用数据目录、相似度阈值和考勤冷却时间。
- SQLite 参数化写入与事务基础，避免连续帧重复考勤。
- 连续帧确认、当天签到/签退状态与数据库幂等键，包含按人员的识别冷却。
- `IVideoSource` 与 `VideoFileSource` 本地视频输入；可从 Qt 界面选择视频文件进行回归测试。
- 考勤记录按工号、日期和签到/签退状态筛选，并将当前筛选结果导出为 UTF-8 CSV。
- Windows 独立打包脚本，包含 Qt、OpenCV、SeetaFace、SQLite 驱动和模型依赖。

## 开发中能力

- `RtspSource` 只读客户端与断流自动重连。
- 抓拍与隐私数据治理。

> RK3568 网关当前尚未正式提供并验收 RTSP 输出。本项目不会操作网关硬件、摄像头、麦克风、NPU、服务、端口或板端文件；RTSP 联调将在网关提供验收通过的地址后进行。

## 技术边界

- 人脸检测、特征匹配和考勤决策只在 Windows PC 端运行。
- 摄像头、麦克风、媒体编码和板端 RKNN 推理由 RK3568 网关独占。
- Qt 客户端未来仅主动读取可配置 RTSP 地址，预期接口为 `rtsp://<rk3568-ip>:8554/live`；不在 Windows 上启动 RTSP 服务端或监听 8554 端口。
- SQLite、模型、抓拍、报表和日志保存在 Windows 本地应用数据目录，不与网关共享。

## 构建环境

- Qt 5.12.0，MinGW 7.3 64 位
- OpenCV 4.5.2
- SeetaFace2
- Windows 10/11

第三方 SDK、模型、运行数据库、人员照片、抓拍图、构建产物和发布 DLL 不纳入仓库。复制 `src/third_party.pri.example` 为 `src/third_party.pri` 后，按本机路径填写 `THIRD_PARTY_ROOT`。

## 文档

详细实施记录、问题复盘、面试材料、升级方案和 RK3568 协作边界见 [doc](doc/00-文档目录.md)。

## 项目状态

这是持续开发中的学习与工程化升级项目。README 只描述已实现或已验证的能力；未完成的 RTSP 联调不会表述为已完成成果。
