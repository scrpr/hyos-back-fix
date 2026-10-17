package dev.scrpr.hyosbackfix

import android.content.Context
import android.os.Build
import android.system.Os
import android.system.OsConstants

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
    fun run(context: Context): CheckReport {
        val items = mutableListOf<CheckItem>()
        val arm64 = "arm64-v8a" in Build.SUPPORTED_ABIS
        items += CheckItem("运行环境", "${Build.MANUFACTURER} ${Build.MODEL}\nAndroid ${Build.VERSION.RELEASE} · ${Build.SUPPORTED_ABIS.joinToString()}", arm64)
        items += CheckItem("设备页大小", "${Os.sysconf(OsConstants._SC_PAGESIZE) / 1024} KB · 按运行时页大小保护")
        val home = try { context.packageManager.getPackageInfo("com.miui.home", 0) }
            catch (_: android.content.pm.PackageManager.NameNotFoundException) { null }
        items += CheckItem("系统桌面", home?.let { "${it.versionName} (${it.longVersionCode})" } ?: "未找到 com.miui.home", home != null)
        items += CheckItem("原生安装状态", "需查看原生日志；应用无法直接读取独立 Runtime 的私有状态，未自动确认 Hook 安装结果。")
        items += CheckItem("检查方式", "重启系统桌面后，使用电脑执行：\nadb logcat -v threadtime -s HyosBackFixNative\n出现 native hook installed 表示三处 Hook 与回收保护已安装。")
        items += CheckItem("更新模块", "启用、更新或停用模块后，重启系统桌面使改动生效。")
        val ready = arm64 && home != null
        return CheckReport(if (ready) "环境信息已读取" else "请检查运行环境",
            "环境自检不代表模块已生效。请确认 LSPosed 作用域并通过原生日志查看安装结果。", items)
    }
}
