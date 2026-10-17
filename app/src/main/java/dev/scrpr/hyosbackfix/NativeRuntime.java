package dev.scrpr.hyosbackfix;

import android.app.Application;
import android.content.pm.ApplicationInfo;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.util.zip.ZipFile;

/** A separate inode and linker mapping for each API-102 generation. */
final class NativeRuntime {
    private File library;
    private File directory;
    private boolean started;

    void start(Application application, ApplicationInfo module, long[] previousApi) throws IOException {
        directory = Files.createTempDirectory(application.getCodeCacheDir().toPath(), "hyos-back-fix-").toFile();
        library = new File(directory, "libhyosbackfix.so");
        try {
            try (ZipFile apk = new ZipFile(module.sourceDir);
                 InputStream input = apk.getInputStream(apk.getEntry("lib/arm64-v8a/libhyosbackfix.so"))) {
                Files.copy(input, library.toPath());
            }
            library.setReadOnly();
            System.load(library.getAbsolutePath());
            long hook = previousApi == null ? 0 : previousApi[0];
            long unhook = previousApi == null ? 0 : previousApi[1];
            long pages = previousApi == null ? 0 : previousApi[2];
            if (!nativeStart(hook, unhook, pages)) throw new IllegalStateException("LSPosed native API was not supplied");
            started = true;
        } finally {
            // Linux keeps the mapping alive; no executable files accumulate in code_cache.
            library.delete();
            directory.delete();
            library = null;
            directory = null;
        }
    }

    long[] api() { return started ? nativeApi() : null; }

    boolean stop() {
        if (!started) return true;
        if (!nativeStop()) return false;
        started = false;
        return true;
    }

    private static native boolean nativeStart(long hook, long unhook, long pages);
    private static native long[] nativeApi();
    private static native boolean nativeStop();
}
