// EOS 1.19.2 result codes and the id-to-string copy contract of the shim.
#include "abi/eos_result.hpp"

#include <cstdio>
#include <cstring>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (condition) return;
    std::printf("  FAIL: %s\n", what);
    ++failures;
}

const char kId[] = "00000000A1B2C3D4A1B2C3D400000001";

}  // namespace

int main() {
    check(eos_result::kInvalidParameters == 10, "EOS_InvalidParameters is 10, not 1");
    check(eos_result::kOperationWillRetry == 19, "EOS_OperationWillRetry is 19, not 8");
    check(eos_result::kLimitExceeded == 22, "EOS_LimitExceeded is 22");
    check(eos_result::kLoginStatusLoggedIn == 2, "EOS_ELoginStatus_LoggedIn is 2");

    check(!eos_result::operation_complete(19), "will-retry is incomplete");
    check(eos_result::operation_complete(8), "too-many-requests (8) is complete");
    check(eos_result::operation_complete(0), "success is complete");

    char buffer[64];
    std::int32_t length = sizeof buffer;
    check(eos_result::copy_id_string(kId, sizeof kId, buffer, &length) == 0, "copy succeeds");
    check(length == 33, "length counts the NUL");
    check(std::strcmp(buffer, kId) == 0, "the id is copied with its NUL");

    length = 32;
    std::memset(buffer, 'x', sizeof buffer);
    check(eos_result::copy_id_string(kId, sizeof kId, buffer, &length) == 22,
          "a short buffer answers LimitExceeded");
    check(length == 33, "a short buffer reports the size needed");
    check(buffer[0] == 'x', "a short buffer is not written");

    length = 33;
    check(eos_result::copy_id_string(kId, sizeof kId, nullptr, &length) == 10,
          "a null buffer answers InvalidParameters");
    check(eos_result::copy_id_string(kId, sizeof kId, buffer, nullptr) == 10,
          "a null length answers InvalidParameters");
    length = -1;
    check(eos_result::copy_id_string(kId, sizeof kId, buffer, &length) == 10,
          "a negative length answers InvalidParameters");

    if (failures) {
        std::printf("eos_result_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("eos_result_test: ok\n");
    return 0;
}
