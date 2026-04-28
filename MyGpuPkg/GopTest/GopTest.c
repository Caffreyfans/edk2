#include <Uefi.h>

#include <Protocol/GraphicsOutput.h>

#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color;
  UINT32                        *Fb;
  UINTN                         Index;
  UINTN                         PixelCount;
  UINT32                        Pixel;

  Print(L"GopTest start\n");

  Status = gBS->LocateProtocol(
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  (VOID **)&Gop
                  );

  Print(L"Locate GOP Status=%r Gop=%p\n", Status, Gop);

  if (EFI_ERROR(Status)) {
    return Status;
  }

  Print(L"Mode=%u MaxMode=%u\n", Gop->Mode->Mode, Gop->Mode->MaxMode);
  Print(L"Resolution=%ux%u PixelsPerScanLine=%u\n",
        Gop->Mode->Info->HorizontalResolution,
        Gop->Mode->Info->VerticalResolution,
        Gop->Mode->Info->PixelsPerScanLine);

  Print(L"FramebufferBase=0x%lx FramebufferSize=0x%lx\n",
        Gop->Mode->FrameBufferBase,
        Gop->Mode->FrameBufferSize);

  //
  // Test 1: use GOP Blt VideoFill.
  //
  Color.Red      = 0x00;
  Color.Green    = 0x80;
  Color.Blue     = 0xff;
  Color.Reserved = 0x00;

  Status = Gop->Blt(
                  Gop,
                  &Color,
                  EfiBltVideoFill,
                  0,
                  0,
                  0,
                  0,
                  Gop->Mode->Info->HorizontalResolution,
                  Gop->Mode->Info->VerticalResolution,
                  0
                  );

  Print(L"GOP Blt Fill Status=%r\n", Status);

  //
  // Test 2: directly write framebuffer.
  // PixelBlueGreenRedReserved8BitPerColor:
  // byte0 Blue, byte1 Green, byte2 Red, byte3 Reserved.
  //
  Fb = (UINT32 *)(UINTN)Gop->Mode->FrameBufferBase;
  Pixel = 0x000080ff;

  PixelCount =
    Gop->Mode->Info->PixelsPerScanLine *
    Gop->Mode->Info->VerticalResolution;

  Print(L"Direct write Pixel=0x%08x PixelCount=%u\n", Pixel, PixelCount);

  for (Index = 0; Index < PixelCount; Index++) {
    Fb[Index] = Pixel;
  }

  Print(L"After direct write Fb[0]=0x%08x Fb[1]=0x%08x\n",
        Fb[0],
        Fb[1]);

  Print(L"GopTest done\n");

  return EFI_SUCCESS;
}