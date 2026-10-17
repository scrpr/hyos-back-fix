package dev.scrpr.hyosbackfix

/** A live installer snapshot, not a claim that a particular Back event succeeded. */
data class NativeStatus(
    val version: String,
    val versionCode: Long,
    val pid: Int,
    val stage: String,
    val guard: Boolean,
    val remote: Boolean,
    val remoteBack: Boolean,
    val merge: Boolean,
    val pages: Int,
    val pageSize: Long,
    val debugLogging: Boolean,
) {
    val installed get() = stage == "installed" && guard && remote && remoteBack && merge && pages > 0
    val failed get() = stage.endsWith("_failed") || (stage == "installed" && !installed)

    val stageLabel get() = when (stage) {
        "waiting_runtime" -> "等待桌面运行时"
        "waiting_launcher" -> "等待桌面业务库"
        "waiting_merge" -> "等待动画业务"
        "reclaim_failed" -> "回收保护安装失败"
        "runner_failed" -> "返回入口安装失败"
        "merge_failed" -> "动画入口安装失败"
        "installed" -> if (installed) "安装完成" else "安装结果不完整"
        else -> "未知安装阶段"
    }

    companion object {
        const val PROTOCOL = "HYOS_BACK_FIX_STATUS/2"

        fun parse(text: String, nonce: String, homeUid: Int, started: Long, finished: Long): NativeStatus {
            require(text.length <= 4096 && text.endsWith("\nend\n")) { "报告不完整" }
            val lines = text.trimEnd().lines()
            require(lines.first() == "$PROTOCOL $nonce") { "自检协议不匹配" }
            val fields = mutableMapOf<String, String>()
            lines.drop(1).dropLast(1).forEach {
                val pair = it.split('=', limit = 2)
                require(pair.size == 2 && fields.put(pair[0], pair[1]) == null) { "报告字段无效" }
            }
            fun value(key: String) = requireNotNull(fields[key]) { "缺少 $key" }
            fun flag(key: String) = when (value(key)) {
                "1" -> true
                "0" -> false
                else -> throw IllegalArgumentException("$key 状态无效")
            }
            require(value("uid").toInt() == homeUid) { "桌面用户不匹配" }
            require(value("elapsedMs").toLong() in started..finished) { "报告已过期" }
            val result = NativeStatus(value("version"), value("versionCode").toLong(),
                value("pid").toInt(), value("stage"), flag("guard"), flag("remote"),
                flag("remoteBack"), flag("merge"), value("pages").toInt(), value("pageSize").toLong(),
                flag("debugLogging"))
            require(result.version.isNotBlank() && result.versionCode > 0 && result.pid > 0 &&
                result.pages >= 0 && result.pageSize > 0) { "报告数值无效" }
            require(result.stage in setOf("waiting_runtime", "waiting_launcher", "waiting_merge",
                "reclaim_failed", "runner_failed", "merge_failed", "installed")) { "未知安装阶段" }
            return result
        }
    }
}
