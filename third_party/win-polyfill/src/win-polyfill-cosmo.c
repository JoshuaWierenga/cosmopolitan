/* clang-format off */

#include "libc/isystem/windows.h"
#include "libc/nt/ntdll.h"
#include "libc/nt/enum/fileinfobyhandleclass.h"
#include "libc/nt/enum/fileinformationclass.h"
#include "libc/nt/enum/status.h"
#include "libc/nt/nt/file.h"
#include "libc/nt/struct/fileattributetaginformation.h"
#include "libc/nt/struct/filecompressioninfo.h"
#include "libc/nt/struct/filenameinformation.h"
#include "libc/nt/struct/filestandardinformation.h"
#include "libc/nt/struct/filestreaminformation.h"

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

static DWORD BaseSetLastNTError(NTSTATUS Status)
{
    NTSTATUS lStatus = RtlNtStatusToDosError(Status);
    SetLastError(lStatus);
    return lStatus;
}

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