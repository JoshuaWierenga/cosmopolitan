/* clang-format off */

#include "libc/intrin/safemacros.internal.h"
#include "libc/nt/dll.h"
#include "libc/nt/enum/fileinfobyhandleclass.h"
#include "libc/nt/enum/fileinformationclass.h"
#include "libc/nt/enum/objectinformationclass.h"
#include "libc/nt/enum/status.h"
#include "libc/nt/errors.h"
#include "libc/nt/files.h"
#include "libc/nt/memory.h"
#include "libc/nt/nt/file.h"
#include "libc/nt/ntdll.h"
#include "libc/nt/process.h"
#include "libc/nt/runtime.h"
#include "libc/nt/struct/fdset.h"
#include "libc/nt/struct/fileattributetaginformation.h"
#include "libc/nt/struct/filecompressioninfo.h"
#include "libc/nt/struct/filenameinformation.h"
#include "libc/nt/struct/filestandardinformation.h"
#include "libc/nt/struct/filestreaminformation.h"
#include "libc/nt/struct/objectnameinformation.h"
#include "libc/nt/struct/peb.h"
#include "libc/nt/struct/pollfd.h"
#include "libc/nt/struct/teb.h"
#include "libc/nt/struct/timeval.h"
#include "libc/nt/synchronization.h"
#include "libc/nt/version.h"
#include "libc/nt/winsock.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/poll.h"

#define MAX_PATH 260

#define INVALID_SOCKET -1ULL
#define SOCKET_ERROR -1
#define WAIT_FAILED -1U

// TODO Add to libc/nt/version.h
#define IsAtLeastWindowsVista() (GetNtMajorVersion() >= 6)

// TODO Add to libc/nt/enum/status.h
#define kNtStatusBufferOverflow           0x80000005

// TODO Add to libc/nt/enum/fileinformationclass.h
#define kNtFileRemoteProtocolInformation 55

// TODO Create libc/nt/struct/fileremoteprotocolinfomation.h and move this to it
struct NtFileRemoteProtocolInformation {
  unsigned short StructureVersion;
  unsigned short StructureSize;
  uint32_t  Protocol;
  unsigned short ProtocolMajorVersion;
  unsigned short ProtocolMinorVersion;
  unsigned short ProtocolRevision;
  unsigned short Reserved;
  uint32_t  Flags;
  struct {
    uint32_t Reserved[8];
  } GenericReserved;
  struct {
    uint32_t Reserved[16];
  } ProtocolSpecificReserved;
  union {
    struct {
      struct {
        uint32_t Capabilities;
      } Server;
      struct {
        uint32_t Capabilities;
        uint32_t ShareFlags;
        uint32_t CachingFlags;
      } Share;
    } Smb2;
    uint32_t Reserved[16];
  } ProtocolSpecific;
};

// This is very similar to NtFileBothDirectoryInformation/FILE_BOTH_DIR_INFORMATION but that is missing FileId
// TODO Create libc/nt/struct/fileiobothdirectoryinformation.h and move this to it
struct NtFileIoBothDirectoryInformation {
  uint32_t NextEntryOffset;
  uint32_t FileIndex;
  int64_t CreationTime;
  int64_t LastAccessTime;
  int64_t LastWriteTime;
  int64_t ChangeTime;
  int64_t EndOfFile;
  int64_t AllocationSize;
  uint32_t FileAttributes;
  uint32_t FileNameLength;
  uint32_t EaSize;
  char ShortNameLength;
  char16_t ShortName[12];
  int64_t FileId;
  char16_t FileName[1];
};


static uint32_t NtStatusToDosError(int32_t Status)
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

static uint32_t BaseSetLastNTError(int32_t Status)
{
    int32_t lStatus = RtlNtStatusToDosError(Status);
    SetLastError(lStatus);
    return lStatus;
}


static bool32 BasepGetVolumeGUIDFromNTName(
    const struct NtUnicodeString *NtName,
    char16_t szVolumeGUID[MAX_PATH])
{
#define __szVolumeMountPointPrefix__ u"\\\\?\\GLOBALROOT"

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

    szVolumeMountPoint[cbBufferNeed / 2 - 1] = u'\0';

    return GetVolumeNameForVolumeMountPoint(szVolumeMountPoint, szVolumeGUID, MAX_PATH);

#undef __szVolumeMountPointPrefix__
}

static bool32 BasepGetVolumeDosLetterNameFromNTName(
    const struct NtUnicodeString *NtName,
    char16_t szVolumeDosLetter[MAX_PATH])
{
    char16_t szVolumeName[MAX_PATH];

    // GetVolumeNameForVolumeMountPoint and hence GetVolumePathNamesForVolumeName only work if NtName doesn't represent a UNC path
    if (!BasepGetVolumeGUIDFromNTName(NtName, szVolumeName))
	{
		if (GetLastError() == kNtErrorNotEnoughMemory)
		{
			return FALSE;
		}

		szVolumeDosLetter[4] = u'U';
		szVolumeDosLetter[5] = u'N';
		szVolumeDosLetter[6] = u'C';
		szVolumeDosLetter[7] = u'\\';
	}
	else
	{
		uint32_t cchVolumePathName = 0;

		if (!GetVolumePathNamesForVolumeName(
		    szVolumeName, szVolumeDosLetter + 4, MAX_PATH - 4, &cchVolumePathName))
		{
			return FALSE;
		}
    }

    szVolumeDosLetter[0] = u'\\';
    szVolumeDosLetter[1] = u'\\';
    szVolumeDosLetter[2] = u'?';
    szVolumeDosLetter[3] = u'\\';

    return TRUE;
}


#define SetupNativeWindowsFunction(library, returnType, funcName, ...) \
typedef returnType __attribute__((__ms_abi__)) (*funcName##Func)(__VA_ARGS__);\
funcName##Func p##funcName = NULL;\
\
funcName##Func get_##funcName() {\
	if (p##funcName) {\
		return p##funcName;\
	}\
\
	int64_t libraryDll = LoadLibrary(u"" #library);\
	if (!libraryDll) {\
		return NULL;\
	}\
\
	p##funcName = (funcName##Func)GetProcAddress(libraryDll, #funcName);\
\
	FreeLibrary(libraryDll);\
	return p##funcName;\
}


#define CallNativeWindowsFunction(check, funcName, setLastErrorAndReturn, returnValue, ...) \
if (check) {\
    funcName##Func p##funcName = get_##funcName();\
    if (p##funcName != NULL) {\
        return p##funcName(__VA_ARGS__);\
    }\
\
    if (setLastErrorAndReturn) {\
        SetLastError(kNtErrorProcNotFound);\
        return returnValue;\
    }\
}


SetupNativeWindowsFunction(Kernel32.dll, bool32, GetFileInformationByHandleEx, int64_t hFile,
                                                                               uint32_t FileInformationClass,
                                                                               void *out_lpFileInformation,
                                                                               uint32_t dwBufferSize)

SetupNativeWindowsFunction(Kernel32.dll, uint32_t, GetFinalPathNameByHandleW, int64_t hFile, char16_t *out_path,
                                                                              uint32_t arraylen, uint32_t flags)

SetupNativeWindowsFunction(Kernel32.dll, uint32_t, CreateSymbolicLinkW, const char16_t *lpSymlinkFileName,
                                                                        const char16_t *lpTargetPathName, uint32_t dwFlags)

SetupNativeWindowsFunction(Kernel32.dll, bool32, GetVolumeInformationByHandleW, int64_t hFile,
                                                                                char16_t *opt_out_lpVolumeNameBuffer,
                                                                                uint32_t nVolumeNameSize,
                                                                                uint32_t *opt_out_lpVolumeSerialNumber,
                                                                                uint32_t *opt_out_lpMaximumComponentLength,
                                                                                uint32_t *opt_out_lpFileSystemFlags,
                                                                                char16_t *opt_out_lpFileSystemNameBuffer,
                                                                                uint32_t nFileSystemNameSize)

SetupNativeWindowsFunction(Ws2_32.dll, int, WSAPoll, struct sys_pollfd_nt *inout_fdArray, uint32_t nfds,
                                                     signed timeout_ms)

SetupNativeWindowsFunction(Ws2_32.dll, int, __WSAFDIsSet, uint64_t unnamedParam1,
                                                          struct NtFdSet *unnamedParam2)
// Kernel32.dll polyfills

bool32 GetFileInformationByHandleEx(int64_t hFile,
                                    uint32_t FileInformationClass,
                                    void *out_lpFileInformation,
                                    uint32_t dwBufferSize) {
    CallNativeWindowsFunction(IsAtLeastWindowsVista(), GetFileInformationByHandleEx,
                              true, false,
                              hFile, FileInformationClass, out_lpFileInformation, dwBufferSize)

    uint32_t NtFileInformationClass;
    uint32_t cbMinBufferSize;
    bool bNtQueryDirectoryFile = false;
    bool32 RestartScan = false;

    switch (FileInformationClass)
    {
    case kNtFileBasicInfo:
        NtFileInformationClass = kNtFileBasicInformation;
        cbMinBufferSize = sizeof(struct NtFileBasicInformation);
        break;
    case kNtFileStandardInfo:
        NtFileInformationClass = kNtFileStandardInformation;
        cbMinBufferSize = sizeof(struct NtFileStandardInformation);
        break;
    case kNtFileNameInfo:
        NtFileInformationClass = kNtFileNameInformation;
        cbMinBufferSize = sizeof(struct NtFileNameInformation);
        break;
    case kNtFileStreamInfo:
        NtFileInformationClass = kNtFileStreamInformation;
        cbMinBufferSize = sizeof(struct NtFileStreamInformation);
        break;
    case kNtFileCompressionInfo:
        NtFileInformationClass = kNtFileCompressionInformation;
        cbMinBufferSize = sizeof(struct NtFileCompressionInfo);
        break;
    case kNtFileAttributeTagInfo:
        NtFileInformationClass = kNtFileAttributeTagInformation;
        cbMinBufferSize = sizeof(struct NtFileAttributeTagInformation);
        break;
    case kNtFileIdBothDirectoryRestartInfo:
        RestartScan = true;
    case kNtFileIdBothDirectoryInfo:
        NtFileInformationClass = kNtFileIdBothDirectoryInformation;
        cbMinBufferSize = sizeof(struct NtFileIoBothDirectoryInformation);
        bNtQueryDirectoryFile = true;
        break;
    case kNtFileRemoteProtocolInfo:
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
    int32_t Status;

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
        if (kNtFileStreamInfo == FileInformationClass && IoStatusBlock.Information == 0)
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

uint32_t GetFinalPathNameByHandle(int64_t hFile, char16_t *out_path,
                                  uint32_t arraylen, uint32_t flags) {
    CallNativeWindowsFunction(IsAtLeastWindowsVista(), GetFinalPathNameByHandleW,
                              true, 0,
                              hFile, out_path, arraylen, flags)

    // 参数检查
    if (kNtInvalidHandleValue == hFile)
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

    struct NtUnicodeString VolumeNtName = {0, 0, 0};

    char16_t szVolumeRoot[MAX_PATH] = {0};

    char16_t *szLongPathNameBuffer = NULL;

    // 目标所需的分区名称，不包含最后的 '\\'
    struct NtUnicodeString TargetVolumeName = {0, 0, 0};
    // 目标所需的文件名，开始包含 '\\'
    struct NtUnicodeString TargetFileName = {0, 0, 0};

    const uint64_t ProcessHeap = ((struct NtPeb *)NtGetPeb())->ProcessHeap;
    uint32_t lStatus = kNtErrorSuccess;
    uint32_t cchReturn = 0;

    struct NtObjectNameInformation *pObjectName = NULL;
    uint32_t cbObjectName = 528;

    struct NtFileNameInformation *pFileNameInfo = NULL;
    uint32_t cbFileNameInfo = 528;

    for (;;)
    {
        if (pObjectName)
        {
            struct NtObjectNameInformation* pNewBuffer =
                (struct NtObjectNameInformation *)HeapReAlloc(ProcessHeap, 0, pObjectName, cbObjectName);

            if (!pNewBuffer)
            {
                lStatus = kNtErrorNotEnoughMemory;
                goto __Exit;
            }

            pObjectName = pNewBuffer;
        }
        else
        {
            pObjectName = (struct NtObjectNameInformation *)HeapAlloc(ProcessHeap, 0, cbObjectName);

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
            struct NtFileNameInformation* pNewBuffer =
                (struct NtFileNameInformation *)HeapReAlloc(ProcessHeap, 0, pFileNameInfo, cbFileNameInfo);
            if (!pNewBuffer)
            {
                lStatus = kNtErrorNotEnoughMemory;
                goto __Exit;
            }

            pFileNameInfo = pNewBuffer;
        }
        else
        {
            pFileNameInfo = (struct NtFileNameInformation *)HeapAlloc(ProcessHeap, 0, cbFileNameInfo);

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
            cbFileNameInfo = pFileNameInfo->FileNameLength + sizeof(struct NtFileNameInformation);
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

        uint32_t cbszVolumeRoot = TargetVolumeName.Length;

        if (szVolumeRoot[0] == u'\0')
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
            u'\0';

        for (;;)
        {
            uint32_t result = GetLongPathName(
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
        out_path[cchReturn] = u'\0';
    }

__Exit:
    if (pFileNameInfo)
        HeapFree(ProcessHeap, 0, pFileNameInfo);
    if (pObjectName)
        HeapFree(ProcessHeap, 0, pObjectName);
    if (szLongPathNameBuffer)
        HeapFree(ProcessHeap, 0, szLongPathNameBuffer);

    if (lStatus != kNtErrorSuccess)
    {
        SetLastError(lStatus);
        return 0;
    }
    else
    {
        return cchReturn;
    }
}

bool32 CreateSymbolicLink(const char16_t *lpSymlinkFileName,
                          const char16_t *lpTargetPathName, uint32_t dwFlags) {
    CallNativeWindowsFunction(IsAtLeastWindowsVista(), CreateSymbolicLinkW,
                              true, false,
                              lpSymlinkFileName, lpTargetPathName, dwFlags)

    SetLastError(kNtErrorInvalidFunction);

    return FALSE;
}

// This is not actually from win-polyfill, I just needed somewhere to put this.
// It is based on GetFinalPathNameByHandle anyway.
bool32 GetVolumeInformationByHandle(int64_t hFile,
                                    char16_t *opt_out_lpVolumeNameBuffer,
                                    uint32_t nVolumeNameSize,
                                    uint32_t *opt_out_lpVolumeSerialNumber,
                                    uint32_t *opt_out_lpMaximumComponentLength,
                                    uint32_t *opt_out_lpFileSystemFlags,
                                    char16_t *opt_out_lpFileSystemNameBuffer,
                                    uint32_t nFileSystemNameSize) {
    CallNativeWindowsFunction(IsAtLeastWindowsVista(), GetVolumeInformationByHandleW,
                              true, false,
                              hFile,
                              opt_out_lpVolumeNameBuffer,
                              nVolumeNameSize,
                              opt_out_lpVolumeSerialNumber,
                              opt_out_lpMaximumComponentLength,
                              opt_out_lpFileSystemFlags,
                              opt_out_lpFileSystemNameBuffer,
                              nFileSystemNameSize)

    struct NtUnicodeString VolumeNtName = {0, 0, 0};

    char16_t szVolumePathName[MAX_PATH] = {0};

    const uint64_t ProcessHeap = ((struct NtPeb *)NtGetPeb())->ProcessHeap;
    uint32_t lStatus = kNtErrorSuccess;
    bool32 result = false;

    struct NtObjectNameInformation *pObjectName = NULL;
    uint32_t cbObjectName = 528;

    for (;;)
    {
        if (pObjectName)
        {
            struct NtObjectNameInformation* pNewBuffer =
                (struct NtObjectNameInformation *)HeapReAlloc(ProcessHeap, 0, pObjectName, cbObjectName);

            if (!pNewBuffer)
            {
                lStatus = kNtErrorNotEnoughMemory;
                goto __Exit;
            }

            pObjectName = pNewBuffer;
        }
        else
        {
            pObjectName = (struct NtObjectNameInformation *)HeapAlloc(ProcessHeap, 0, cbObjectName);

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

    GetVolumePathName(pObjectName->Name.Data, szVolumePathName, MAX_PATH);

    result = GetVolumeInformation(szVolumePathName,
	                              opt_out_lpVolumeNameBuffer,
	                              nVolumeNameSize,
	                              opt_out_lpVolumeSerialNumber,
	                              opt_out_lpMaximumComponentLength,
	                              opt_out_lpFileSystemFlags,
	                              opt_out_lpFileSystemNameBuffer,
	                              nFileSystemNameSize);

	if (!result)
	{
		lStatus = GetLastError();
	}

__Exit:
    if (pObjectName)
        HeapFree(ProcessHeap, 0, pObjectName);

    if (lStatus != kNtErrorSuccess)
    {
        SetLastError(lStatus);
        return false;
    }
    else
    {
        return result;
    }
}


// Ws2_32.dll polyfills

static int poll_win32_select(struct sys_pollfd_nt* fdArray, uint32_t fds, int timeout)
{
    uintptr_t *fd_set_memory = NULL;
    uint32_t i = 0;
    int count = 0;
    struct NtFdSet *readfds = NULL;
    struct NtFdSet *writefds = NULL;
    struct NtFdSet *exceptfds = NULL;
    struct NtTimeval *__ptimeout = NULL;
    struct NtTimeval time_out;
    int result = -1;

    for (count = 0; count < 2; ++count)
    {
        int read_count = 0;
        int write_count = 0;
        int except_count = 0;
        for (i = 0; i < fds; i++)
        {
            struct sys_pollfd_nt *fd = fdArray + i;
            if (fd->handle == INVALID_SOCKET)
            {
                continue;
            }

            // Read (in) socket
            if (fd->events & (POLLRDNORM | POLLRDBAND | POLLPRI))
            {
                if (readfds != NULL)
                {
                    readfds->fd_array[read_count] = fd->handle;
                }
                ++read_count;
            }

            // Write (out) socket
            if (fd->events & (POLLWRNORM | POLLWRBAND))
            {
                if (writefds != NULL)
                {
                    writefds->fd_array[write_count] = fd->handle;
                }
                ++write_count;
            }

            // Exception
            if (fd->events & (POLLERR | POLLHUP | POLLNVAL))
            {
                if (exceptfds != NULL)
                {
                    exceptfds->fd_array[except_count] = fd->handle;
                }
                ++except_count;
            }
        }

        if (fd_set_memory == NULL)
        {
            size_t mem_size = 0;
            read_count = max(64, read_count);
            write_count = max(64, write_count);
            except_count = max(64, except_count);
            mem_size = ((read_count + write_count + except_count) + 3) * sizeof(uintptr_t);
            fd_set_memory = (uintptr_t *)HeapAlloc(GetProcessHeap(), 0, mem_size);
        }
        else
        {
            readfds->fd_count = read_count;
            writefds->fd_count = write_count;
            exceptfds->fd_count = except_count;
            break;
        }
        if (fd_set_memory == NULL)
        {
            return -1;
        }
        readfds = (struct NtFdSet *)fd_set_memory;
        writefds = (struct NtFdSet *)(fd_set_memory + (read_count + 1));
        exceptfds = (struct NtFdSet *)(fd_set_memory + (read_count + 1) + (write_count + 1));
    }

    /*
    timeout  < 0 , infinite
    timeout == 0 , nonblock
    timeout  > 0 , the time to wait
    */
    if (timeout >= 0)
    {
        time_out.tv_sec = timeout / 1000;
        time_out.tv_usec = (timeout % 1000) * 1000;
        __ptimeout = &time_out;
    }

    result = __sys_select_nt(1, readfds, writefds, exceptfds, __ptimeout);

    if (result > 0)
    {
        for (i = 0; i < fds; i++)
        {
            struct sys_pollfd_nt *fd = fdArray + i;
            fd->revents = 0;
            if (fd->handle == INVALID_SOCKET)
            {
                continue;
            }
            if (p__WSAFDIsSet(fd->handle, readfds))
            {
                fd->revents |= fd->events & // NOLINT
                               (POLLRDNORM | POLLRDBAND | POLLPRI);
            }

            if (p__WSAFDIsSet(fd->handle, writefds))
            {
                fd->revents |= fd->events & // NOLINT
                               (POLLWRNORM | POLLWRBAND);
            }

            if (p__WSAFDIsSet(fd->handle, exceptfds))
            {
                fd->revents |= fd->events & // NOLINT
                               (POLLERR | POLLHUP | POLLNVAL);
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, fd_set_memory);
    return result;
}

int32_t WSAPoll(struct sys_pollfd_nt *inout_fdArray, uint32_t nfds,
                signed timeout_ms)
{
    CallNativeWindowsFunction(IsAtLeastWindowsVista(), WSAPoll,
                              false, 0,
                              inout_fdArray, nfds, timeout_ms)
    if (IsAtLeastWindowsVista() || !get___WSAFDIsSet()) {
        // If we get here then the above call failed or are on xp and
        // are unable to find __WSAFDIsSet()
        WSASetLastError(WSASYSNOTREADY);
        return SOCKET_ERROR;
    }

    return poll_win32_select(inout_fdArray, nfds, timeout_ms);
}
