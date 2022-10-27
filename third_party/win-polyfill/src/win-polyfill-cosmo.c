/* clang-format off */

#include "libc/isystem/windows.h"
#include "libc/nt/ntdll.h"
#include "libc/nt/enum/fileinfobyhandleclass.h"
#include "libc/nt/enum/fileinformationclass.h"
#include "libc/nt/enum/objectinformationclass.h"
#include "libc/nt/enum/status.h"
#include "libc/nt/nt/file.h"
#include "libc/nt/runtime.h"
#include "libc/nt/struct/fileattributetaginformation.h"
#include "libc/nt/struct/filecompressioninfo.h"
#include "libc/nt/struct/filenameinformation.h"
#include "libc/nt/struct/filestandardinformation.h"
#include "libc/nt/struct/filestreaminformation.h"
#include "libc/nt/struct/objectnameinformation.h"
#include "libc/nt/struct/peb.h"
#include "libc/nt/struct/teb.h"
#include "libc/str/str.h"

// This is very similar to NtFileBothDirectoryInformation/FILE_BOTH_DIR_INFORMATION but that is missing FileId
struct NtFileIoBothDirectoryInformation {
    DWORD         NextEntryOffset;
  DWORD         FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  DWORD         FileAttributes;
  DWORD         FileNameLength;
  DWORD         EaSize;
  CCHAR         ShortNameLength;
  WCHAR         ShortName[12];
  LARGE_INTEGER FileId;
  WCHAR         FileName[1];
};

// TODO Add FileRemoteProtocolInformation to libc/nt/enum/fileinformationclass.h
#define kNtFileRemoteProtocolInformation 55

struct NtFileRemoteProtocolInformation {
  USHORT StructureVersion;
  USHORT StructureSize;
  ULONG  Protocol;
  USHORT ProtocolMajorVersion;
  USHORT ProtocolMinorVersion;
  USHORT ProtocolRevision;
  USHORT Reserved;
  ULONG  Flags;
  struct {
    ULONG Reserved[8];
  } GenericReserved;
  struct {
    ULONG Reserved[16];
  } ProtocolSpecificReserved;
  union {
    struct {
      struct {
        ULONG Capabilities;
      } Server;
      struct {
        ULONG Capabilities;
        ULONG ShareFlags;
        ULONG CachingFlags;
      } Share;
    } Smb2;
    ULONG Reserved[16];
  } ProtocolSpecific;
};


static DWORD NtStatusToDosError(NTSTATUS Status)
{
    if (kNtStatusTimeout == Status)
    {
        /*
        https://github.com/Chuyu-Team/win-polyfill/issues/10

        用户报告，Windows XP 无法转换 STATUS_TIMEOUT。实际结果也是rubin，因此，特殊处理一下。
        */
        return kNtErrorTimeout;
    }

    return RtlNtStatusToDosError(Status);
}

static DWORD BaseSetLastNTError(NTSTATUS Status)
{
    NTSTATUS lStatus = RtlNtStatusToDosError(Status);
    SetLastError(lStatus);
    return lStatus;
}

#define MAX_PATH 260

// For some reason the cosmo version gives a not defined by direct deps error despite kernel32 being listed as a dep
DWORD GetLongPathName(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer) {
    const HINSTANCE nt_dll = LoadLibrary(u"ntdll");
    BOOL (*fun_GetLongPathName)(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer) = GetProcAddress(nt_dll, "GetLongPathName");

	FreeLibrary(nt_dll);

    return fun_GetLongPathName(lpszShortPath, lpszLongPath, cchBuffer);
}

// For some reason the cosmo version gives a not defined by direct deps error despite kernel32 being listed as a dep
BOOL GetVolumeNameForVolumeMountPointW(LPCWSTR lpszVolumeMountPoint, LPWSTR lpszVolumeName, DWORD cchBufferLength) {
    const HINSTANCE nt_dll = LoadLibrary(u"ntdll");
    BOOL (*fun_GetVolumeNameForVolumeMountPointW)(LPCWSTR lpszVolumeMountPoint, LPWSTR lpszVolumeName, DWORD cchBufferLength) = GetProcAddress(nt_dll, "GetVolumeNameForVolumeMountPointW");

	FreeLibrary(nt_dll);

    return fun_GetVolumeNameForVolumeMountPointW(lpszVolumeMountPoint, lpszVolumeName, cchBufferLength);
}

// For some reason the cosmo version gives a not defined by direct deps error despite kernel32 being listed as a dep
BOOL GetVolumePathNamesForVolumeNameW(LPCWSTR lpszVolumeName, LPWCH lpszVolumePathNames, DWORD cchBufferLength, PDWORD lpcchReturnLength) {
	const HINSTANCE nt_dll = LoadLibrary(u"ntdll");
    BOOL (*fun_GetVolumePathNamesForVolumeNameW)(LPCWSTR lpszVolumeName, LPWCH lpszVolumePathNames, DWORD cchBufferLength, PDWORD lpcchReturnLength) = GetProcAddress(nt_dll, "GetVolumePathNamesForVolumeNameW");

	FreeLibrary(nt_dll);

    return fun_GetVolumePathNamesForVolumeNameW(lpszVolumeName, lpszVolumePathNames, cchBufferLength, lpcchReturnLength);
}

static BOOL BasepGetVolumeGUIDFromNTName(
    const UNICODE_STRING *NtName,
    char16_t szVolumeGUID[MAX_PATH])
{
#define __szVolumeMountPointPrefix__ L"\\\\?\\GLOBALROOT"

    // 一个设备名称 512 长度够多了吧？
    char16_t szVolumeMountPoint[512];

    // 检查缓冲区是否充足
    size_t cbBufferNeed = sizeof(__szVolumeMountPointPrefix__) + NtName->Length;

    if (cbBufferNeed > sizeof(szVolumeMountPoint))
    {
        SetLastError(kNtErrorNotEnoughMemory);
        return FALSE;
    }

    __builtin_memcpy(
        szVolumeMountPoint,
        __szVolumeMountPointPrefix__,
        sizeof(__szVolumeMountPointPrefix__) - sizeof(__szVolumeMountPointPrefix__[0]));
    __builtin_memcpy(
        (char *)szVolumeMountPoint + sizeof(__szVolumeMountPointPrefix__) -
            sizeof(__szVolumeMountPointPrefix__[0]),
        NtName->Data,
        NtName->Length);

    szVolumeMountPoint[cbBufferNeed / 2 - 1] = L'\0';

    return GetVolumeNameForVolumeMountPointW(szVolumeMountPoint, szVolumeGUID, MAX_PATH);

#undef __szVolumeMountPointPrefix__
}

static BOOL BasepGetVolumeDosLetterNameFromNTName(
    const UNICODE_STRING *NtName,
    char16_t szVolumeDosLetter[MAX_PATH])
{
    char16_t szVolumeName[MAX_PATH];

    if (!BasepGetVolumeGUIDFromNTName(NtName, szVolumeName))
    {
        return FALSE;
    }

    DWORD cchVolumePathName = 0;

    if (!GetVolumePathNamesForVolumeNameW(
            szVolumeName, szVolumeDosLetter + 4, MAX_PATH - 4, &cchVolumePathName))
    {
        return FALSE;
    }

    szVolumeDosLetter[0] = L'\\';
    szVolumeDosLetter[1] = L'\\';
    szVolumeDosLetter[2] = L'?';
    szVolumeDosLetter[3] = L'\\';

    return TRUE;
}

// TODO Add to libc/nt/enum/status.h
#define kNtStatusBufferOverflow           0x80000005

// TODO Use libc/nt/kernel32/GetFileInformationByHandleEx.s
bool32 (*wp_get_GetFileInformationByHandleEx())()
{
    // What is this set to if the function is found to not exist at runtime?
    //return &GetFileInformationByHandleEx;
    
	const HINSTANCE nt_dll = LoadLibrary(u"ntdll");
    bool32 (*fun_GetFileInformationByHandleEx)() = GetProcAddress(nt_dll, "GetFileInformationByHandleEx");

	FreeLibrary(nt_dll);
	return fun_GetFileInformationByHandleEx;
}

// TODO Use libc/nt/kernel32/GetFinalPathNameByHandleW.s
uint32_t (*wp_get_GetFinalPathNameByHandleW())(int64_t hFile, char16_t *out_path,
                                  uint32_t arraylen, uint32_t flags)
{
    // What is this set to if the function is found to not exist at runtime?
    //return &GGetFinalPathNameByHandleW;

	const HINSTANCE nt_dll = LoadLibrary(u"ntdll");
    uint32_t (*fun_GetFinalPathNameByHandleW)(int64_t hFile, char16_t *out_path,
                                  uint32_t arraylen, uint32_t flags) = GetProcAddress(nt_dll, "GetFinalPathNameByHandleW");

	FreeLibrary(nt_dll);
	return fun_GetFinalPathNameByHandleW;
}


bool32 wp_GetFileInformationByHandleEx(int64_t hFile,
                                    uint32_t FileInformationClass,
                                    void *out_lpFileInformation,
                                    uint32_t dwBufferSize) {
    bool32 (*const pGetFileInformationByHandleEx)() = wp_get_GetFileInformationByHandleEx();
    if (pGetFileInformationByHandleEx != NULL)
    {
        return pGetFileInformationByHandleEx(
            hFile, FileInformationClass, out_lpFileInformation, dwBufferSize);
    }
    uint32_t NtFileInformationClass;
    DWORD cbMinBufferSize;
    bool bNtQueryDirectoryFile = false;
    BOOLEAN RestartScan = false;

    switch (FileInformationClass)
    {
    case FileBasicInfo:
        NtFileInformationClass = kNtFileBasicInformation;
        cbMinBufferSize = sizeof(FILE_BASIC_INFO);
        break;
    case FileStandardInfo:
        NtFileInformationClass = kNtFileStandardInformation;
        cbMinBufferSize = sizeof(FILE_STANDARD_INFO);
        break;
    case FileNameInfo:
        NtFileInformationClass = kNtFileNameInformation;
        cbMinBufferSize = sizeof(FILE_NAME_INFORMATION);
        break;
    case FileStreamInfo:
        NtFileInformationClass = kNtFileStreamInformation;
        cbMinBufferSize = sizeof(FILE_STREAM_INFORMATION);
        break;
    case FileCompressionInfo:
        NtFileInformationClass = kNtFileCompressionInformation;
        cbMinBufferSize = sizeof(struct NtFileCompressionInfo);
        break;
    case FileAttributeTagInfo:
        NtFileInformationClass = kNtFileAttributeTagInformation;
        cbMinBufferSize = sizeof(FILE_ATTRIBUTE_TAG_INFORMATION);
        break;
    case FileIdBothDirectoryRestartInfo:
        RestartScan = true;
    case FileIdBothDirectoryInfo:
        NtFileInformationClass = kNtFileIdBothDirectoryInformation;
        cbMinBufferSize = sizeof(struct NtFileIoBothDirectoryInformation);
        bNtQueryDirectoryFile = true;
        break;
    case FileRemoteProtocolInfo:
        NtFileInformationClass = kNtFileRemoteProtocolInformation;
        cbMinBufferSize = sizeof(struct NtFileRemoteProtocolInformation);
        break;
    default:
        SetLastError(kNtErrorInvalidPassword);
        return FALSE;
        break;
    }

    if (cbMinBufferSize > dwBufferSize)
    {
        SetLastError(kNtErrorBadLength);
        return FALSE;
    }

    struct NtIoStatusBlock IoStatusBlock;
    NTSTATUS Status;

    if (bNtQueryDirectoryFile)
    {
        Status = NtQueryDirectoryFile(
            hFile,
            NULL,
            NULL,
            NULL,
            &IoStatusBlock,
            out_lpFileInformation,
            dwBufferSize,
            NtFileInformationClass,
            false,
            NULL,
            RestartScan);

        if (kNtStatusPending == Status)
        {
            if (WaitForSingleObjectEx(hFile, 0, FALSE) == WAIT_FAILED)
            {
                // WaitForSingleObjectEx会设置LastError
                return FALSE;
            }

            Status = IoStatusBlock.Status;
        }
    }
    else
    {
        Status = NtQueryInformationFile(
            hFile, &IoStatusBlock, out_lpFileInformation, dwBufferSize, NtFileInformationClass);
    }

    if (Status >= kNtStatusSuccess)
    {
        if (FileStreamInfo == FileInformationClass && IoStatusBlock.Information == 0)
        {
            SetLastError(kNtErrorHandleEof);
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
    else
    {
        BaseSetLastNTError(Status);

        return FALSE;
    }
}

uint32_t wp_GetFinalPathNameByHandle(int64_t hFile, char16_t *out_path,
                                  uint32_t arraylen, uint32_t flags) {
    uint32_t (*const pGetFinalPathNameByHandleW)(int64_t hFile, char16_t *out_path,
                                  uint32_t arraylen, uint32_t flags) = wp_get_GetFinalPathNameByHandleW();
    if (pGetFinalPathNameByHandleW != NULL)
    {
        return pGetFinalPathNameByHandleW(hFile, out_path, arraylen, flags);
    }

    // 参数检查
    if (INVALID_HANDLE_VALUE == hFile)
    {
        SetLastError(kNtErrorInvalidHandle);
        return 0;
    }

    switch (flags & (kNtVolumeNameDos | kNtVolumeNameGuid | kNtVolumeNameNone | kNtVolumeNameNt))
    {
    case kNtVolumeNameDos:
        break;
    case kNtVolumeNameGuid:
        break;
    case kNtVolumeNameNt:
        break;
    case kNtVolumeNameNone:
        break;
    default:
        SetLastError(kNtErrorInvalidParameter);
        return 0;
        break;
    }

    UNICODE_STRING VolumeNtName = {};

    char16_t szVolumeRoot[MAX_PATH];
    szVolumeRoot[0] = L'\0';

    char16_t *szLongPathNameBuffer = NULL;

    // 目标所需的分区名称，不包含最后的 '\\'
    UNICODE_STRING TargetVolumeName = {};
    // 目标所需的文件名，开始包含 '\\'
    UNICODE_STRING TargetFileName = {};

    const uint64_t ProcessHeap = ((struct NtPeb *)NtGetPeb())->ProcessHeap;
    uint32_t lStatus = ERROR_SUCCESS;
    DWORD cchReturn = 0;

    OBJECT_NAME_INFORMATION *pObjectName = NULL;
    ULONG cbObjectName = 528;

    FILE_NAME_INFORMATION *pFileNameInfo = NULL;
    ULONG cbFileNameInfo = 528;

    for (;;)
    {
        if (pObjectName)
        {
            OBJECT_NAME_INFORMATION* pNewBuffer =
                (OBJECT_NAME_INFORMATION *)HeapReAlloc(ProcessHeap, 0, pObjectName, cbObjectName);

            if (!pNewBuffer)
            {
                lStatus = kNtErrorNotEnoughMemory;
                goto __Exit;
            }

            pObjectName = pNewBuffer;
        }
        else
        {
            pObjectName = (OBJECT_NAME_INFORMATION *)HeapAlloc(ProcessHeap, 0, cbObjectName);

            if (!pObjectName)
            {
                // 内存不足？
                lStatus = kNtErrorNotEnoughMemory;
                goto __Exit;
            }
        }

        NtStatus Status =
            NtQueryObject((void*)hFile, kNtObjectNameInformation, pObjectName, cbObjectName, &cbObjectName);

        if (kNtStatusBufferOverflow == Status)
        {
            continue;
        }
        else if (Status < 0)
        {
            lStatus = NtStatusToDosError(Status);

            goto __Exit;
        }
        else
        {
            break;
        }
    }

    for (;;)
    {
        if (pFileNameInfo)
        {
            FILE_NAME_INFORMATION* pNewBuffer =
                (FILE_NAME_INFORMATION *)HeapReAlloc(ProcessHeap, 0, pFileNameInfo, cbFileNameInfo);
            if (!pNewBuffer)
            {
                lStatus = kNtErrorNotEnoughMemory;
                goto __Exit;
            }

            pFileNameInfo = pNewBuffer;
        }
        else
        {
            pFileNameInfo = (FILE_NAME_INFORMATION *)HeapAlloc(ProcessHeap, 0, cbFileNameInfo);

            if (!pFileNameInfo)
            {
                // 内存不足？
                lStatus = kNtErrorNotEnoughMemory;
                goto __Exit;
            }
        }

        struct NtIoStatusBlock IoStatusBlock;

        NtStatus Status = NtQueryInformationFile(
            hFile, &IoStatusBlock, pFileNameInfo, cbFileNameInfo, kNtFileNameInformation);

        if (kNtStatusBufferOverflow == Status)
        {
            cbFileNameInfo = pFileNameInfo->FileNameLength + sizeof(FILE_NAME_INFORMATION);
            continue;
        }
        else if (Status < 0)
        {
            lStatus = NtStatusToDosError(Status);

            goto __Exit;
        }
        else
        {
            break;
        }
    }

    if (pFileNameInfo->FileName[0] != '\\')
    {
        lStatus = kNtErrorAccessDenied;
        goto __Exit;
    }

    if (pFileNameInfo->FileNameLength >= pObjectName->Name.Length)
    {
        lStatus = kNtErrorBadPathname;
        goto __Exit;
    }

    VolumeNtName.Data = pObjectName->Name.Data;
    VolumeNtName.Length = VolumeNtName.MaxLength =
        pObjectName->Name.Length - pFileNameInfo->FileNameLength + sizeof(char16_t);

    if (kNtVolumeNameNt & flags)
    {
        // 返回NT路径
        TargetVolumeName.Data = VolumeNtName.Data;
        TargetVolumeName.Length = TargetVolumeName.MaxLength =
            VolumeNtName.Length - sizeof(char16_t);
    }
    else if (kNtVolumeNameNone & flags)
    {
        // 仅返回文件名
    }
    else
    {
        if (kNtVolumeNameGuid & flags)
        {
            // 返回分区GUID名称
            if (!BasepGetVolumeGUIDFromNTName(&VolumeNtName, szVolumeRoot))
            {
                lStatus = GetLastError();
                goto __Exit;
            }
        }
        else
        {
            // 返回Dos路径
            if (!BasepGetVolumeDosLetterNameFromNTName(&VolumeNtName, szVolumeRoot))
            {
                lStatus = GetLastError();
                goto __Exit;
            }
        }

        TargetVolumeName.Data = szVolumeRoot;
        TargetVolumeName.Length = TargetVolumeName.MaxLength =
            (strlen16(szVolumeRoot) - 1) * sizeof(szVolumeRoot[0]);
    }

    // 将路径进行规范化
    if ((kNtFileNameOpened & flags) == 0)
    {
        // 由于 Windows XP不支持 FileNormalizedNameInformation，所以我们直接调用 GetLongPathNameW
        // 获取完整路径。

        DWORD cbszVolumeRoot = TargetVolumeName.Length;

        if (szVolumeRoot[0] == L'\0')
        {
            // 转换分区信息

            if (!BasepGetVolumeDosLetterNameFromNTName(&VolumeNtName, szVolumeRoot))
            {
                lStatus = GetLastError();

                if (lStatus == kNtErrorNotEnoughMemory)
                    goto __Exit;

                if (!BasepGetVolumeGUIDFromNTName(&VolumeNtName, szVolumeRoot))
                {
                    lStatus = GetLastError();
                    goto __Exit;
                }
            }

            cbszVolumeRoot = (strlen16(szVolumeRoot) - 1) * sizeof(szVolumeRoot[0]);
        }

        size_t cbLongPathNameBufferSize = cbszVolumeRoot + pFileNameInfo->FileNameLength + 1024;

        szLongPathNameBuffer = (char16_t *)HeapAlloc(ProcessHeap, 0, cbLongPathNameBufferSize);
        if (!szLongPathNameBuffer)
        {
            lStatus = kNtErrorNotEnoughMemory;
            goto __Exit;
        }

        size_t cchLongPathNameBufferSize = cbLongPathNameBufferSize / sizeof(szLongPathNameBuffer[0]);

        __builtin_memcpy(szLongPathNameBuffer, szVolumeRoot, cbszVolumeRoot);
        __builtin_memcpy(
            (char *)szLongPathNameBuffer + cbszVolumeRoot,
            pFileNameInfo->FileName,
            pFileNameInfo->FileNameLength);
        szLongPathNameBuffer[(cbszVolumeRoot + pFileNameInfo->FileNameLength) / sizeof(char16_t)] =
            L'\0';

        for (;;)
        {
            DWORD result = GetLongPathName(
                szLongPathNameBuffer, szLongPathNameBuffer, cchLongPathNameBufferSize);

            if (result == 0)
            {
                // 失败
                lStatus = GetLastError();
                goto __Exit;
            }
            else if (result >= cchLongPathNameBufferSize)
            {
                cchLongPathNameBufferSize = result + 1;

                char16_t* pNewLongPathName = (char16_t *)HeapReAlloc(
                    ProcessHeap,
                    0,
                    szLongPathNameBuffer,
                    cchLongPathNameBufferSize * sizeof(char16_t));
                if (!pNewLongPathName)
                {
                    lStatus = kNtErrorNotEnoughMemory;
                    goto __Exit;
                }

                szLongPathNameBuffer = pNewLongPathName;
            }
            else
            {
                // 转换成功
                TargetFileName.Data = (char16_t *)((char *)szLongPathNameBuffer + cbszVolumeRoot);
                TargetFileName.Length = TargetFileName.MaxLength =
                    result * sizeof(char16_t) - cbszVolumeRoot;
                break;
            }
        }
    }
    else
    {
        // 直接返回原始路径
        TargetFileName.Data = pFileNameInfo->FileName;
        TargetFileName.Length = TargetFileName.MaxLength = pFileNameInfo->FileNameLength;
    }

    // 返回结果，根目录 + 文件名 的长度
    cchReturn = (TargetVolumeName.Length + TargetFileName.Length) / sizeof(char16_t);

    if (arraylen <= cchReturn)
    {
        // 长度不足……

        cchReturn += 1;
    }
    else
    {
        // 复制根目录
        __builtin_memcpy(out_path, TargetVolumeName.Data, TargetVolumeName.Length);
        // 复制文件名
        __builtin_memcpy(
            (char *)out_path + TargetVolumeName.Length,
            TargetFileName.Data,
            TargetFileName.Length);
        // 保证字符串 '\0' 截断
        out_path[cchReturn] = L'\0';
    }

__Exit:
    if (pFileNameInfo)
        HeapFree(ProcessHeap, 0, pFileNameInfo);
    if (pObjectName)
        HeapFree(ProcessHeap, 0, pObjectName);
    if (szLongPathNameBuffer)
        HeapFree(ProcessHeap, 0, szLongPathNameBuffer);

    if (lStatus != ERROR_SUCCESS)
    {
        SetLastError(lStatus);
        return 0;
    }
    else
    {
        return cchReturn;
    }
}
