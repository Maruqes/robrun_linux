#include "abi/eos_result.hpp"

#include <cstring>

namespace eos_result {

bool operation_complete(std::int32_t result) { return result != kOperationWillRetry; }

std::int32_t copy_id_string(const char* id, std::size_t id_size, char* buffer,
                            std::int32_t* length) {
    if (!id || !buffer || !length || *length < 0) return kInvalidParameters;
    const std::int32_t need = static_cast<std::int32_t>(id_size);
    if (*length < need) {
        *length = need;
        return kLimitExceeded;
    }
    std::memcpy(buffer, id, id_size);
    *length = need;
    return kSuccess;
}

}  // namespace eos_result
