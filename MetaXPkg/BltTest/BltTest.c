#include <Uefi.h>

#include <Protocol/GraphicsOutput.h>

#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#define RECT_W  160
#define RECT_H  100

STATIC
VOID
SetPixel (
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Pixel,
  IN  UINT8 Red,
  IN  UINT8 Green,
  IN  UINT8 Blue
  )
{
  Pixel->Red      = Red;
  Pixel->Green    = Green;
  Pixel->Blue     = Blue;
  Pixel->Reserved = 0;
}

INTN
EFIAPI
ShellAppMain (
  IN UINTN   Argc,
  IN CHAR16  **Argv
  )
{
  EFI_STATUS                         Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL       *Gop;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      Color;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *Buffer;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL      ReadBack;
  UINTN                              X;
  UINTN                              Y;
  UINTN                              Width;
  UINTN                              Height;
  UINTN                              RectX;
  UINTN                              RectY;
  UINTN                              CopyX;
  UINTN                              CopyY;

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

  Width  = Gop->Mode->Info->HorizontalResolution;
  Height = Gop->Mode->Info->VerticalResolution;

  Print(L"GOP: Mode=%u MaxMode=%u Resolution=%ux%u\n",
        Gop->Mode->Mode,
        Gop->Mode->MaxMode,
        Width,
        Height);

  //
  // 1. Test EfiBltVideoFill: fill full screen background.
  //
  SetPixel(&Color, 0x00, 0x20, 0x80);  // dark blue

  Status = Gop->Blt(
                  Gop,
                  &Color,
                  EfiBltVideoFill,
                  0,
                  0,
                  0,
                  0,
                  Width,
                  Height,
                  0
                  );
  Print(L"Test 1 EfiBltVideoFill full screen: %r\n", Status);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  gBS->Stall(500 * 1000);

  //
  // 2. Test EfiBltBufferToVideo: draw a red rectangle from memory buffer.
  //
  Buffer = AllocateZeroPool(RECT_W * RECT_H * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (Buffer == NULL) {
    Print(L"Allocate Buffer failed\n");
    return EFI_OUT_OF_RESOURCES;
  }

  for (Y = 0; Y < RECT_H; Y++) {
    for (X = 0; X < RECT_W; X++) {
      //
      // Red rectangle with a small green gradient, useful to detect stride bugs.
      //
      Buffer[Y * RECT_W + X].Red      = 0xFF;
      Buffer[Y * RECT_W + X].Green    = (UINT8)(X & 0xFF);
      Buffer[Y * RECT_W + X].Blue     = 0x00;
      Buffer[Y * RECT_W + X].Reserved = 0x00;
    }
  }

  RectX = 40;
  RectY = 40;

  Status = Gop->Blt(
                  Gop,
                  Buffer,
                  EfiBltBufferToVideo,
                  0,
                  0,
                  RectX,
                  RectY,
                  RECT_W,
                  RECT_H,
                  0
                  );
  Print(L"Test 2 EfiBltBufferToVideo red rectangle: %r\n", Status);
  if (EFI_ERROR(Status)) {
    FreePool(Buffer);
    return Status;
  }

  gBS->Stall(500 * 1000);

  //
  // 3. Test EfiBltVideoToBltBuffer: read one pixel back from the rectangle.
  //
  ZeroMem(&ReadBack, sizeof(ReadBack));

  Status = Gop->Blt(
                  Gop,
                  &ReadBack,
                  EfiBltVideoToBltBuffer,
                  RectX,
                  RectY,
                  0,
                  0,
                  1,
                  1,
                  0
                  );

  Print(L"Test 3 EfiBltVideoToBltBuffer readback: %r\n", Status);
  Print(L"ReadBack Pixel: R=0x%02x G=0x%02x B=0x%02x Reserved=0x%02x\n",
        ReadBack.Red,
        ReadBack.Green,
        ReadBack.Blue,
        ReadBack.Reserved);

  if (!EFI_ERROR(Status)) {
    if (ReadBack.Red == 0xFF && ReadBack.Blue == 0x00) {
      Print(L"ReadBack basic check: PASS\n");
    } else {
      Print(L"ReadBack basic check: FAIL\n");
    }
  }

  gBS->Stall(500 * 1000);

  //
  // 4. Test EfiBltVideoToVideo: copy the red rectangle to another position.
  //
  CopyX = Width  > RECT_W + 80 ? Width  - RECT_W - 40 : 220;
  CopyY = Height > RECT_H + 80 ? Height - RECT_H - 40 : 180;

  Status = Gop->Blt(
                  Gop,
                  NULL,
                  EfiBltVideoToVideo,
                  RectX,
                  RectY,
                  CopyX,
                  CopyY,
                  RECT_W,
                  RECT_H,
                  0
                  );

  Print(L"Test 4 EfiBltVideoToVideo copy rectangle: %r\n", Status);

  FreePool(Buffer);

  Print(L"\nExpected screen:\n");
  Print(L"  - dark blue background\n");
  Print(L"  - red/gradient rectangle at left-top\n");
  Print(L"  - copied red/gradient rectangle near right-bottom\n");

  return Status;
}