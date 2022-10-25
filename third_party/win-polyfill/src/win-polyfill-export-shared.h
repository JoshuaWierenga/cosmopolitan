// clang-format off
﻿#ifndef _WIN_POLYFILL_EXPORT_SHARED_H_
#define _WIN_POLYFILL_EXPORT_SHARED_H_

/* This file should be included before any windows header when possible */

#pragma once

#if defined(_WIN32)

#if (WP_SUPPORT_VERSION < NTDDI_WIN6)
#define INITKNOWNFOLDERS
#endif /* (WP_SUPPORT_VERSION < NTDDI_WIN6) */

/* win-polyfill 保证STDIO新老模式兼容 */
#define _CRT_STDIO_ARBITRARY_WIDE_SPECIFIERS

/* Disable include of sockets from Windows.h */
#if defined(_WINSOCKAPI_) || defined(_INC_WINDOWS)
#define _WIN_POLYFILL_WINDOWS_ALREADY_INCLUDED
#define WIN_POLYFILL_DISABLE_SOCKET
#else
#define _WINSOCKAPI_
#endif

#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif

#define PATHCCH_NO_DEPRECATE

#ifndef UMDF_USING_NTSTATUS
#define UMDF_USING_NTSTATUS
#endif

#include "third_party/win-polyfill/src/win-polyfill-version.h"

// MISSING #include <crtdbg.h>
// MISSING #include <intrin.h>

#if !defined(WIN_POLYFILL_DISABLE_SOCKET)
// MISSING #include <winsock2.h>
// MISSING #include <ws2def.h>
// MISSING #include <ws2tcpip.h>
#endif

#include "libc/nt/accounting.h"
#include "libc/nt/automation.h"
#include "libc/nt/console.h"
#include "libc/nt/debug.h"
#include "libc/nt/dll.h"
#include "libc/nt/enum/keyaccess.h"
#include "libc/nt/enum/regtype.h"
#include "libc/nt/errors.h"
#include "libc/nt/events.h"
#include "libc/nt/files.h"
#include "libc/nt/ipc.h"
#include "libc/nt/memory.h"
#include "libc/nt/paint.h"
#include "libc/nt/process.h"
#include "libc/nt/registry.h"
#include "libc/nt/synchronization.h"
#include "libc/nt/thread.h"
#include "libc/nt/windows.h"
#include "libc/nt/winsock.h"

#if !defined(_WIN_POLYFILL_WINDOWS_ALREADY_INCLUDED)
// MISSING #include <ntstatus.h>
// MISSING #include <winnt.h>
#endif

// MISSING #include <d3d11.h>
// MISSING #include <d3d9.h>
// MISSING #include <dbghelp.h>
// MISSING #include <dwmapi.h>
// MISSING #include <dxgi.h>
// MISSING #include <evntprov.h>
// MISSING #include <ncrypt.h>
// MISSING #include <pathcch.h>
// MISSING #include <psapi.h>
// MISSING #include <roapi.h>
// MISSING #include <roerrorapi.h>
// MISSING #include <setupapi.h>
// MISSING #include <shellapi.h>
// MISSING #include <shellscalingapi.h>
// MISSING #include <shlobj.h>
// MISSING #include <shlwapi.h>
// MISSING #include <strsafe.h>
// MISSING #include <threadpoolapiset.h>
// MISSING #include <timezoneapi.h>
// MISSING #include <uxtheme.h>
// MISSING #include <winnls.h>
// MISSING #include <winstring.h>

/* iphlpapi should after them all */
// MISSING #include <iphlpapi.h>

#ifdef __cplusplus
// MISSING #include <dwrite.h>
#endif

#if !defined(WIN_POLYFILL_EXPORT_SHARED) && !defined(WIN_POLYFILL_EXPORT_STATIC)
#if defined(_DLL)
#define WIN_POLYFILL_EXPORT_SHARED_INFERRED
#else
#define WIN_POLYFILL_EXPORT_STATIC
#define WIN_POLYFILL_EXPORT_STATIC_INFERRED
#endif /* defined(_DLL) */
#endif

#if defined(WIN_POLYFILL_EXPORT_STATIC)
#define WP_EXPORT
#elif defined(WIN_POLYFILL_EXPORT_SHARED)
#define WP_EXPORT __declspec(dllexport)
#else
#define WP_EXPORT __declspec(dllimport)
#pragma comment(lib, "win-polyfill.lib")
#endif

#define WP_EXTERN_C EXTERN_C WP_EXPORT

#endif /* defined(_WIN32) */

#endif /* _WIN_POLYFILL_EXPORT_SHARED_H_ */
