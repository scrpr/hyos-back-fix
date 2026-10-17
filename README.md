# HyperOS Back Fix

由 `hyperos-back-fix` 重构的 LSPosed 模块。作者 **scrpr**，包名
`dev.scrpr.hyosbackfix`，版本 `0.1.2`，主项目采用 **MIT** 协议。

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

## 两种版本

| 版本 | 内容 | APK 文件 |
| --- | --- | --- |
| 界面版 | miuix 界面、实时自检、诊断报告 | `hyos-back-fix-0.1.2.apk` |
| 无 GUI 精简版 | LSPosed 注册、三处 Hook、回收保护和原生日志 | `hyos-back-fix-0.1.2-nogui.apk` |

当前本地签名的 Release 精简版约 **227.7 KiB**，界面版约 **1.83 MiB**，体积减少约 **87.8%**。
签名、工具版本和后续源码变化会影响最终体积。

无 GUI 版由独立的 `minimal` 纯 Java 模块构建，只编译共享的 `BackFixModule.java`
注册入口，不打包 Kotlin、Compose、miuix、AndroidX 或其运行依赖。它没有桌面图标、
Activity、查询桌面包的声明或应用权限，也不编译状态查询服务和对应诊断线程。
安装结果通过原生 / LSPosed 日志确认，返回业务与界面版共用同一份源码。

两个版本使用相同包名、版本号和签名，可以覆盖切换并沿用 LSPosed 启用状态和作用域，
不能同时安装。切换后需要强制停止系统桌面，使新版本的原生库重新加载。

精简版对原生代码使用 `-Oz`、ThinLTO 和安全的相同代码折叠；
保留 ARM64 与 16 KB ELF / APK 对齐。DEX 使用压缩打包，原生库保持未压缩：
当前 HyperOS Runtime 注入器直接从 `base.apk!/lib/...` 加载 `.so`，真机验证中
压缩 `.so` 会导致框架加载失败，因此不采用这种压缩方式。所用组件的完整许可证仍随包提供。

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
Compose 组件，跟随系统浅色 / 深色主题。首页展示状态、环境信息和页面入口，
状态标题旁提供自检图标；安装详情、报告复制与使用说明放在独立页面。
回到界面时重新查询，前台每 5 秒刷新，后台停止轮询。

### 无 Root 通信

原生入口启动一个独立诊断线程，仅监听 `127.0.0.1:42760 + Android 用户编号`。
应用通过普通 `INTERNET` 权限连接本机接口，不需要 Root、ADB、读取日志或存储授权，
也不依赖桌面存在 Java / JNI 环境。没有 DNS、外部服务器或联网数据上传。
桌面进程退出后监听随之消失；不读写或复用磁盘报告。

接口接受 `HYOS_BACK_FIX_STATUS/2 <32 位随机十六进制请求号> <操作>`：
`?` 查询，`0` 关闭调试日志，`1` 开启调试日志。返回当前内存中的安装快照、
调试开关实际状态并回显请求号。客户端检查协议、请求号、用户 UID、当前请求期间的单调时钟、
字段完整性及大小；连接和读取均有超时，总读取时间限制为 1.8 秒，报告上限 4 KB。
每个连接只处理一次请求，不能执行命令或更改 Hook。监听冲突或通信失败不影响返回业务。

回显请求号和 UID 检查不构成对恶意本机程序的身份认证；同用户本机程序也能切换日志详细程度。
接口不包含业务控制能力、密钥或应用数据。用户编号用于分离各用户端点，报告 UID 也必须
匹配当前用户安装的系统桌面；不要把诊断结果当成安全认证依据。

### 状态含义

- **已激活**：原生报告的版本与当前 APK 一致，三处 Hook、回收保护均安装成功，且已登记补丁页。
- **安装中**：已连通原生入口，仍在等待运行时、桌面业务库或动画业务。
- **安装失败**：原生安装器明确报告某处安装失败或最终结果不完整。
- **需要重启桌面**：当前运行中的原生版本与 APK 不一致。
- **未连接 / 自检不可用**：无法取得有效报告；可能未注入、尚未重启桌面、端口被占用或本机通信受限，不能据此断言 Hook 安装失败。

诊断页展示各处 Hook、回收保护、原生版本、PID 和补丁页数，并提供调试日志开关和报告复制。
自检确认的是**安装结果**，不读取补丁字节，也不证明某次返回已经发生。
实际效果需在启动动画中返回、熄屏恢复后返回等场景体验确认。

日志统一写入 LSPosed 2.2.1 的 `LSPosedFramework` 通道，携带进程名、模块包名、
`HyosBackFixNative` 标签和独立消息编号，已在设备的 Xposed 模块日志中验证收录。
直接写 `LSPosed-Bridge` 或不带模块信息的 `LSPosedFramework` 不适用于该收集器。
日志输出不依赖 ART / JNI、界面进程、Root 或文件写入，GUI 与无 GUI 版均可记录安装结果。

默认只输出启动、Hook 安装阶段和安装结果。诊断页开启“调试日志”后，才输出返回事件、
动画交接、等待耗时与补丁页回收明细；关闭后立即停止详细输出。开关状态由桌面进程返回，
不乐观显示设置成功；仅在本次桌面进程中生效，重启桌面后默认关闭。
无 GUI 版编译时移除详细日志调用和字符串，不提供开关或诊断接口。

也可通过 ADB 筛选同一日志通道：

```sh
adb logcat -v threadtime -s LSPosedFramework | grep dev.scrpr.hyosbackfix
```

`native installer started` 表示原生入口已启动；`stage` 显示安装阶段。
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

输出：`dist/hyos-back-fix-0.1.2.apk` 和 `dist/hyos-back-fix-0.1.2-nogui.apk`，
均为开启 R8 代码压缩、混淆及资源裁剪的 Release APK。
仅打包 ARM64 库，保留 LSPosed 入口类名。脚本默认使用本地签名，也支持通过下述环境变量指定签名。
脚本只构建 APK；项目不包含测试代码、测试脚本
或测试依赖。缓存、临时文件及本地签名密钥位于 `.local`，不会提交到 Git。
请保留 `.local/debug.keystore`，以便后续 APK 可以覆盖安装。

也可以直接设置 `ANDROID_HOME` 和 `ANDROID_NDK_HOME` 后运行
`./gradlew -Pbackfix.localSigning=true assembleRelease`。`assembleDebug` 用于未裁剪的调试构建。
只构建精简版可执行 `./gradlew -Pbackfix.localSigning=true :minimal:assembleRelease`；
只构建界面版则使用 `:app:assembleRelease`。两个版本的 NDK 中间文件和输出目录独立。
不需要外部参考仓库、固件文件或当前工作区的其他目录即可构建。
版本在 `gradle.properties` 的 `moduleVersionName` / `moduleVersionCode` 中统一维护；
APK 和原生自检报告使用同一个版本名称。

## GitHub Actions

[Build APK](.github/workflows/build.yml) 在 push、pull request 和手动触发时安装所需
SDK / NDK，分别构建界面版和精简版的 Debug / Release，将四种 APK 上传到 Actions Artifacts。
Release 开启代码和资源裁剪，并分别保存两个模块的 `mapping.txt`，用于还原混淆后的崩溃堆栈。
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
- `native/src/diagnostics.h`：独立于业务逻辑的状态接口与调试日志开关。
- `native/src/logging.h`：Xposed 模块日志格式和默认关闭的详细日志输出。
- `app/src/main/java/dev/scrpr/hyosbackfix`：miuix UI、实时原生自检和 LSPosed 注册声明。
- `minimal`：无 GUI 版本的构建配置、最小 Manifest 和许可证说明，共享注册入口与原生源码。
- `gradle/native.gradle`：两种版本共用的 NDK 构建逻辑。

## 开源协议

项目代码为 [MIT](LICENSE)。XZ Embedded、libc++、miuix、AndroidX 等依赖保留原协议，
见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。许可证文本随 APK 打包。
