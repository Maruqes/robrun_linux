// Naming the module a fault landed in, for the one-line log mirror of the
// crash probe's fault line.
//
// Platform-free on purpose: the host suite links this file.
#pragma once

// The file name part of `module_path` (after the last backslash). Falls back to
// `where` when the path is null or empty, so a fault outside every module, or
// inside the shim itself, keeps its description instead of a placeholder.
const char* fault_leaf(const char* module_path, const char* where);
