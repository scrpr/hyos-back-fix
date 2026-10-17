package dev.scrpr.hyosbackfix

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.system.Os
import android.system.OsConstants
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume

data class CheckItem(val title: String, val detail: String, val ok: Boolean? = null)
data class CheckReport(val title: String, val detail: String, val items: List<CheckItem>) {
    fun asText() = buildString {
        appendLine("HyOS Back Fix ${BuildConfig.VERSION_NAME} · scrpr")
        appendLine(title)
        appendLine(detail)
        items.forEach { appendLine("${it.title}: ${it.detail}") }
    }
}

object SelfCheck {
    suspend fun run(context: Context): CheckReport {
        val items = mutableListOf<CheckItem>()
        val arm64 = "arm64-v8a" in Build.SUPPORTED_ABIS
        items += CheckItem("运行环境", "${Build.MANUFACTURER} ${Build.MODEL}\nAndroid ${Build.VERSION.RELEASE} · ${Build.SUPPORTED_ABIS.joinToString()}", arm64)
        items += CheckItem("设备页大小", "${Os.sysconf(OsConstants._SC_PAGESIZE) / 1024} KB · 按运行时页大小保护")
        val home = try { context.packageManager.getPackageInfo(DiagnosticProtocol.HOME, 0) }
            catch (_: android.content.pm.PackageManager.NameNotFoundException) { null }
        items += CheckItem("系统桌面", home?.let { "${it.versionName} (${it.longVersionCode})" } ?: "未找到 com.miui.home", home != null)
        val status = queryHome(context)
        val loaded = status?.getBoolean("loaded") == true
        val current = status?.getBoolean("current") == true
        items += CheckItem("模块加载", if (loaded) "当前桌面进程已响应" else "未收到桌面响应，请检查 LSPosed 作用域并重启", if (loaded) true else null)
        items += CheckItem("API 102 热重载", if (status?.getBoolean("hotReload") == true)
            "已接入 · 同签名更新由 LSPosed 自动交接" else "等待支持热重载的模块进程", if (loaded) status?.getBoolean("hotReload") else null)
        val stage = status?.getString("stage")
        val stageText = when (stage) {
            "installed" -> "三处 Hook 与回收保护均已安装"
            "waiting_runtime" -> "等待 HyperOS Flutter 运行时"
            "waiting_launcher" -> "等待桌面原生库"
            "waiting_merge" -> "等待 Dart merge 业务定位"
            "reclaim_failed" -> "运行时回收保护安装失败"
            "runner_failed" -> "Rust 入口 Hook 安装失败"
            "merge_failed" -> "Dart merge Hook 安装失败"
            "reloading" -> "正在交接至新版本"
            "retired" -> "旧版本已退出，等待新版本加载"
            "reload_failed" -> "原生 Hook 未完全卸载，热重载未完成"
            "java_failed" -> "原生模块启动失败：${status?.getString("detail")}"
            else -> "暂无当前进程的原生安装报告"
        }
        listOf("remote" to "ForRemote", "remoteBack" to "ForRemoteBack", "merge" to "Dart merge").forEach { (key, name) ->
            val installed = current && status?.getString(key) == "1"
            items += CheckItem(name, if (installed) "Inline hook 已安装" else if (current) "尚未安装" else "等待原生报告", if (current) installed else null)
        }
        val guard = current && status?.getString("guard") == "1"
        items += CheckItem("MADV_DONTNEED 保护", if (guard) "已接管回收 · ${status?.getString("pages")} 个补丁页" else if (current) "尚未建立回收保护" else "等待原生报告", if (current) guard else null)
        if (current) items += CheckItem("原生报告", "PID ${status?.getString("pid")} · 模块 ${status?.getString("version")}\n$stageText")
        val installed = current && stage == "installed"
        return CheckReport(if (installed) "已安装，等待体验验证" else if (loaded) "模块已加载" else "等待模块响应",
            if (installed) "自检确认安装结果；请在打开应用、动画中返回及熄屏恢复后验证实际效果。" else stageText, items)
    }

    private suspend fun queryHome(context: Context): Bundle? = suspendCancellableCoroutine { continuation ->
        val handler = Handler(Looper.getMainLooper())
        val timeout = Runnable { if (continuation.isActive) continuation.resume(null) }
        handler.postDelayed(timeout, 2500)
        continuation.invokeOnCancellation { handler.removeCallbacks(timeout) }
        context.sendOrderedBroadcast(Intent(DiagnosticProtocol.ACTION).setPackage(DiagnosticProtocol.HOME)
            .addFlags(Intent.FLAG_RECEIVER_FOREGROUND), null, object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                handler.removeCallbacks(timeout)
                if (continuation.isActive) continuation.resume(getResultExtras(false))
            }
        }, handler, 0, null, null)
    }
}
