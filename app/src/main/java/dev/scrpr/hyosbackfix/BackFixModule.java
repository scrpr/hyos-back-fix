package dev.scrpr.hyosbackfix;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import java.io.File;
import java.io.FileReader;
import java.nio.file.Files;
import java.util.Properties;
import java.util.concurrent.FutureTask;
import io.github.libxposed.api.XposedModule;

/** Java owns the API-102 lifecycle; all navigation hooks remain native. */
public final class BackFixModule extends XposedModule {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final NativeRuntime runtime = new NativeRuntime();
    private boolean mainProcess;
    private Application application;
    private BroadcastReceiver receiver;
    private Runnable pendingStart;
    private String startupError;

    @Override public void onModuleLoaded(ModuleLoadedParam param) {
        mainProcess = DiagnosticProtocol.HOME.equals(param.getProcessName());
    }

    @Override public void onPackageReady(PackageReadyParam param) {
        if (!mainProcess || !param.isFirstPackage()) return;
        onMain(() -> scheduleStart(null));
    }

    @Override public boolean onHotReloading(HotReloadingParam param) {
        if (!mainProcess) return true;
        onMain(() -> {
            if (pendingStart != null) handler.removeCallbacks(pendingStart);
            pendingStart = null;
        });
        long[] api = runtime.api();
        if (!runtime.stop()) return false;
        onMain(() -> {
            if (receiver != null) application.unregisterReceiver(receiver);
            receiver = null;
            application = null;
        });
        Bundle state = new Bundle();
        state.putLongArray("nativeApi", api);
        param.setSavedInstanceState(state);
        return true;
    }

    @Override public void onHotReloaded(HotReloadedParam param) {
        mainProcess = DiagnosticProtocol.HOME.equals(param.getProcessName());
        if (!mainProcess) return;
        Bundle state = (Bundle) param.getSavedInstanceState();
        long[] api = state == null ? null : state.getLongArray("nativeApi");
        // API 102 does not replay onModuleLoaded/onPackageReady on reload.
        onMain(() -> scheduleStart(api));
    }

    private void scheduleStart(long[] api) {
        pendingStart = new Runnable() {
            @Override public void run() {
                try {
                    application = (Application) Class.forName("android.app.ActivityThread")
                            .getMethod("currentApplication").invoke(null);
                    if (application == null) { handler.postDelayed(this, 100); return; }
                    pendingStart = null;
                    receiver = new BroadcastReceiver() {
                        @Override public void onReceive(Context context, Intent intent) {
                            setResultExtras(readStatus());
                            setResultCode(1);
                        }
                    };
                    application.registerReceiver(receiver, new IntentFilter(DiagnosticProtocol.ACTION), Context.RECEIVER_EXPORTED);
                    runtime.start(application, getModuleApplicationInfo(), api);
                } catch (Exception | LinkageError error) {
                    startupError = error.getClass().getSimpleName() + ": " + error.getMessage();
                    android.util.Log.e("HyosBackFixNative", "Cannot start module generation", error);
                    // A reload callback runs under framework error handling. A first-load
                    // Handler callback must report the error rather than crash the launcher.
                    if (api != null) throw new IllegalStateException("Cannot start HyOS Back Fix generation", error);
                }
            }
        };
        // Application already exists on hot reload, so complete activation before returning.
        if (api != null) pendingStart.run();
        else handler.post(pendingStart);
    }

    private Bundle readStatus() {
        Bundle result = new Bundle();
        result.putBoolean("loaded", true);
        result.putBoolean("hotReload", true);
        if (startupError != null) {
            result.putString("stage", "java_failed");
            result.putString("detail", startupError);
            return result;
        }
        try {
            Properties status = new Properties();
            try (FileReader reader = new FileReader(new File(application.getFilesDir(), "hyos-back-fix.status"))) {
                status.load(reader);
            }
            String stat = new String(Files.readAllBytes(new File("/proc/self/stat").toPath()), java.nio.charset.StandardCharsets.UTF_8);
            String start = stat.substring(stat.lastIndexOf(')') + 2).split(" ")[19];
            boolean current = Integer.toString(Process.myPid()).equals(status.getProperty("pid"))
                    && start.equals(status.getProperty("start"));
            result.putBoolean("current", current);
            if (current) for (String key : status.stringPropertyNames()) result.putString(key, status.getProperty(key));
        } catch (Exception error) {
            result.putString("detail", error.getClass().getSimpleName());
        }
        return result;
    }

    private void onMain(Runnable action) {
        if (Looper.myLooper() == Looper.getMainLooper()) { action.run(); return; }
        FutureTask<Void> task = new FutureTask<>(action, null);
        handler.post(task);
        try { task.get(); }
        catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Interrupted during module lifecycle", error);
        } catch (java.util.concurrent.ExecutionException error) {
            throw new IllegalStateException("Module lifecycle failed", error.getCause());
        }
    }
}
