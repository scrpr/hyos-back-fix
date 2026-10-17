# HyOS Back Fix

由 `hyperos-back-fix` 重构的 LSPosed 模块。作者 **scrpr**，包名
`dev.scrpr.hyosbackfix`，版本 `0.1.0`，主项目采用 **MIT** 协议。

## 行为

保留原模块的三处 inline hook：两个 Rust 按键入口 ForRemote / ForRemoteBack，
以及 Dart AOT merge 业务检查点。接管系统交给桌面的返回键抬起事件，CLOSE
状态及其他按键继续交给原方法。

返回处理保持原有顺序：开始返回导航 → 原厂 merge / default break-open 业务 →
应用回调或原厂 fallback。merge 路径在原方法解析动画实例后设置允许打断标志，
保留桌面对启动转场的管理；default 消费事件时取消返回导航，否则仍拦截时以
`toHome=false` 完成原厂收尾。没有回调时继续等待系统交接，进入 CLOSE 则取消。
不自行合成按键，不创建侧边手势窗口，也不限制桌面图标、后台卡片或全屏模式。

安装业务 Hook 前先接管 HyperOS Flutter 运行时导入的 `madvise`。
`MADV_DONTNEED` 请求只跳过补丁覆盖页，其他区间继续回收；其他 advice 和系统调用
错误语义保持原状。页大小取自当前设备，补丁安装与回收共用同步锁。
这把锁是回收保护的一部分，不是字节锁定或版本限制。

## 兼容策略

- 不读取系统或桌面版本来决定是否安装 Hook；不使用机型名单、文件 hash 或固定 RVA。
- 不比较补丁前的完整字节，也不把代码恢复成某个固件的字节。
- Rust 按业务符号名定位；Dart 按 ARM64 指令含义和寄存器关系定位，动态读取字段偏移。
  已去掉原定位器的完整指令值比较及后续无关的三条指令约束，允许 W/X 两种立即数写法。
- 不为返回导航添加超时或新的业务拦截条件。
- 安装失败结果用于自检展示；内存权限、导入符号解析和系统调用结果处理仍用于完成 Hook 本身。

面向 Android 16 / 17 上的 HyperOS 4，最低 Android 16 / API 36，原生模块为
`arm64-v8a`。桌面仍须存在相应 HyperOS Flutter / Rust / Dart 业务，Rust
私有结构布局、函数 ABI 与 Dart 调用约定也是实际依赖。版本号不会阻止安装，
但 ABI 或业务实现变化仍可能需要适配。提供 16 KB ELF 对齐，保护按设备实际页大小计算。

## 安装

1. 安装 APK；若从旧模块迁移，先停用旧模块，避免对同一入口重复 Hook。
2. 在支持 **Modern API 102、native 模块及 HyperOS Rust Runtime 注入** 的 LSPosed
   中启用本模块，作用域仅选择 **系统桌面 `com.miui.home`**。
3. 首次启用后重启一次系统桌面，使 LSPosed 注入模块；也可重启手机。

包名与旧模块不同，因此可以同时安装，但不要同时启用。模块使用静态作用域。

## 原生启动

桌面运行在独立的 HyperOS Rust Runtime 中，没有 Java / JNI 环境。
LSPosed 调用 `native_init` 时直接启动安装线程，等待 Flutter 运行时、Rust 桌面库
和 Dart AOT 业务出现，依次安装回收保护与三处 inline hook。库加载回调也调用同一
启动入口，`std::call_once` 保证安装线程只启动一次。

Java 入口仅保留框架注册声明，不启动原生库、不注册诊断广播。
模块不使用 JNI、不提取或重复加载 `.so`，也不支持热重载；启用、更新或停用后
需要重启系统桌面。原生代码和回收保护随桌面进程存活。

入口约定见 [LSPosed Native Hook 文档](https://github.com/LSPosed/LSPosed/wiki/Native-Hook)。

## miuix 界面与自检

界面使用真实的 [miuix](https://github.com/compose-miuix-ui/miuix/tree/v0.5.1)
Compose 组件，支持系统浅色 / 深色主题、滚动说明、重新自检和复制诊断报告。

UI 自检展示设备 ABI、系统桌面版本和页面大小。它不依赖 Java 广播，
也不把环境信息已读取当成模块生效。普通应用无法直接读取桌面 Runtime 的私有状态，
因此原生安装状态明确显示为未确认，并提供日志查看方式。

原生线程将每次安装阶段写入 logcat，同时尝试保存到
`/data/user/<用户编号>/com.miui.home/files/hyos-back-fix.status`，供具有权限时手动读取。
报告包含三处 Hook、回收保护、补丁页数、阶段、版本、PID 和进程启动时刻。
文件可能保留自上一个进程，必须结合当前 PID / 启动时刻判断；不能仅凭文件中的
`installed` 确认当前进程。文件写入失败不会影响日志发布或返回处理。

原生日志报告的是**安装结果**，不读取补丁字节，也不证明某次返回已经发生。
实际效果需在启动动画中返回、熄屏恢复后返回等场景体验确认。

日志：

```sh
adb logcat -v threadtime -s HyosBackFixNative
```

`native installer started` 表示原生入口已启动安装线程；`status ... stage=...` 显示安装阶段。
`reclaim guard installed` 表示回收导入已接管；`native hook installed` 表示三处 Hook
安装完成；`reclaim preserved` 表示一次回收实际跳过了补丁页。`state=1/2/3/4` 分别
对应 OPEN / CLOSE / OPEN_NEAR_FULL / NormalState。`path=callback/fallback` 表示调用
路径，不代表应用页面一定已跳转。

## 构建

需要 JDK 21、Android SDK Platform 36 / Build Tools 35.0.0 和 Android NDK r29（29.0.14206865）。
使用 Gradle Wrapper 8.11.1、AGP 8.10.1、Kotlin 2.2.10、Compose 1.8.2、miuix 0.5.1。
`compileSdk`、`targetSdk`、`minSdk` 均为 API 36。NDK r29 的原生 API stub 最高为 35，
因此 `APP_PLATFORM=android-35`；APK 的最低安装版本仍严格为 Android 16。

```powershell
.\build.ps1 -SdkPath '你的 Android SDK 目录' -NdkPath '你的 NDK 目录'
```

输出：`dist/hyos-back-fix-0.1.0.apk`，为开启 R8 代码压缩、混淆及资源裁剪的 Release APK。
仅打包 ARM64 库，保留 LSPosed 入口类名。脚本默认使用本地签名，也支持通过下述环境变量指定签名。
脚本只构建 APK；项目不包含测试代码、测试脚本
或测试依赖。缓存、临时文件及本地签名密钥位于 `.local`，不会提交到 Git。
请保留 `.local/debug.keystore`，以便后续 APK 可以覆盖安装。

也可以直接设置 `ANDROID_HOME` 和 `ANDROID_NDK_HOME` 后运行
`./gradlew -Pbackfix.localSigning=true assembleRelease`。`assembleDebug` 用于未裁剪的调试构建。
不需要外部参考仓库、固件文件或当前工作区的其他目录即可构建。
版本在 `gradle.properties` 的 `moduleVersionName` / `moduleVersionCode` 中统一维护；
APK 和原生自检报告使用同一个版本名称。

## GitHub Actions

[Build APK](.github/workflows/build.yml) 在 push、pull request 和手动触发时安装所需
SDK / NDK，执行 `assembleDebug assembleRelease`，把两种 APK 上传到 Actions Artifacts。
Release 开启代码和资源裁剪，并单独保存 `mapping.txt`，用于还原混淆后的崩溃堆栈。
工作流只构建，不添加或运行测试，不自动发布 Release。

无需 Secrets 即可构建：Debug APK 使用当次生成的开发签名，Release APK 未签名。
CI 的临时 Debug 签名不能用于连续覆盖更新。需要可覆盖更新的 Release APK 时，在
仓库 Actions Secrets 中设置：

| Secret | 内容 |
| --- | --- |
| `SIGNING_KEYSTORE_BASE64` | 固定签名 keystore 的 Base64 |
| `SIGNING_STORE_PASSWORD` | keystore 密码 |
| `SIGNING_KEY_ALIAS` | 签名别名 |
| `SIGNING_KEY_PASSWORD` | 私钥密码 |

从本地 Debug APK 迁移且需要保留覆盖安装能力时，可以使用同一份
`.local/debug.keystore`（别名 `hyos-back-fix`，默认密码 `android`）；正式分发也可以
另选自己的签名，但更换签名无法直接覆盖已有安装。PR 构建不恢复签名密钥。
本地签名 Release 可设置 `SIGNING_STORE_FILE`（绝对路径）及上表的其余三个环境变量，
然后运行 `./gradlew assembleRelease`。

## 源码结构

- `native/src/back_fix.cpp`：入口安装与原厂返回业务。
- `native/src/merge_business.h` / `.S`：AOT 业务定位与 Dart 栈桥接。
- `native/src/hook_pages.cpp` / `reclaim_ranges.h`：补丁页登记和回收区间拆分。
- `native/src/symbols.h` / `rust_wake.h`：ELF 符号定位与 Rust Future 唤醒。
- `native/src/diagnostics.h`：独立于业务逻辑的安装状态发布。
- `app/src/main/java/dev/scrpr/hyosbackfix`：miuix UI、环境自检和 LSPosed 注册声明。

## 开源协议

项目代码为 [MIT](LICENSE)。XZ Embedded、libc++、miuix、AndroidX 等依赖保留原协议，
见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。许可证文本随 APK 打包。
