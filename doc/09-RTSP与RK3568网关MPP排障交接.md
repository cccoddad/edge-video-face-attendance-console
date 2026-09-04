# RTSP 与 RK3568 网关 MPP 排障交接

**记录日期**：2026-09-04
**交接目的**：保留本轮 Windows Qt 人脸考勤控制台与经用户明确授权的 RK3568 网关排障事实，区分已验证、未验证和当前未完成源码状态。本文不包含网关源码、模型、个人照片、SQLite、日志或构建产物。

## 1. 范围和长期规则

- Qt 仓库：`D:\vs-document\Real-Time Face Recognition Application Based on Qt and OpenCV`，分支 `main`，本记录落库后需推送 `origin/main`。
- 用户于本轮明确授权：可读取、修改、构建和验证指定网关项目；优先修复 MPP H.264，之后实现只读 RTSP 输出；不得删除既有成果。
- 指定网关源码：Ubuntu 主机 `ubuntu-vm` 的 `/home/china/rk3568-work/rkav-mpp-rga-controller-20260902-110614-3688`。该目录**不是 Git 仓库**，不能声称网关源码已提交或推送。
- 开发板：`RK356X`，ADB 地址 `192.168.50.2:5555`，板端网关证据目录为 `/userdata/rkav/mpp-rga-abi-19700101-021545-755`。测试只能新增唯一命名文件，不覆盖该目录已有二进制、配置或日志。
- 每次完整思考、检查、编辑、构建、测试和 Git 操作后，必须立即用中文说明“做了什么、为什么、实际结果、专有名词通俗解释”；未验证必须写明“未验证”。需要用户操作时，给出运行位置和一整段可粘贴命令。

## 2. 本轮主要操作和真实结论

### 2.1 Windows Qt 项目

1. 恢复 Qt 5.12.0 + MinGW 7.3 工具链。项目没有迁移 Qt6；已有 qmake 工程、第三方 DLL 与测试基线继续使用 Qt5。
2. 诊断 Windows `HP 5MP Camera` 黑流。曾观察到 DirectShow 成功协商 `640x480 YUY2` 但 BGR 均值为 `0,0,0`；经 USB 复合设备重枚举后，Windows `MediaCapture` 与 OpenCV 探针都能读到非黑帧。根因未确定，不能归因于外接摄像头、权限或挡片。
3. 完成本机摄像头 30 分钟回归：`1800.342` 秒、`38380` 帧、`21.318 FPS`、识别 `379/379`、平均识别延迟 `13.549 ms`、无运行中源错误；SQLite 审计无重复键。CSV 已由 Excel 只读验证 UTF-8 BOM 中文表头。
4. Qt RTSP 客户端已具备 URL 配置、显式打开入口、脱敏状态、断流暂停考勤和可控替身测试。提交 `7c7a57b` 与 `158ec87` 已在 `origin/main`。它没有连接真实 RTSP，不能称作 RTSP 联调完成。

### 2.2 板端与 RTSP 现状

1. 板端 IP 为 `192.168.50.2`；`ss -lntp` 只有 ADB、DNS 和 SSH，**没有** `554` 或 `8554`。现有 `rk356x-demo` 不是 RTSP 服务。
2. `/etc/init.d/S50launch_demo start` 会额外启动 `rk356x-demo`，不会启动 RTSP。曾启动的第二实例 PID `876` 已被停止，原有实例继续保留；后续不要用该脚本试图“启动 RTSP”。
3. 部署配置确认只有原始 H.264 文件输出，视频为 `/dev/video9`、`1280x720@30`、MJPEG，编码器为 MPP H.264；没有 RTSP output 类型或服务端配置。因此 `rtsp://192.168.50.2:8554/live` 目前不存在。

### 2.3 MPP / RGA 排障进展

1. 初始网关日志：V4L2 和 ALSA 都已打开，但 `mpp_rga_video_encoder.configure_encoder` 报 `MPP rejected H.264 encoder configuration`。
2. 板端官方 `/usr/bin/mpi_enc_test` 已成功将一帧 `1280x720` NV12 编码为 H.264，输出 `458` 字节、退出码 `0`。结论：板端 RKVENC 硬件和官方 MPP 基础能力可用；问题在网关适配而非“硬件完全坏了”。
3. 源码已按官方样例修正：使用 `rc:fps_in_denorm` / `rc:fps_out_denorm`；移除板端不支持的推测性设置；RGA RGB 步长由字节改为像素 `frame.stride / 3`；MPP 缓冲区使用官方实际数值对应的 `MPP_BUFFER_TYPE_DRM`。此前 RGA `Bad address` 已消失，应用能进入运行态。
4. 不能动态加载官方静态样例内部的 `mpp_buffer_sync_begin_f/end_f`：板端动态库未导出这两个符号，已撤销该方案。`MPP_POLL_BLOCK` 曾使队列等待编码输出，因没有输出而挂住；只停止了本轮测试 PID，没有影响既有实例。
5. 最新工作在给每帧绑定 `KEY_OUTPUT_PACKET` 元数据，这是官方样例让编码器写回输出缓冲区的方式。补丁被中断，当前源码不可编译，详见下一节。

## 3. 当前源码精确停留点

文件：`/home/china/rk3568-work/rkav-mpp-rga-controller-20260902-110614-3688/src/media/mpp/mpp_rga_video_encoder.cpp`

已加入且需保留的内容：

- 动态符号：`mpp_frame_get_meta`、`mpp_packet_init_with_buffer`、`mpp_packet_set_length`、`mpp_meta_set_packet`；
- `packet_buffer` 的分配/释放；
- `MPP_BUFFER_TYPE_DRM`、RGA fd 目标缓冲区和像素步长修正；
- 编码提交前创建 `output_packet` 并调用 `meta_set_packet(meta, KEY_OUTPUT_PACKET, output_packet)`。

当前错误位于约 493 行：

```cpp
if (result != MPP_OK) {
    impl_->symbols.packet_deinit(&output_packet);
    impl_->symbols.frame_deinit(&mpp_frame);
    return Result<std::vector<EncodedPacket>>::Failure(
result = impl_->api->encode_put_frame(impl_->context, mpp_frame);
```

这里缺失错误对象和 `}`。恢复时必须先补为：

```cpp
return Result<std::vector<EncodedPacket>>::Failure(
    MppRgaError("prepare_output_packet", result,
                "cannot bind MPP output packet metadata"));
}
```

然后再提交帧。还应审查 `output_packet` 的所有权：成功提交后不要立即释放仍由 MPP 使用的输出包；收到输出或关闭编码器时按 MPP API 的所有权约定回收。这个所有权路径**未验证**。

## 4. 下一步严格顺序

1. 先阅读 `AGENTS.md`、`doc/01-项目实施记录与下一步.md`、本文件、`doc/02-项目问题汇总-通俗版.md` 和 `doc/03-项目问题汇总-面试版.md`，再检查 Qt Git 状态、网关源文件和板端进程/端口。
2. 只修复第 3 节的 C++ 语法缺口，构建网关到新的唯一输出目录；不能修改旧部署二进制，也不能把未完成代码覆盖到旧证据目录。
3. 使用有外层时限的单次网关测试，先验证：进程在预期时长后退出、日志不再出现配置/RGA/提交帧错误、唯一命名 `.h264` 文件非空。把文件拉到 Ubuntu 用解析工具验证 NAL/H.264；此时才能说“网关原始 H.264 输出通过”。
4. H.264 未通过前，禁止写 RTSP 服务端。H.264 通过后再提出 RTSP 设计：协议服务、端口、路径、H.264 参数集、单客户端/多客户端、断流行为、认证和只读访问。新增监听 `8554` 前应先报告影响和验证方案。
5. 网关验收出稳定 URL 后，Qt 侧只用现有显式按钮进行只读拉流，验证首帧、断流、重连、恢复、端到端延迟和“断流期间绝不写考勤”。不要把 Qt 测试视频或个人数据传到板端。
6. Qt 文档或代码每个验证闭环后执行 `git diff`、`git diff --check`、`git status`，确认没有隐私/产物后提交并 `git push origin main`。网关目录没有 Git 时，明确报告这一限制，不能伪造提交号。

## 5. 下一段对话可直接粘贴的提示词

```text
接手 Windows Qt 人脸考勤控制台与经授权的 RK3568 网关 MPP 排障。先在
D:\vs-document\Real-Time Face Recognition Application Based on Qt and OpenCV
执行 git status、当前分支和 git log；完整阅读 README.md、AGENTS.md、doc/00-文档目录.md、doc/01-项目实施记录与下一步.md、doc/02-项目问题汇总-通俗版.md、doc/03-项目问题汇总-面试版.md、doc/05-RK3568协作边界.md、doc/07-本轮开发与演示总结.md、doc/09-RTSP与RK3568网关MPP排障交接.md 和 .cursor/rules/*.mdc。保留已有修改，绝不使用 git reset、git checkout、git clean、强制覆盖或删除既有成果。

永久沟通规则：每完成一次完整的思考、检查、修改、构建、测试或 Git 操作后，必须立即用中文说明：1）做了什么；2）为什么；3）实际证据/结果；4）本次专有名词的一句通俗解释。未验证必须明确写“未验证”。需要我操作时，必须给出运行位置、完整可粘贴命令、前提、目的、预期结果和失败时需返回的完整输出/截图，不能把命令藏在思考里。

Git 规则：每完成一个可验证闭环，依次运行 git diff、git diff --check、git status，确认不含隐私数据、构建产物、模型、DLL、SQLite、个人照片、CSV、视频、日志、dist、build 或 *.pro.user；通过后提交并 push 到 origin/main，禁止 force push。网关源码目录不是 Git 仓库时必须直说，不能虚构提交或推送。

本轮用户明确授权你读取、修改、构建和验证网关项目，只为修复 MPP H.264，随后实现只读 RTSP 输出；不删除或覆盖已有成果。网关源码在 ubuntu-vm:/home/china/rk3568-work/rkav-mpp-rga-controller-20260902-110614-3688，板端 ADB 为 192.168.50.2:5555。当前没有 554/8554 RTSP 监听；不要再用 /etc/init.d/S50launch_demo 试图启动 RTSP，也不要停止既有 rk356x-demo。

先做的唯一代码任务：修复 src/media/mpp/mpp_rga_video_encoder.cpp 约 493 行的未闭合 prepare_output_packet 失败分支。已经添加 packet_buffer、KEY_OUTPUT_PACKET 元数据、MPP_BUFFER_TYPE_DRM、RGA fd 目标和 frame.stride / 3；这些保留。修复编译后，只在新的唯一输出目录构建和做带外层时限的原始 H.264 单次验证。官方 mpi_enc_test 已证明板端硬件能编码一帧，但网关的 H.264 输出仍未通过。原始 H.264 持续输出并解析通过前，不要实现或宣称 RTSP 已完成。

阶段结束请报告：修改文件、验证结果、剩余风险、Git 提交号、推送状态和下一步。
```
