// Shim boot: the log, the optional sidecar load, and the shared stub that
// every unimplemented export lands in.

#include "core/boot.hpp"
#include "core/diag.hpp"

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace {

constexpr std::uint32_t kLogFirst = 32;
constexpr std::uint32_t kLogEveryNth = 128;

struct State {
    std::mutex lock;
    FILE* file = nullptr;
    bool enabled = true;
    std::unordered_map<const char*, std::uint32_t> calls;
};

State g_state;

BOOL CALLBACK open_log(PINIT_ONCE, PVOID, PVOID*) {
    char flag[8] = {};
    if (GetEnvironmentVariableA("EOSSHIM_LOG", flag, sizeof flag) && flag[0] == '0') {
        g_state.enabled = false;
        return TRUE;
    }

    char module[MAX_PATH] = {};
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&g_state), &self) ||
        !GetModuleFileNameA(self, module, MAX_PATH)) {
        g_state.enabled = false;
        return TRUE;
    }

    if (char* slash = strrchr(module, '\\')) {
        slash[1] = '\0';
    } else {
        module[0] = '\0';
    }

    std::string path = std::string(module) + "eos-shim.log";
    g_state.file = std::fopen(path.c_str(), "a");
    if (!g_state.file) {
        g_state.enabled = false;
        return TRUE;
    }

    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(g_state.file, "=== shim attached pid=%lu %04d-%02d-%02d %02d:%02d:%02d ===\n",
                 GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond);
    std::fflush(g_state.file);
    shim_diag_note_locked("shim attached pid=%lu", GetCurrentProcessId());
    return TRUE;
}

INIT_ONCE g_open_once = INIT_ONCE_STATIC_INIT;

}  // namespace

// Nothing wires this in this build: there is no sidecar to load, so the pump
// stays null and EOS_Platform_Tick only measures its own cost. The declaration
// is kept so every other translation unit is identical to the build that does
// load better_game.dll.
void (*g_pump)() = nullptr;

int stub(const char* name) {
    InitOnceExecuteOnce(&g_open_once, open_log, nullptr, nullptr);

    std::lock_guard<std::mutex> guard(g_state.lock);
    const std::uint32_t n = ++g_state.calls[name];
    if (!g_state.enabled || !g_state.file) {
        return 0;
    }
    if (n > kLogFirst && (n % kLogEveryNth) != 0) {
        return 0;
    }

    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(g_state.file,
                 "[%02d:%02d:%02d.%03d pid=%lu tid=%lu n=%llu] %s%s\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                 GetCurrentProcessId(), GetCurrentThreadId(),
                 static_cast<unsigned long long>(n), name,
                 n > kLogFirst ? " (repeat)" : "");
    std::fflush(g_state.file);
    return 0;
}

void shim_log(const char* fmt, ...) {
    InitOnceExecuteOnce(&g_open_once, open_log, nullptr, nullptr);

    std::lock_guard<std::mutex> guard(g_state.lock);
    if (!g_state.enabled || !g_state.file) return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(g_state.file, "[%02d:%02d:%02d.%03d pid=%lu tid=%lu] ", st.wHour, st.wMinute,
                 st.wSecond, st.wMilliseconds, GetCurrentProcessId(), GetCurrentThreadId());
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_state.file, fmt, ap);
    va_end(ap);
    std::fputc('\n', g_state.file);
    std::fflush(g_state.file);
}

// ---------------------------------------------------------------------------
// crash probe: if the process dies on a native fault, say where
// ---------------------------------------------------------------------------
namespace {

LONG WINAPI on_fault(EXCEPTION_POINTERS* info) {
    InitOnceExecuteOnce(&g_open_once, open_log, nullptr, nullptr);
    std::lock_guard<std::mutex> guard(g_state.lock);
    if (!g_state.file) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    const EXCEPTION_RECORD* rec = info->ExceptionRecord;
    void* address = rec->ExceptionAddress;

    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&g_state), &self);

    MEMORY_BASIC_INFORMATION mbi{};
    const char* where = "unknown";
    char module_name[MAX_PATH] = "?";
    unsigned long long rva = 0;

    if (VirtualQuery(address, &mbi, sizeof mbi) && mbi.AllocationBase) {
        if (mbi.AllocationBase == self) {
            where = "inside our shim";
        } else {
            HMODULE owner = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   static_cast<LPCSTR>(mbi.AllocationBase), &owner) &&
                owner) {
                GetModuleFileNameA(owner, module_name, MAX_PATH);
                where = module_name;
                rva = reinterpret_cast<unsigned long long>(address) -
                      reinterpret_cast<unsigned long long>(owner);
            }
        }
    }

    std::fprintf(g_state.file,
                 "!!! fault code=0x%08lx address=%p (%s + %#llx) thread=%lu\n",
                 static_cast<unsigned long>(rec->ExceptionCode), address, where, rva,
                 GetCurrentThreadId());
    std::fflush(g_state.file);
    shim_diag_fault();
    const char* leaf = where;  // "unknown" when no owning module was found
    if (module_name[0]) {
        const char* slash = std::strrchr(module_name, '\\');
        leaf = slash ? slash + 1 : module_name;
    }
    shim_diag_note_locked("!!! fault code=0x%08lx in %s + %#llx thread=%lu",
                          static_cast<unsigned long>(rec->ExceptionCode), leaf, rva,
                          GetCurrentThreadId());
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

// Registered from DllMain through a one-shot initialiser so it is installed
// before the game makes its first call.
static const bool g_fault_hook = [] {
    SetUnhandledExceptionFilter(on_fault);
    return true;
}();
