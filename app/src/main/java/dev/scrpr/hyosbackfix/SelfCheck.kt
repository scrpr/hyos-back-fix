package dev.scrpr.hyosbackfix

import android.content.Context
import android.os.Build
import android.system.Os
import android.system.OsConstants
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.IOException

data class CheckItem(val title: String, val detail: String)
enum class CheckState { ACTIVE, WAITING, UNAVAILABLE, FAILED, RESTART }

data class CheckReport(
    val title: String,
    val detail: String,
    val state: CheckState,
    val items: List<CheckItem>,
    val native: NativeStatus? = null,
    val connectionDetail: String = "",
) {
    fun asText() = buildString {
        appendLine("HyperOS Back Fix ${BuildConfig.VERSION_NAME}")
        appendLine(title)
        appendLine(detail)
        items.forEach { appendLine("${it.title}: ${it.detail}") }
        appendLine("通信: $connectionDetail")
        native?.let {
            appendLine("原生版本: ${it.version} (${it.versionCode})")
            appendLine("PID: ${it.pid} · 阶段: ${it.stage}")
            appendLine("guard=${it.guard} remote=${it.remote} remoteBack=${it.remoteBack} merge=${it.merge}")
            appendLine("补丁页数: ${it.pages} · 页大小: ${it.pageSize}")
            appendLine("调试日志: ${if (it.debugLogging) "开启" else "关闭"}")
        }
    }
}

object SelfCheck {
    suspend fun run(context: Context, debugLogging: Boolean? = null): CheckReport = withContext(Dispatchers.IO) {
        val items = mutableListOf<CheckItem>()
        val arm64 = "arm64-v8a" in Build.SUPPORTED_ABIS
        val home = try { context.packageManager.getPackageInfo("com.miui.home", 0) }
            catch (_: android.content.pm.PackageManager.NameNotFoundException) { null }
        items += CheckItem("模块版本", "${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE})")
        items += CheckItem("系统桌面", home?.let { "${it.versionName}\n${it.longVersionCode}" } ?: "未安装")
        items += CheckItem("系统版本", "Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        items += CheckItem("设备", "${Build.MANUFACTURER} ${Build.MODEL}")
        items += CheckItem("系统架构", "${Build.SUPPORTED_ABIS.firstOrNull() ?: "未知"} (${Os.sysconf(OsConstants._SC_PAGESIZE) / 1024} KB)")
        if (!arm64 || home == null) return@withContext CheckReport("环境不支持",
            if (!arm64) "需要 ARM64 设备" else "未找到系统桌面", CheckState.FAILED, items)
        val homeUid = requireNotNull(home.applicationInfo).uid
        val status = try { NativeStatusClient.query(homeUid, debugLogging) }
        catch (error: IOException) {
            return@withContext CheckReport("未连接", "请确认模块已启用，并重启系统桌面。",
                CheckState.UNAVAILABLE, items, connectionDetail = "${error.javaClass.simpleName}: ${error.message}")
        } catch (error: IllegalArgumentException) {
            return@withContext CheckReport("自检不可用", "未收到有效的原生报告，请查看诊断信息。",
                CheckState.UNAVAILABLE, items, connectionDetail = error.message.orEmpty())
        } catch (error: SecurityException) {
            return@withContext CheckReport("连接受限", "本机通信被系统限制，请检查应用联网设置。",
                CheckState.UNAVAILABLE, items, connectionDetail = error.message.orEmpty())
        }
        val state = when {
            status.version != BuildConfig.VERSION_NAME || status.versionCode != BuildConfig.VERSION_CODE.toLong() -> CheckState.RESTART
            status.failed -> CheckState.FAILED
            status.installed -> CheckState.ACTIVE
            else -> CheckState.WAITING
        }
        CheckReport(when (state) {
            CheckState.ACTIVE -> "已激活"
            CheckState.RESTART -> "需要重启桌面"
            CheckState.FAILED -> "安装失败"
            else -> "安装中"
        }, when (state) {
            CheckState.ACTIVE -> "Hook 已安装"
            CheckState.RESTART -> "桌面仍在运行 ${status.version}，重启后生效。"
            else -> status.stageLabel
        }, state, items, status)
    }
}
