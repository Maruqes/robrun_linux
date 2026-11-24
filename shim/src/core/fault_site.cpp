#include "core/fault_site.hpp"

#include <cstring>

const char* fault_leaf(const char* module_path, const char* where) {
    if (!module_path || !module_path[0]) return where;
    const char* slash = std::strrchr(module_path, '\\');
    return slash ? slash + 1 : module_path;
}
