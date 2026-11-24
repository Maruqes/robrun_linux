// The module name on the crash probe's log line.
#include "core/fault_site.hpp"

#include <cstdio>
#include <cstring>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (condition) return;
    std::printf("  FAIL: %s\n", what);
    ++failures;
}

}  // namespace

int main() {
    check(std::strcmp(fault_leaf("C:\\Game\\GameAssembly.dll", "x"), "GameAssembly.dll") == 0,
          "a module path names its file");
    check(std::strcmp(fault_leaf("", "inside our shim"), "inside our shim") == 0,
          "a fault in the shim keeps its description, not a placeholder");
    check(std::strcmp(fault_leaf("", "unknown"), "unknown") == 0,
          "a fault outside every module reads unknown");
    check(std::strcmp(fault_leaf(nullptr, "unknown"), "unknown") == 0, "null path is safe");
    check(std::strcmp(fault_leaf("plain.dll", "x"), "plain.dll") == 0, "a bare name stays");

    if (failures) {
        std::printf("fault_site_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("fault_site_test: ok\n");
    return 0;
}
