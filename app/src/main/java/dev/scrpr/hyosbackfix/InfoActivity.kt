package dev.scrpr.hyosbackfix

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
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
    var report by remember { mutableStateOf<CheckReport?>(null) }
    var running by remember { mutableStateOf(false) }
    var copied by remember { mutableStateOf(false) }
    val check: () -> Unit = {
        scope.launch {
            running = true
            copied = false
            try { report = SelfCheck.run(context) } finally { running = false }
        }
    }
    LaunchedEffect(Unit) { check() }
    Column(Modifier.fillMaxSize().background(MiuixTheme.colorScheme.background)
        .safeDrawingPadding().verticalScroll(rememberScrollState())
        .padding(horizontal = 24.dp).padding(top = 24.dp, bottom = 32.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)) {
        Text("SCRPR  /  HYPEROS", color = MiuixTheme.colorScheme.primary, fontSize = 12.sp, fontWeight = FontWeight.Bold)
        Text("让返回\n及时发生。", fontSize = 38.sp, lineHeight = 46.sp, fontWeight = FontWeight.Bold)
        Text("HyOS Back Fix", fontSize = 18.sp, fontWeight = FontWeight.Medium)
        Text("三键导航 · 原厂动画 · 返回交接", color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
        Section {
            Text(if (running) "正在自检…" else report?.title ?: "准备自检", fontSize = 21.sp, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(8.dp))
            Text(if (running) "正在读取设备与系统桌面信息。" else report?.detail ?: "", lineHeight = 23.sp)
            Spacer(Modifier.height(18.dp))
            TextButton(text = if (running) "检查中…" else "重新自检", onClick = check,
                enabled = !running, modifier = Modifier.fillMaxWidth())
        }
        Text("运行状态", fontSize = 14.sp, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
        Section {
            report?.items?.forEachIndexed { index, item ->
                if (index != 0) Spacer(Modifier.height(20.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text(when (item.ok) { true -> "✓"; false -> "!"; null -> "·" },
                        color = MiuixTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
                    Column(Modifier.weight(1f)) {
                        Text(item.title, fontWeight = FontWeight.Medium)
                        Spacer(Modifier.height(4.dp))
                        Text(item.detail, fontSize = 13.sp, lineHeight = 20.sp,
                            color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
                    }
                }
            }
            if (report == null) Text("等待设备信息…")
        }
        TextButton(text = if (copied) "已复制诊断报告" else "复制诊断报告", enabled = report != null && !running,
            modifier = Modifier.fillMaxWidth(), onClick = {
                (context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager)
                    .setPrimaryClip(ClipData.newPlainText("HyOS Back Fix", report!!.asText()))
                copied = true
            })
        Text("开始使用", fontSize = 14.sp, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
        Section {
            Info("01  启用模块", "在支持 Modern API 102 和 HyperOS Rust Runtime native 注入的 LSPosed 中启用本模块。")
            Info("02  选择系统桌面", "作用域仅勾选系统桌面（com.miui.home），首次启用后重启一次桌面。迁移时请先停用旧模块，避免重复 Hook。")
            Info("03  更新与返回", "从桌面图标或后台卡片打开应用，在启动动画中按三键返回。启用、更新或停用模块后，重启一次系统桌面使改动生效。", last = true)
        }
        Text("它如何工作", fontSize = 14.sp, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
        Section {
            Info("沿用原厂返回流程", "接管桌面收到的返回键抬起事件，复用 merge / default break-open 业务、应用返回回调和原厂 fallback。关闭动画仍走原逻辑。")
            Info("保护补丁所在页面", "运行时执行 MADV_DONTNEED 时保留 inline hook 所在页，其余页面继续回收，其他 advice 保持原行为。")
            Info("按能力适配", "不校验机型、系统或桌面版本，不比较文件 hash，不锁定补丁前字节。仍需桌面提供对应 Rust / Dart 原生业务与 ARM64 ABI。", last = true)
        }
        Text("${BuildConfig.VERSION_NAME}  ·  MIT License  ·  by scrpr", fontSize = 12.sp,
            color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
        Text("界面由 miuix 驱动 · miuix：Apache-2.0 · libc++：Apache-2.0 with LLVM exception · XZ Embedded：0BSD",
            fontSize = 11.sp, lineHeight = 17.sp, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
    }
}

@Composable
private fun Section(content: @Composable ColumnScope.() -> Unit) {
    Card(modifier = Modifier.fillMaxWidth(), insideMargin = PaddingValues(20.dp), content = content)
}

@Composable
private fun Info(title: String, description: String, last: Boolean = false) {
    Text(title, fontWeight = FontWeight.Medium)
    Spacer(Modifier.height(7.dp))
    Text(description, fontSize = 14.sp, lineHeight = 23.sp, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
    if (!last) Spacer(Modifier.height(22.dp))
}
