# HyperOS Back Fix

HyperOS 4 在打开 App 动画未结束时按下返回键会导致回到桌面，而不是返回到App的上一层级。小米修复了全面屏手势的这个问题，却没有修复三大金刚的对应问题。

本模块让三大金刚的返回键复用全面屏返回手势处理逻辑，修复应用启动动画期间的返回异常。

On HyperOS 4, pressing Back before an app's opening animation finishes returns you to the home screen instead of the previous screen within the app. Xiaomi has fixed this for full-screen gestures, but the issue remains with three-button navigation.

This module fixes the issue by routing the Back button in three-button navigation through the system's back gesture logic.

## 兼容版本 / Compatibility

+ HyperOS 4 (A16/A17)
+ Rust 版系统桌面 / com.miui.home (Rewritten in Rust)
+ Xposed API 102

## 安装 / Installation

1. 安装并启用本模块，选择推荐作用域 `系统桌面`

   Install and enable the module, then select the recommended scope, `System Launcher` (`com.miui.home`).

2. 强行停止系统桌面，使模块生效

   Force-stop the system launcher to apply the module.

## 版本选择 / Editions

| 版本 | 内容 | APK 文件 |
| --- | --- | --- |
| GUI 版 | 可查看状态、开关窗口复用修复和调试日志 | `hyos-back-fix-x.x.x.apk` |
| 无 GUI 版 | 窗口复用修复始终开启，无设置界面 | `hyos-back-fix-x.x.x-nogui.apk` |

| Edition | Features | APK file |
| --- | --- | --- |
| GUI | View status; toggle window-reuse fix and debug logging | `hyos-back-fix-x.x.x.apk` |
| No GUI | Window-reuse fix always on; no settings UI | `hyos-back-fix-x.x.x-nogui.apk` |

“禁用窗口复用”默认开启，修改立即生效，重启后保留。

## 开发 / Development

### 环境需求 / Requirements

+ JDK 27
+ Android SDK Platform 36 / Build Tools 37.0.0
+ Android NDK r30（30.0.16248370）

#### Windows
```powershell
.\build.ps1 -SdkPath '/path/to/sdk' -NdkPath '/path/to/ndk'
```

## 开源协议 / License

项目代码为 [MIT](LICENSE)。XZ Embedded、libc++、miuix、AndroidX 等依赖保留原协议，
见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。许可证文本随 APK 打包。

The project code is licensed under [MIT](LICENSE). Dependencies such as XZ Embedded, libc++, miuix, and AndroidX retain their original licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). License texts are bundled with the APK.

## 鸣谢 / Acknowledgement

+ [MiuiBackGestureHook](https://github.com/wxxsfxyzm/MiuiBackGestureHook)
