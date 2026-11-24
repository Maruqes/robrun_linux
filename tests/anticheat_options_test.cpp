// EOS_AntiCheatClient_ReceiveMessageFromServerOptions: the length sits at +4
// and the pointer at +8. Reading the length at +16 reads past what the game
// passes, and a zero there silently drops every server message.
#include "abi/anticheat_options.hpp"

#include <cstddef>
#include <cstdio>

namespace {

using Options = eos_ac::ReceiveMessageFromServerOptions;

static_assert(offsetof(Options, ApiVersion) == 0, "ApiVersion is the first field");
static_assert(offsetof(Options, DataLengthBytes) == 4, "the length follows the version");
static_assert(offsetof(Options, Data) == 8, "the pointer follows the length");
static_assert(sizeof(Options) == 16, "16 bytes, the size the game allocates");

int failures = 0;

void check(bool condition, const char* what) {
    if (condition) return;
    std::printf("  FAIL: %s\n", what);
    ++failures;
}

}  // namespace

int main() {
    check(offsetof(Options, DataLengthBytes) == 4, "the length is read at +4");
    check(offsetof(Options, Data) == 8, "the payload pointer is read at +8");
    check(sizeof(Options) == 16, "nothing is read past the 16 bytes the game passes");

    if (failures) {
        std::printf("anticheat_options_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("anticheat_options_test: ok\n");
    return 0;
}
