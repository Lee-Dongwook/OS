typedef unsigned long long UINTN;
typedef unsigned long long EFI_STATUS;
typedef void *EFI_HANDLE;

typedef struct {
  char ScanCode;
  char UnicodeChar;
} EFI_INPUT_KEY;

typedef struct EFI_SYSTEM_TABLE {
  char _pad1[44];
  struct {
    char _pad1[8];
    EFI_STATUS (*OutputString)(void *This, const short *String);
  } *ConOut;
  char _pad2[8];
  struct {
    EFI_STATUS (*Reset)(void *This, char ExtendedVerification);
    EFI_STATUS (*ReadKeyStroke)(void *This, EFI_INPUT_KEY *Key);
  } *ConIn;
} EFI_SYSTEM_TABLE;


EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (const short *)L"====================================\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (const short *)L" macOS Reference OS (XNU Kernel)    \r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (const short *)L" UEFI Bootloader Successfully Loaded\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, (const short *)L"====================================\r\n");

  EFI_INPUT_KEY Key;
    while (SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key) != 0);

    return 0;
}
