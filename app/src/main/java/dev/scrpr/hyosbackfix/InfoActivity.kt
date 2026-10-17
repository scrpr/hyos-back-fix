package dev.scrpr.hyosbackfix

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.saveable.rememberSaveableStateHolder
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.sp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.icons.useful.Back
import top.yukonga.miuix.kmp.icon.icons.useful.Refresh
import top.yukonga.miuix.kmp.extra.SuperSwitch
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.theme.darkColorScheme
import top.yukonga.miuix.kmp.theme.lightColorScheme

class InfoActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            MiuixTheme(colors = if (isSystemInDarkTheme()) darkColorScheme() else lightColorScheme()) {
                BackFixScreen(this)
            }
        }
    }
}

@Composable
private fun BackFixScreen(context: Context) {
    val scope = rememberCoroutineScope()
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    val checkMutex = remember { Mutex() }
    var report by remember { mutableStateOf<CheckReport?>(null) }
    var running by remember { mutableStateOf(true) }
    var copied by remember { mutableStateOf(false) }
    var changingDebug by remember { mutableStateOf(false) }
    var changingReuse by remember { mutableStateOf(false) }
    var page by rememberSaveable { mutableStateOf("home") }
    suspend fun refresh(showProgress: Boolean) {
        if (!checkMutex.tryLock()) return
        try {
            if (showProgress) running = true
            report = SelfCheck.run(context)
        } finally {
            running = false
            checkMutex.unlock()
        }
    }
    // No retained activation cache. Refresh on return and while the page is visible.
    LaunchedEffect(lifecycle) {
        lifecycle.repeatOnLifecycle(Lifecycle.State.RESUMED) {
            refresh(true)
            while (true) {
                delay(5000)
                refresh(false)
            }
        }
    }
    LaunchedEffect(copied) { if (copied) { delay(2000); copied = false } }
    BackHandler(page != "home") { page = "home" }
    val copyReport: () -> Unit = {
        report?.let {
            (context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager)
                .setPrimaryClip(ClipData.newPlainText("HyperOS Back Fix", it.asText()))
            copied = true
        }
    }
    val setDebugLogging: (Boolean) -> Unit = { enabled ->
        changingDebug = true
        scope.launch {
            try {
                checkMutex.withLock {
                    report = SelfCheck.run(context, debugLogging = enabled)
                    if (report?.native?.debugLogging != enabled) {
                        Toast.makeText(context, "设置未确认，请重新连接桌面后重试", Toast.LENGTH_SHORT).show()
                    }
                }
            } finally {
                changingDebug = false
            }
        }
    }
    val setNoReuse: (Boolean) -> Unit = { enabled ->
        changingReuse = true
        scope.launch {
            try {
                checkMutex.withLock {
                    report = SelfCheck.run(context, noReuseEnabled = enabled)
                    if (report?.native?.noReuseEnabled != enabled) {
                        Toast.makeText(context, "保存失败，请重试", Toast.LENGTH_SHORT).show()
                    }
                }
            } finally {
                changingReuse = false
            }
        }
    }
    PageTransition(page = page, onBack = { page = "home" }) { currentPage ->
        Column(Modifier.fillMaxWidth().weight(1f).verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp).padding(top = if (currentPage == "home") 36.dp else 16.dp, bottom = 24.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(when (currentPage) { "help" -> "使用说明"; "diagnostics" -> "诊断信息"; else -> "HyperOS Back Fix" },
                fontSize = 30.sp, lineHeight = 38.sp, fontWeight = FontWeight.Medium,
                modifier = Modifier.padding(horizontal = 12.dp).padding(bottom = 12.dp))
            when (currentPage) {
                "home" -> {
                    StatusCard(report, running, onRefresh = { scope.launch { refresh(true) } })
                    Card(modifier = Modifier.fillMaxWidth(), insideMargin = PaddingValues(0.dp)) {
                        SuperSwitch(title = "禁用窗口复用",
                            summary = "开启后从桌面打开的应用响应返回的速度会更快，但可能导致动画不连贯",
                            checked = report?.native?.noReuseEnabled ?: true,
                            enabled = report?.native?.noReuseEnabled != null && report?.state != CheckState.RESTART
                                && !running && !changingReuse && !changingDebug,
                            onCheckedChange = setNoReuse,
                            insideMargin = PaddingValues(horizontal = 24.dp, vertical = 18.dp))
                    }
                    Section {
                        report?.items?.forEachIndexed { index, item ->
                            if (index > 0) Spacer(Modifier.height(24.dp))
                            Info(item.title, item.detail)
                        }
                        if (report == null) Text("正在读取设备信息…")
                    }
                    NavigationRow("诊断信息") { page = "diagnostics" }
                    NavigationRow("使用说明") { page = "help" }
                }
                "diagnostics" -> {
                    StatusCard(report, running)
                    Card(modifier = Modifier.fillMaxWidth(), insideMargin = PaddingValues(0.dp)) {
                        SuperSwitch(title = "调试日志",
                            summary = if (report?.native == null) "连接桌面后可用"
                                else "写入 Xposed 日志\n重启桌面后自动关闭",
                            checked = report?.native?.debugLogging == true,
                            enabled = report?.native != null && report?.state != CheckState.RESTART
                                && !running && !changingDebug && !changingReuse,
                            onCheckedChange = setDebugLogging,
                            insideMargin = PaddingValues(horizontal = 24.dp, vertical = 18.dp))
                    }
                    Section {
                        val native = report?.native
                        if (native == null) {
                            Info("未知状态", "未收到实时报告")
                            Spacer(Modifier.height(20.dp))
                            SelectionContainer { Text(report?.connectionDetail.orEmpty(), fontSize = 14.sp,
                                color = MiuixTheme.colorScheme.onSurfaceVariantSummary) }
                        } else {
                            Info("ForRemote", hookState(native.remote, native))
                            Spacer(Modifier.height(20.dp))
                            Info("ForRemoteBack", hookState(native.remoteBack, native))
                            Spacer(Modifier.height(20.dp))
                            Info("动画业务", hookState(native.merge, native))
                            Spacer(Modifier.height(20.dp))
                            Info("禁用窗口复用", if (native.noReuseEnabled == false) "已关闭" else hookState(native.noReuse, native))
                            Spacer(Modifier.height(20.dp))
                            Info("回收保护", hookState(native.guard, native))
                            Spacer(Modifier.height(20.dp))
                            Info("Native进程", "PID ${native.pid} · ${native.version} (${native.versionCode})")
                            Spacer(Modifier.height(20.dp))
                            Info("补丁页面", "${native.pages} 页 · 每页 ${native.pageSize / 1024} KB")
                        }
                    }
                    TextButton(text = if (copied) "已复制" else "复制诊断报告", onClick = copyReport,
                        enabled = report != null && !running, modifier = Modifier.fillMaxWidth())
                }
                "help" -> {
                    Section {
                        Info("启用模块", "在支持原生注入的 LSPosed 中启用，作用域选择系统桌面（com.miui.home）。")
                        Spacer(Modifier.height(24.dp))
                        Info("重启桌面", "启用、更新或停用模块后，需要强制停止系统桌面。")
                    }
                    Section {
                        Info("关于", "HyperOS Back Fix ${BuildConfig.VERSION_NAME}")
                        Spacer(Modifier.height(24.dp))
                        Info("开发者", "GPT 6 Astra, GPT 6.1 Sol")
                        Spacer(Modifier.height(24.dp))
                        Info("开源协议", "MIT License")
                        Spacer(Modifier.height(24.dp))
                        Info("开源许可", "miuix：Apache-2.0\nlibc++：Apache-2.0 with LLVM exception\nXZ Embedded：0BSD\n完整许可证随 APK 打包")
                    }
                }
            }
        }
    }
}

@Composable
private fun PageTransition(
    page: String,
    onBack: () -> Unit,
    content: @Composable ColumnScope.(String) -> Unit,
) {
    val pageStates = rememberSaveableStateHolder()
    val direction = if (LocalLayoutDirection.current == LayoutDirection.Rtl) -1 else 1
    AnimatedContent(
        targetState = page,
        modifier = Modifier.fillMaxSize().background(MiuixTheme.colorScheme.background).safeDrawingPadding(),
        transitionSpec = {
            val motion = tween<IntOffset>(300, easing = FastOutSlowInEasing)
            val transform = if (targetState == "home") {
                // Pop: reveal the home page as the secondary page leaves.
                (slideInHorizontally(motion) { -direction * it / 4 } + fadeIn(tween(180)))
                    .togetherWith(slideOutHorizontally(motion) { direction * it } + fadeOut(tween(180)))
            } else {
                // Push: the new page enters from the trailing edge.
                (slideInHorizontally(motion) { direction * it } + fadeIn(tween(180)))
                    .togetherWith(slideOutHorizontally(motion) { -direction * it / 4 } + fadeOut(tween(180)))
            }
            // On pop the departing secondary page stays above the revealed home page.
            transform.apply { targetContentZIndex = if (targetState == "home") 0f else 1f }
        },
        label = "pageNavigation",
    ) { currentPage ->
        // Each page keeps its scroll position when removed from composition.
        pageStates.SaveableStateProvider(currentPage) {
            Column(Modifier.fillMaxSize().background(MiuixTheme.colorScheme.background)) {
                if (currentPage != "home") {
                    IconButton(onClick = onBack, backgroundColor = Color.Transparent,
                        modifier = Modifier.padding(start = 12.dp, top = 8.dp).size(48.dp)) {
                        Icon(imageVector = MiuixIcons.Useful.Back, contentDescription = "返回上一页",
                            tint = MiuixTheme.colorScheme.onBackground,
                            modifier = Modifier.size(26.dp).graphicsLayer { scaleX = direction.toFloat() })
                    }
                }
                content(currentPage)
            }
        }
    }
}

private fun hookState(installed: Boolean, status: NativeStatus) = when {
    installed -> "已安装"
    status.failed -> "未安装"
    else -> "等待安装"
}

@Composable
private fun StatusCard(report: CheckReport?, running: Boolean, onRefresh: (() -> Unit)? = null) {
    val dark = isSystemInDarkTheme()
    val state = if (running) null else report?.state
    val (background, foreground) = when (state) {
        CheckState.ACTIVE -> if (dark) Color(0xFF173C2A) to Color(0xFFB9EBCB)
            else Color(0xFFDFF5E5) to Color(0xFF18532E)
        CheckState.FAILED -> if (dark) Color(0xFF482827) to Color(0xFFFFC9C4)
            else Color(0xFFFBE5E2) to Color(0xFF852E27)
        CheckState.WAITING, CheckState.RESTART -> if (dark) Color(0xFF44371F) to Color(0xFFF2D9A5)
            else Color(0xFFFFF0D4) to Color(0xFF765215)
        else -> MiuixTheme.colorScheme.surface to MiuixTheme.colorScheme.onSurface
    }
    Card(modifier = Modifier.fillMaxWidth(), insideMargin = PaddingValues(24.dp),
        colors = CardDefaults.defaultColors(color = background, contentColor = foreground)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(if (running) "正在自检…" else report?.title ?: "未连接", color = foreground,
                fontSize = 23.sp, lineHeight = 30.sp, fontWeight = FontWeight.Bold,
                modifier = Modifier.weight(1f, fill = false))
            if (onRefresh != null) {
                IconButton(onClick = onRefresh, enabled = !running, backgroundColor = Color.Transparent,
                    modifier = Modifier.size(40.dp)) {
                    Icon(imageVector = MiuixIcons.Useful.Refresh,
                        contentDescription = if (running) "正在自检" else "重新自检",
                        tint = if (running) foreground.copy(alpha = 0.5f) else foreground,
                        modifier = Modifier.size(18.dp))
                }
            }
        }
        Spacer(Modifier.height(8.dp))
        Text(if (running) "正在连接系统桌面" else report?.detail.orEmpty(), color = foreground,
            fontSize = 14.sp, lineHeight = 21.sp)
    }
}

@Composable
private fun NavigationRow(title: String, onClick: () -> Unit) {
    Card(modifier = Modifier.fillMaxWidth(), insideMargin = PaddingValues(horizontal = 24.dp, vertical = 19.dp),
        onClick = onClick) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(title, fontSize = 17.sp)
            val color = MiuixTheme.colorScheme.onSurfaceVariantSummary
            Canvas(Modifier.size(20.dp)) {
                drawLine(color, Offset(size.width * .35f, size.height * .2f),
                    Offset(size.width * .65f, size.height * .5f), 2.dp.toPx())
                drawLine(color, Offset(size.width * .65f, size.height * .5f),
                    Offset(size.width * .35f, size.height * .8f), 2.dp.toPx())
            }
        }
    }
}

@Composable
private fun Section(content: @Composable ColumnScope.() -> Unit) {
    Card(modifier = Modifier.fillMaxWidth(), insideMargin = PaddingValues(24.dp), content = content)
}

@Composable
private fun Info(title: String, description: String) {
    Text(title, fontSize = 17.sp, lineHeight = 24.sp)
    Spacer(Modifier.height(4.dp))
    Text(description, fontSize = 14.sp, lineHeight = 21.sp, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
}
