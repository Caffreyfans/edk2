#include <Uefi.h>

#include <Protocol/GraphicsOutput.h>

#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>

INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
  EFI_STATUS Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  UINTN SizeOfInfo;
  UINTN ModeNumber;
  UINTN Index;

  Gop = NULL;

  Status = gBS->LocateProtocol(
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  (VOID **)&Gop
                  );

  if (EFI_ERROR(Status)) {
    Print(L"Locate GOP failed: %r\n", Status);
    return Status;
  }

  Print(L"GOP found: MaxMode=%u CurrentMode=%u\n",
        Gop->Mode->MaxMode,
        Gop->Mode->Mode);

  Print(L"Available modes:\n");

  for (Index = 0; Index < Gop->Mode->MaxMode; Index++) {
    Info = NULL;
    SizeOfInfo = 0;

    Status = Gop->QueryMode(
                    Gop,
                    (UINT32)Index,
                    &SizeOfInfo,
                    &Info
                    );

    if (!EFI_ERROR(Status) && Info != NULL) {
      Print(L"  %u: %ux%u, PixelsPerScanLine=%u\n",
            Index,
            Info->HorizontalResolution,
            Info->VerticalResolution,
            Info->PixelsPerScanLine);

      FreePool(Info);
    }
  }

  if (Argc < 2) {
    Print(L"\nUsage:\n");
    Print(L"  SetModeTest.efi <mode_number>\n");
    Print(L"\nExample:\n");
    Print(L"  SetModeTest.efi 1\n");
    return EFI_SUCCESS;
  }

  ModeNumber = StrDecimalToUintn(Argv[1]);

  if (ModeNumber >= Gop->Mode->MaxMode) {
    Print(L"Invalid mode number: %u\n", ModeNumber);
    return EFI_INVALID_PARAMETER;
  }

  Print(L"\nSwitching to mode %u...\n", ModeNumber);

  Status = Gop->SetMode(Gop, (UINT32)ModeNumber);

  Print(L"SetMode(%u) Status=%r\n", ModeNumber, Status);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  Print(L"Current Mode=%u\n", Gop->Mode->Mode);
  Print(L"Resolution=%ux%u\n",
        Gop->Mode->Info->HorizontalResolution,
        Gop->Mode->Info->VerticalResolution);
  Print(L"FrameBufferBase=0x%lx FrameBufferSize=0x%lx\n",
        Gop->Mode->FrameBufferBase,
        Gop->Mode->FrameBufferSize);

  return EFI_SUCCESS;
}