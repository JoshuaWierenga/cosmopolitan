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
#include "libc/nt/version.h"
#include "libc/str/str.h"

#define MAX_PATH 260

// TODO Add to libc/nt/version.h
#define IsAtLeastWindowsVista() (GetNtMajorVersion() >= 6)

// TODO Add to libc/nt/enum/status.h
#define kNtStatusBufferOverflow           0x80000005

// TODO Add to libc/nt/enum/fileinformationclass.h
#define kNtFileRemoteProtocolInformation 55

// TODO Create libc/nt/struct/fileremoteprotocolinfomation.h and move this to it
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

// This is very similar to NtFileBothDirectoryInformation/FILE_BOTH_DIR_INFORMATION but that is missing FileId
// TODO Create libc/nt/struct/fileiobothdirectoryinformation.h and move this to it
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


static BOOL BasepGetVolumeGUIDFromNTName(
    const UNICODE_STRING *NtName,
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

static BOOL BasepGetVolumeDosLetterNameFromNTName(
    const UNICODE_STRING *NtName,
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
		DWORD cchVolumePathName = 0;

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


typedef bool32 __attribute__((__ms_abi__)) (*GetFileInformationByHandleExFunc)(int64_t hFile,
                                                                               uint32_t FileInformationClass,
                                                                               void *out_lpFileInformation,
                                                                               uint32_t dwBufferSize);

typedef uint32_t __attribute__((__ms_abi__)) (*GetFinalPathNameByHandleWFunc)(int64_t hFile, char16_t *out_path,
                                                                              uint32_t arraylen, uint32_t flags);

typedef uint32_t __attribute__((__ms_abi__)) (*CreateSymbolicLinkWFunc)(const char16_t *lpSymlinkFileName,
                                                                   const char16_t *lpTargetPathName, uint32_t dwFlags);

GetFileInformationByHandleExFunc pGetFileInformationByHandleEx = NULL;

GetFinalPathNameByHandleWFunc pGetFinalPathNameByHandleW = NULL;

CreateSymbolicLinkWFunc pCreateSymbolicLinkW = NULL;

GetFileInformationByHandleExFunc get_GetFileInformationByHandleEx()
{
	if (pGetFileInformationByHandleEx)
	{
		return pGetFileInformationByHandleEx;
	}

	HINSTANCE kernel32Dll = LoadLibrary(u"Kernel32.dll");
	if (kernel32Dll == NULL)
	{
		return NULL;
	}

	pGetFileInformationByHandleEx = GetProcAddress(kernel32Dll, "GetFileInformationByHandleEx");

	FreeLibrary(kernel32Dll);
	return pGetFileInformationByHandleEx;
}

GetFinalPathNameByHandleWFunc get_GetFinalPathNameByHandleW()
{
	if (pGetFinalPathNameByHandleW)
	{
		return pGetFinalPathNameByHandleW;
	}

	HINSTANCE kernel32Dll = LoadLibrary(u"Kernel32.dll");
	if (kernel32Dll == NULL)
	{
		return NULL;
	}

	pGetFinalPathNameByHandleW = GetProcAddress(kernel32Dll, "GetFinalPathNameByHandleW");

	FreeLibrary(kernel32Dll);
	return pGetFinalPathNameByHandleW;
}

CreateSymbolicLinkWFunc get_CreateSymbolicLinkW()
{
	if (pCreateSymbolicLinkW)
	{
		return pCreateSymbolicLinkW;
	}

	HINSTANCE kernel32Dll = LoadLibrary(u"Kernel32.dll");
	if (kernel32Dll == NULL)
	{
		return NULL;
	}

	pCreateSymbolicLinkW = GetProcAddress(kernel32Dll, "CreateSymbolicLinkW");

	FreeLibrary(kernel32Dll);
	return pCreateSymbolicLinkW;
}


bool32 GetFileInformationByHandleEx(int64_t hFile,
                                    uint32_t FileInformationClass,
                                    void *out_lpFileInformation,
                                    uint32_t dwBufferSize) {
    if (IsAtLeastWindowsVista()) 
    {
        GetFileInformationByHandleExFunc pGetFileInformationByHandleEx = get_GetFileInformationByHandleEx();
        if (pGetFileInformationByHandleEx != NULL)
        {
            return pGetFileInformationByHandleEx(hFile, FileInformationClass, out_lpFileInformation, dwBufferSize);
        }
        return 0;
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

uint32_t GetFinalPathNameByHandle(int64_t hFile, char16_t *out_path,
                                  uint32_t arraylen, uint32_t flags) {
    if (IsAtLeastWindowsVista()) 
    {
        GetFinalPathNameByHandleWFunc pGetFinalPathNameByHandleW = get_GetFinalPathNameByHandleW();
        if (pGetFinalPathNameByHandleW != NULL)
        {
            return pGetFinalPathNameByHandleW(hFile, out_path, arraylen, flags);
        }
        return 0;
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

    UNICODE_STRING VolumeNtName = {0, 0, 0};

    char16_t szVolumeRoot[MAX_PATH] = {0};

    char16_t *szLongPathNameBuffer = NULL;

    // 目标所需的分区名称，不包含最后的 '\\'
    UNICODE_STRING TargetVolumeName = {0, 0, 0};
    // 目标所需的文件名，开始包含 '\\'
    UNICODE_STRING TargetFileName = {0, 0, 0};

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
        out_path[cchReturn] = u'\0';
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

bool32 CreateSymbolicLink(const char16_t *lpSymlinkFileName,
                          const char16_t *lpTargetPathName, uint32_t dwFlags) {
    if (IsAtLeastWindowsVista()) 
    {
        CreateSymbolicLinkWFunc pCreateSymbolicLinkW = get_CreateSymbolicLinkW();
        if (pCreateSymbolicLinkW != NULL)
        {
            return pCreateSymbolicLinkW(lpSymlinkFileName, lpTargetPathName, dwFlags);
        }
        return 0;
    }

    SetLastError(kNtErrorInvalidFunction);

    return FALSE;
}
