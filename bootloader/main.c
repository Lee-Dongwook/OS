typedef unsigned long long UINTN;
typedef unsigned long long EFI_STATUS;
typedef void *EFI_HANDLE;

#define EFI_SUCCESS 0

typedef struct {
    unsigned int Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} EFI_GUID;

typedef struct {
    unsigned int RedMask;
    unsigned int GreenMask;
    unsigned int BlueMask;
    unsigned int ReservedMask;
} EFI_PIXEL_BITMASK;

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    unsigned int Version;
    unsigned int HorizontalResolution;
    unsigned int VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    unsigned int PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    unsigned int MaxMode;
    unsigned int Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    unsigned long long FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    void *QueryMode;
    void *SetMode;
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    unsigned long long Signature;
    unsigned int Revision;
    unsigned int HeaderSize;
    unsigned int CRC32;
    unsigned int Reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL;
    void *RestoreTPL;
    void *AllocatePages;
    void *FreePages;
    EFI_STATUS (*GetMemoryMap)(UINTN *MemoryMapSize, void *MemoryMap, UINTN *MapKey, UINTN *DescriptorSize, unsigned int *DescriptorVersion);
    void *AllocatePool;
    void *FreePool;
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    void *HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void *GetNextMonotonicCount;
    void *Stall;
    void *SetWatchdogTimer;
    void *ConnectController;
    void *DisconnectController;
    void *OpenProtocol;
    void *CloseProtocol;
    void *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    void *LocateHandleBuffer;
    EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;
    void *CalculateCrc32;
    void *CopyMem;
    void *SetMem;
    void *CreateEventEx;
} EFI_BOOT_SERVICES;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (*OutputString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const short *String);
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    short *FirmwareVendor;
    unsigned int FirmwareRevision;
    unsigned int _pad;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    void *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

static EFI_GUID gEfiGraphicsOutputProtocolGuid = 
    { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };

// 넉넉하게 16KB로 메모리 맵 버퍼 선언
static unsigned char memory_map_buffer[16384];

typedef struct {
    unsigned int *framebuffer;
    unsigned int width;
    unsigned int height;
    unsigned int pixels_per_scan_line;
} BootInfo;

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS status;

    // 1. GOP (그래픽 프로토콜) 가져오기
    status = SystemTable->BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, 0, (void **)&gop);
    if (status != EFI_SUCCESS) {
        return status;
    }

    // 2. 메모리 맵 수집 및 안전한 ExitBootServices 탈출
    UINTN map_size = sizeof(memory_map_buffer);
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    unsigned int descriptor_version = 0;

    // GetMemoryMap 호출
    status = SystemTable->BootServices->GetMemoryMap(
        &map_size,
        memory_map_buffer,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    // ExitBootServices 시도 (실패 시 최신 map_key를 받아 재시도)
    status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        map_size = sizeof(memory_map_buffer);
        SystemTable->BootServices->GetMemoryMap(
            &map_size,
            memory_map_buffer,
            &map_key,
            &descriptor_size,
            &descriptor_version
        );
        SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    }

    // -------------------------------------------------------------
    // 3. 완벽한 Bare-Metal 상태 진입!
    //    이제 UEFI 서비스는 종료되었으며 직접 하드웨어를 제어합니다.
    // -------------------------------------------------------------

    BootInfo boot_info;
    boot_info.framebuffer = (unsigned int *)gop->Mode->FrameBufferBase;
    boot_info.width = gop->Mode->Info->HorizontalResolution;
    boot_info.height = gop->Mode->Info->VerticalResolution;
    boot_info.pixels_per_scan_line = gop->Mode->Info->PixelsPerScanLine;

    extern void kernel_main(BootInfo * boot_info);

    kernel_main(&boot_info);

    while (1) {
        __asm__ __volatile__("hlt");
    }

    return EFI_SUCCESS;
}
