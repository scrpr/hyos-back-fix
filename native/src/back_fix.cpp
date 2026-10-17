#include <android/log.h>
#include <atomic>
#include <dlfcn.h>
#include <link.h>
#include <thread>
#include <jni.h>
#include <memory>
#include "symbols.h"
#include "rust_wake.h"
#include "merge_business.h"
#include "hook_pages.h"
#include "diagnostics.h"

extern "C" {
std::atomic<uint32_t> mergeKeyRequests{0};
std::atomic<uint32_t> mergePrepared{0};
int32_t mergeInterruptOffset;
uintptr_t originalMergeBusiness;
void prepareMergeBusiness();
}

using HookFunction = int (*)(void *, void *, void **);
using LibraryLoaded = void (*)(const char *, void *);
struct NativeAPIEntries { uint32_t version; HookFunction hook; int (*unhook)(void *); };

namespace {
HookFunction hookFunction;
hook_pages::InlineUnhook unhookFunction;
template<unsigned Runner> void (*originalNotify)(void *, void *);
int (*keyCode)(void *), (*keyAction)(void *), (*keyFlags)(void *);
void (*enterRuntime)(void *,uintptr_t,uintptr_t);
uint32_t (*pollFinish)(void *,void *);
void (*dropFinish)(void *), (*dropRuntime)(void *);
void (*backStart)(void *,int,float,float);
void (*backInvoke)(void *), (*backCancel)(void *), (*dropBackInfo)(void *);
void *(*backCallback)(void *);
void (*dropCallback)(void *);
uint32_t (*mergeAnimationSupport)(), (*breakOpen)(uint32_t);
uintptr_t manager, runtimeSlot;
std::atomic<bool> stopping{false};
std::mutex dispatchLock;
std::thread installerThread;
struct BackJob {
    std::thread thread;
    std::atomic<bool> complete{false};
    ~BackJob() { if (thread.joinable()) thread.join(); }
};
std::vector<std::unique_ptr<BackJob>> backJobs;
void *remoteTarget, *remoteBackTarget, *mergeTarget;
bool guardInstalled, installationFailed, nativeInstalled, mergeInstalled;
bool remoteInstalled, remoteBackInstalled;
bool nativeAttempted, mergeAttempted;

void report(const char *stage) {
    diagnostics::publish(stage, guardInstalled, remoteInstalled, remoteBackInstalled,
                         mergeInstalled, hook_pages::count());
}

template<class T> T load(uintptr_t address) {
    return __atomic_load_n(reinterpret_cast<T *>(address), __ATOMIC_ACQUIRE);
}

void finishOpen() {
    alignas(16) uint8_t future[256]{};
    auto *wake = new rust_wake::Wake;
    rust_wake::RawWaker waker{&rust_wake::wakeTable, wake};
    uintptr_t context[3]{reinterpret_cast<uintptr_t>(&waker), reinterpret_cast<uintptr_t>(&waker), 0};
    *reinterpret_cast<uintptr_t *>(future+16) = manager;
    future[32] = 1;
    future[33] = 0; // Finish the opening transition with toHome=false.
    while (!stopping.load(std::memory_order_acquire) && pollFinish(future, context) == 1) {
        std::unique_lock<std::mutex> lock(wake->lock);
        // Polling is only to observe generation retirement; there is no navigation timeout.
        while (!wake->signaled && !stopping.load(std::memory_order_acquire))
            wake->changed.wait_for(lock, std::chrono::milliseconds(16));
        wake->signaled = false;
    }
    dropFinish(future);
    rust_wake::release(wake);
    __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "finish returned state=%d", load<int>(manager+64));
}

void gestureBack() {
    if (stopping.load(std::memory_order_acquire)) return;
    alignas(16) uintptr_t guard[3]{};
    uintptr_t runtime = load<uintptr_t>(runtimeSlot);
    enterRuntime(guard, load<uintptr_t>(runtime+64), load<uintptr_t>(runtime+72));
    // Follow handle_back_gesture: start navigation, run break-open business,
    // then confirm Back. Merge keeps ownership of the opening transition.
    uintptr_t helper[2]{};
    backStart(helper, 0, 0.0f, 0.0f);
    void *callback = backCallback(reinterpret_cast<void *>(helper[1]));
    __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "gesture start navigation=%d callback=%d state=%d",
            helper[1] != 0, callback != nullptr, load<int>(manager+64));

    bool merge = (mergeAnimationSupport() & 1) != 0;
    if (merge) {
        mergePrepared.store(0, std::memory_order_relaxed);
        mergeKeyRequests.fetch_add(1, std::memory_order_release);
    }
    bool accepted = (breakOpen(merge) & 1) != 0;
    if (merge) mergeKeyRequests.fetch_sub(1, std::memory_order_release);
    bool prepared = merge && mergePrepared.load(std::memory_order_acquire);
    __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "gesture business mode=%s accepted=%d prepared=%d state=%d",
            merge ? "merge" : "default", accepted, prepared, load<int>(manager+64));
    if (!merge && accepted) {
        backCancel(helper);
        if (callback) dropCallback(callback);
        dropRuntime(guard);
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "gesture consumed by default break-open");
        return;
    }
    if (!merge && load<uint8_t>(manager+73)) finishOpen();

    // The stock gesture helper owns fallback once merge has released the keys.
    bool fallback = !callback && prepared && accepted && load<int>(manager+64) == 3;
    if (fallback) {
        if (helper[1]) backCancel(helper);
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "gesture fallback: merge handoff complete");
    } else if (!callback) {
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "navigation pending: await application navigation");
        auto started = std::chrono::steady_clock::now();
        unsigned attempts = 0;
        do {
            if (helper[1]) backCancel(helper);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            backStart(helper, 0, 0.0f, 0.0f);
            callback = backCallback(reinterpret_cast<void *>(helper[1]));
            ++attempts;
        } while (!callback && load<int>(manager+64) != 2 && !stopping.load(std::memory_order_acquire));
        if (load<int>(manager+64) == 2 || stopping.load(std::memory_order_acquire)) {
            backCancel(helper);
            if (callback) dropCallback(callback);
            dropRuntime(guard);
            __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "navigation cancelled: closing transition");
            return;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "navigation ready attempts=%u wait_ms=%lld state=%d",
                attempts, static_cast<long long>(elapsed), load<int>(manager+64));
    }
    if (stopping.load(std::memory_order_acquire)) {
        backCancel(helper);
        if (callback) dropCallback(callback);
        dropRuntime(guard);
        return;
    }
    if (callback) dropCallback(callback);
    backInvoke(reinterpret_cast<void *>(helper[1]));
    if (helper[1]) dropBackInfo(reinterpret_cast<void *>(helper[1]));
    dropRuntime(guard);
    __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "gesture invoke returned path=%s state=%d",
            fallback ? "fallback" : "callback", load<int>(manager+64));
}

template<unsigned Runner>
void intercept(void *self, void *key) {
    std::lock_guard<std::mutex> dispatch(dispatchLock);
    if (stopping.load(std::memory_order_acquire)) {
        originalNotify<Runner>(self, key);
        return;
    }
    if (keyCode(key) == 4 && keyAction(key) == 1) {
        int state = load<int>(manager+64);
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "back received runner=%u flags=%x state=%d",
                Runner, keyFlags(key), state);
        if (state == 2) {
            originalNotify<Runner>(self, key);
            return;
        }
        // Bypass the original before it stores the key interception timestamp.
        __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative", "back routed to gesture business");
        std::erase_if(backJobs, [](const auto &job) { return job->complete.load(std::memory_order_acquire); });
        auto job = std::make_unique<BackJob>();
        auto *work = job.get();
        work->thread = std::thread([work] {
            gestureBack();
            work->complete.store(true, std::memory_order_release);
        });
        backJobs.push_back(std::move(job));
        return;
    }
    originalNotify<Runner>(self, key);
}

int install(dl_phdr_info *image, size_t, void *) {
    if (!guardInstalled && strstr(image->dlpi_name, "libhyper_os_flutter.so")) {
        guardInstalled = hook_pages::install(*image);
        if (!guardInstalled) {
            installationFailed = true;
            report("reclaim_failed");
            __android_log_print(ANDROID_LOG_ERROR, "HyosBackFixNative", "reclaim guard installation failed");
            return 1;
        }
        report("waiting_launcher");
    }
    if (!guardInstalled || nativeAttempted || !strstr(image->dlpi_name, "libapp_launcher.so")) return 0;
    Symbols symbols(image->dlpi_addr, image->dlpi_name);
    auto notify = symbols.find({"36IRecentsAnimationRunnerImplForRemoteI", "26notify_intercept_key_event"});
    auto enter = symbols.find({"_RNvM", "5tokio7runtime6handle", "6Handle5enter"});
    auto sync = symbols.find({"_RNvNt", "WindowTransitionAgent_finish_remote_transition_sync_impl"});
    manager = referencedGlobal(notify);
    runtimeSlot = referencedGlobal(sync, enter.address);
    enterRuntime = reinterpret_cast<decltype(enterRuntime)>(enter.address);
    pollFinish = reinterpret_cast<decltype(pollFinish)>(symbols.find({"_RNCNv", "23WindowTransitionManager24finish_remote_transition0"}).address);
    dropFinish = reinterpret_cast<decltype(dropFinish)>(symbols.find({"9drop_glueNCNv", "23WindowTransitionManager24finish_remote_transition0E"}).address);
    dropRuntime = reinterpret_cast<decltype(dropRuntime)>(symbols.find({"9drop_glue", "15SetCurrentGuardE"}).address);
    backStart = reinterpret_cast<decltype(backStart)>(symbols.find({"_RNvMs", "22GestureInputBackHelper13on_back_start"}).address);
    backInvoke = reinterpret_cast<decltype(backInvoke)>(symbols.find({"_RNvMs", "22GestureInputBackHelper14on_back_invoke"}).address);
    backCancel = reinterpret_cast<decltype(backCancel)>(symbols.find({"_RNvMs", "22GestureInputBackHelper17on_back_cancelled"}).address);
    backCallback = reinterpret_cast<decltype(backCallback)>(symbols.find({"_RNvMs", "22GestureInputBackHelper28get_on_back_invoked_callback"}).address);
    mergeAnimationSupport = reinterpret_cast<decltype(mergeAnimationSupport)>(symbols.find({"_RNvM", "20WindowTransitionUtil26is_merge_animation_support"}).address);
    breakOpen = reinterpret_cast<decltype(breakOpen)>(symbols.find({"_RNvNt", "3api13back_controll28can_use_break_open_anim_impl"}).address);
    void *library = dlopen(image->dlpi_name, RTLD_NOW);
    keyCode = reinterpret_cast<decltype(keyCode)>(dlsym(library, "input_KeyEvent_getKeyCode"));
    keyAction = reinterpret_cast<decltype(keyAction)>(dlsym(library, "input_KeyEvent_getAction"));
    keyFlags = reinterpret_cast<decltype(keyFlags)>(dlsym(library, "input_KeyEvent_getFlags"));
    dropBackInfo = reinterpret_cast<decltype(dropBackInfo)>(dlsym(library, "BackNavigationInfo_drop"));
    dropCallback = reinterpret_cast<decltype(dropCallback)>(dlsym(library, "IOnBackInvokedCallback_drop"));
    remoteTarget = reinterpret_cast<void *>(notify.address);
    remoteInstalled = hook_pages::hook(hookFunction, remoteTarget, reinterpret_cast<void *>(intercept<0>),
            reinterpret_cast<void **>(&originalNotify<0>)) == 0;
    auto remoteBack = symbols.find({"40IRecentsAnimationRunnerImplForRemoteBackI", "26notify_intercept_key_event"});
    remoteBackTarget = reinterpret_cast<void *>(remoteBack.address);
    remoteBackInstalled = hook_pages::hook(hookFunction, remoteBackTarget, reinterpret_cast<void *>(intercept<2>),
            reinterpret_cast<void **>(&originalNotify<2>)) == 0;
    dlclose(library);
    nativeInstalled = remoteInstalled && remoteBackInstalled;
    nativeAttempted = true;
    report(nativeInstalled ? "waiting_merge" : "runner_failed");
    return 0;
}

void start() {
    char process[128]{};
    FILE *file = fopen("/proc/self/cmdline", "rb");
    fread(process, 1, sizeof(process)-1, file);
    fclose(file);
    if (strcmp(process, "com.miui.home")) return;
    installerThread = std::thread([] {
            report("waiting_runtime");
            while (!stopping.load(std::memory_order_acquire)) {
                dl_iterate_phdr(install, nullptr);
                if (installationFailed) return;
                if (nativeAttempted && !mergeAttempted) {
                    auto business = findLoadedMergeBusiness();
                    if (business.address) {
                        mergeInterruptOffset = business.interruptOffset;
                        mergeTarget = reinterpret_cast<void *>(business.address);
                        mergeInstalled = hook_pages::hook(hookFunction, mergeTarget, reinterpret_cast<void *>(prepareMergeBusiness),
                                reinterpret_cast<void **>(&originalMergeBusiness)) == 0;
                        mergeAttempted = true;
                        report(!nativeInstalled ? "runner_failed" : mergeInstalled ? "installed" : "merge_failed");
                    }
                }
                if (nativeAttempted && mergeAttempted) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            __android_log_print(ANDROID_LOG_INFO, "HyosBackFixNative",
                    "native hook %s version=" MODULE_VERSION " merge_bridge=%d reclaim_guard=1 hook_pages=%zu",
                    nativeInstalled && mergeInstalled ? "installed" : "incomplete", mergeInstalled, hook_pages::count());
    });
}

bool stop() {
    // Stop accepting work, then finish/cancel every module-owned thread.
    std::vector<std::unique_ptr<BackJob>> jobs;
    {
        std::lock_guard<std::mutex> dispatch(dispatchLock);
        stopping.store(true, std::memory_order_release);
        jobs.swap(backJobs);
    }
    if (installerThread.joinable()) installerThread.join();
    jobs.clear(); // BackJob joins, including workers waiting for Rust futures/navigation.
    std::lock_guard<std::mutex> dispatch(dispatchLock);
    report("reloading");
    if (remoteInstalled && hook_pages::unhook(unhookFunction, remoteTarget) == 0) {
        remoteInstalled = false;
        originalNotify<0> = reinterpret_cast<decltype(originalNotify<0>)>(remoteTarget);
    }
    if (remoteBackInstalled && hook_pages::unhook(unhookFunction, remoteBackTarget) == 0) {
        remoteBackInstalled = false;
        originalNotify<2> = reinterpret_cast<decltype(originalNotify<2>)>(remoteBackTarget);
    }
    if (mergeInstalled && hook_pages::unhook(unhookFunction, mergeTarget) == 0)
        mergeInstalled = false;
    if (remoteInstalled || remoteBackInstalled || mergeInstalled) {
        report("reload_failed");
        return false;
    }
    bool removed = hook_pages::uninstall();
    guardInstalled = !removed;
    report(removed ? "retired" : "reload_failed");
    return removed;
}
}

extern "C" __attribute__((visibility("default"), used))
LibraryLoaded native_init(const NativeAPIEntries *entries) {
    hookFunction = entries->hook;
    unhookFunction = entries->unhook;
    // No loader callback survives a generation. Java owns start/stop explicitly.
    return nullptr;
}

static jboolean startNative(JNIEnv *, jclass, jlong hook, jlong unhook, jlong pages) {
    if (hook) hookFunction = reinterpret_cast<HookFunction>(hook);
    if (unhook) unhookFunction = reinterpret_cast<hook_pages::InlineUnhook>(unhook);
    if (!hookFunction || !unhookFunction) return false;
    hook_pages::adoptState(static_cast<uintptr_t>(pages));
    // Keep retired code mapped for late native stack returns / Rust waker drops.
    // No Java object or classloader is retained; no new calls are registered there.
    Dl_info image{};
    dladdr(reinterpret_cast<void *>(start), &image);
    if (!dlopen(image.dli_fname, RTLD_NOW | RTLD_NODELETE)) return false;
    start();
    return true;
}

static jlongArray nativeApi(JNIEnv *env, jclass) {
    jlong addresses[]{reinterpret_cast<jlong>(hookFunction), reinterpret_cast<jlong>(unhookFunction),
                      static_cast<jlong>(hook_pages::stateAddress())};
    auto result = env->NewLongArray(3);
    env->SetLongArrayRegion(result, 0, 3, addresses);
    return result;
}

static jboolean stopNative(JNIEnv *env, jclass type) {
    if (!stop()) return false;
    env->UnregisterNatives(type);
    return true;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    jclass type = env->FindClass("dev/scrpr/hyosbackfix/NativeRuntime");
    if (!type) return JNI_ERR;
    // Bind this exact mapping, even if the framework preloaded a library with
    // the same SONAME. No global reference to a module classloader is created.
    const JNINativeMethod methods[]{
        {"nativeStart", "(JJJ)Z", reinterpret_cast<void *>(startNative)},
        {"nativeApi", "()[J", reinterpret_cast<void *>(nativeApi)},
        {"nativeStop", "()Z", reinterpret_cast<void *>(stopNative)},
    };
    int result = env->RegisterNatives(type, methods, 3);
    env->DeleteLocalRef(type);
    return result == JNI_OK ? JNI_VERSION_1_6 : JNI_ERR;
}
