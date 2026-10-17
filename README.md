# HyperOS Back Fix

HyperOS 4 在打开App动画未结束时按下返回键会导致回到桌面，而不是返回到App的上一层级。小米修复了全面屏手势的这个问题，却没有修复三大金刚的对应问题。

本模块把三大金刚的返回键行为接入返回手势业务处理，实现对应修复。

## 兼容版本

+ HyperOS 4 (A16/A17)
+ Rust 版系统桌面
+ Xposed API 102

## 安装

1. 安装并启用本模块，选择推荐作用域 `系统桌面`
2. 强行停止系统桌面，使模块生效

## 版本选择

| 版本 | 内容 | APK 文件 |
| --- | --- | --- |
| GUI 版 | 可以查看注入状态，可以打开调试日志，体积较大 | `hyos-back-fix-x.x.x.apk` |
| 无 GUI 版 | 无法打开调试日志，体积较小 | `hyos-back-fix-x.x.x-nogui.apk` |


## 开发

### 环境需求
+ JDK 21
+ Android SDK Platform 36 / Build Tools 35.0.0
+ Android NDK r29（29.0.14206865）

```powershell
.\build.ps1 -SdkPath '/path/to/sdk' -NdkPath '/path/to/ndk'
```

## 开源协议

项目代码为 [MIT](LICENSE)。XZ Embedded、libc++、miuix、AndroidX 等依赖保留原协议，
见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。许可证文本随 APK 打包。
