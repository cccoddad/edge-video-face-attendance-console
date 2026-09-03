# Windows 便携包验收清单

## 1. 适用范围

本清单只验证 Windows Qt 人脸考勤控制台的便携包，不访问真实 RTSP、网关端口或 RK3568。模型、SQLite、照片、抓拍、CSV、日志和测试视频不得随公开包分发；三份 SeetaFace 模型属于程序运行依赖，由打包脚本放入 `models`。

## 2. 本机隔离验证

在项目根目录打开 PowerShell，使用唯一输出目录打包并验证：

```powershell
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath 'D:\vs-document\Real-Time Face Recognition Application Based on Qt and OpenCV'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$package = Join-Path $PWD ("dist\FaceAttendance-windows-x64-verify-" + $stamp)
& '.\scripts\package-windows.ps1' -OutputDirectory $package
& '.\scripts\verify-windows-package.ps1' -PackageDirectory $package
```

验证脚本执行以下检查：

1. EXE、Qt、MinGW、OpenCV、SeetaFace、平台插件、SQLite 插件和三份模型均存在。
2. 包内没有 SQLite、CSV、照片、视频、日志、`*.pro.user` 或 `third_party.pri`。
3. 将 `PATH` 缩减为 Windows 系统目录，并清除开发模型目录、自动视频、自动摄像头和 RTSP 环境变量。
4. 使用包外的 Git 忽略目录保存临时数据库；程序运行 3 秒后正常退出。
5. SQLite 审计必须为零事件、零重复键、零缺失键。
6. 在 `runtime-data/package-verification/<时间戳>/` 写入结果和 SHA-256 文件清单。

通过标志是终端同时出现 `Package verification passed` 和 `Package verification evidence`，且命令退出码为 `0`。

## 3. 干净 Windows 机器人工验收

本机隔离验证不能替代一台从未安装 Qt、OpenCV 或 SeetaFace 开发环境的 Windows 10/11 x64 机器。正式交付前还应：

1. 只复制已通过脚本验证的整个包目录，不复制项目源码、`runtime-data` 或开发依赖目录。
2. 在没有配置 `D:\QT`、`D:\qtdeps` 和相关 `PATH` 的普通用户会话中双击 `FaceAttendance.exe`。
3. 确认主窗口正常显示中文，注册、识别、查询页面可切换，启动时没有缺失 DLL、平台插件、SQLite 驱动或模型提示。
4. 关闭程序后确认数据只写到当前 Windows 用户的 `FaceAttendance/data`，没有写回便携包目录。
5. 如该机器有可用摄像头，可做一次本机摄像头打开/停止；这只验证该机器摄像头，不代表真实 RTSP 或 RK3568 联调。

人工验收需记录 Windows 版本、是否安装过 Qt、包目录名、启动结果和异常截图。涉及个人照片或 SQLite 时只保存在本机，不上传公开仓库。

## 4. 结果边界

- “本机隔离验证通过”表示发布包不依赖当前开发 PATH，必需文件齐全，基础启动和 SQLite 初始化正常。
- “干净 Windows 机器通过”才能证明新机器无需安装 Qt/OpenCV/SeetaFace 开发环境即可启动。
- 两类验证都不证明真实 RTSP、网络端口或 RK3568 已联调。
