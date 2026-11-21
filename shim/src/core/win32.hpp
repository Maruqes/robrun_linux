// One place that defines the Windows version and thin includes. The build
// scripts also pass -D_WIN32_WINNT, which wins when a standard header comes first.
#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
