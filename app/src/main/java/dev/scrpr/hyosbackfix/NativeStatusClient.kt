package dev.scrpr.hyosbackfix

import android.os.SystemClock
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketTimeoutException
import java.util.UUID

object NativeStatusClient {
    fun query(homeUid: Int, debugLogging: Boolean? = null): NativeStatus {
        val user = homeUid / 100000
        require(user in 0..22775) { "不支持的用户编号" }
        val nonce = UUID.randomUUID().toString().replace("-", "")
        val started = SystemClock.elapsedRealtime()
        Socket().use { socket ->
            // Numeric loopback only: no DNS, proxy, external network, or root.
            socket.connect(InetSocketAddress(InetAddress.getByAddress(byteArrayOf(127, 0, 0, 1)),
                42760 + user), 700)
            val action = when (debugLogging) { null -> "?"; true -> "1"; false -> "0" }
            socket.getOutputStream().write("${NativeStatus.PROTOCOL} $nonce $action\n".toByteArray(Charsets.US_ASCII))
            val input = socket.getInputStream()
            val output = ByteArrayOutputStream()
            val buffer = ByteArray(512)
            while (true) {
                val remaining = 1800 - (SystemClock.elapsedRealtime() - started)
                if (remaining <= 0) throw SocketTimeoutException("自检连接超时")
                socket.soTimeout = minOf(700, remaining.toInt())
                val count = input.read(buffer)
                if (count < 0) break
                if (output.size() + count > 4096) throw IOException("自检报告过大")
                output.write(buffer, 0, count)
            }
            val status = NativeStatus.parse(output.toString(Charsets.UTF_8.name()), nonce, homeUid,
                started, SystemClock.elapsedRealtime())
            require(debugLogging == null || status.debugLogging == debugLogging) { "调试日志状态未确认" }
            return status
        }
    }
}
