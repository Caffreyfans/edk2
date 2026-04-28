#include <Uefi.h>

#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DriverBinding.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include "driver.h"

#define MYGPU_PRIVATE_FROM_GOP(a) \
  BASE_CR(a, MYGPU_PRIVATE, Gop)

EFI_STATUS
EFIAPI
MyGpuGopQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)
{
    MYGPU_PRIVATE *Private = NULL;

    if (This == NULL || SizeOfInfo == NULL || Info == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (ModeNumber != 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    Private = BASE_CR(This, MYGPU_PRIVATE, Gop);

    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    *Info = AllocateCopyPool(*SizeOfInfo, &Private->GopModeInfo);
    if (*Info == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MyGpuGopSetMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber)
{
    MYGPU_PRIVATE *Private = NULL;
    UINT32 Enable;

    if (This == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }
    if (ModeNumber != 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    Private = BASE_CR(This, MYGPU_PRIVATE, Gop);

    Enable = 1;
    Private->PciIo->Mem.Write(Private->PciIo,
                              EfiPciIoWidthUint32,
                              0,
                              REG_ENABLE,
                              1,
                              &Enable);

    Private->GopMode.Mode = ModeNumber;

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MyGpuGopBlt (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL      *This,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL     *BltBuffer OPTIONAL,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
  IN UINTN                             SourceX,
  IN UINTN                             SourceY,
  IN UINTN                             DestinationX,
  IN UINTN                             DestinationY,
  IN UINTN                             Width,
  IN UINTN                             Height,
  IN UINTN                             Delta OPTIONAL
  )
{
  MYGPU_PRIVATE *Private;
  UINT32        *Fb;
  UINTN         Pitch;
  UINTN         X;
  UINTN         Y;
  UINT32        Pixel;
  UINTN         BufferStrideBytes;
  UINT8         *Src8;
  UINT8         *Dst8;

  if (This == NULL || Width == 0 || Height == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Private = MYGPU_PRIVATE_FROM_GOP(This);

  Pitch = Private->GopModeInfo.PixelsPerScanLine;
  Fb = (UINT32 *)(UINTN)Private->FramebufferBase;

  switch (BltOperation) {

  case EfiBltVideoFill:
    if (BltBuffer == NULL) {
      return EFI_INVALID_PARAMETER;
    }

    if (DestinationX + Width > Private->GopModeInfo.HorizontalResolution ||
        DestinationY + Height > Private->GopModeInfo.VerticalResolution) {
      return EFI_INVALID_PARAMETER;
    }

    Pixel =
      ((UINT32)BltBuffer->Blue) |
      ((UINT32)BltBuffer->Green << 8) |
      ((UINT32)BltBuffer->Red << 16) |
      ((UINT32)BltBuffer->Reserved << 24);

    for (Y = 0; Y < Height; Y++) {
      UINT32 *Row = Fb + (DestinationY + Y) * Pitch + DestinationX;
      for (X = 0; X < Width; X++) {
        Row[X] = Pixel;
      }
    }

    return EFI_SUCCESS;

  case EfiBltBufferToVideo:
    if (BltBuffer == NULL) {
      return EFI_INVALID_PARAMETER;
    }

    if (DestinationX + Width > Private->GopModeInfo.HorizontalResolution ||
        DestinationY + Height > Private->GopModeInfo.VerticalResolution) {
      return EFI_INVALID_PARAMETER;
    }

    if (Delta == 0) {
      BufferStrideBytes = Width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    } else {
      BufferStrideBytes = Delta;
    }

    Src8 = (UINT8 *)BltBuffer +
           SourceY * BufferStrideBytes +
           SourceX * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);

    for (Y = 0; Y < Height; Y++) {
      EFI_GRAPHICS_OUTPUT_BLT_PIXEL *SrcPixel;
      UINT32 *DstPixel;

      SrcPixel = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)(Src8 + Y * BufferStrideBytes);
      DstPixel = Fb + (DestinationY + Y) * Pitch + DestinationX;

      for (X = 0; X < Width; X++) {
        DstPixel[X] =
          ((UINT32)SrcPixel[X].Blue) |
          ((UINT32)SrcPixel[X].Green << 8) |
          ((UINT32)SrcPixel[X].Red << 16) |
          ((UINT32)SrcPixel[X].Reserved << 24);
      }
    }

    return EFI_SUCCESS;

  case EfiBltVideoToBltBuffer:
    if (BltBuffer == NULL) {
      return EFI_INVALID_PARAMETER;
    }

    if (SourceX + Width > Private->GopModeInfo.HorizontalResolution ||
        SourceY + Height > Private->GopModeInfo.VerticalResolution) {
      return EFI_INVALID_PARAMETER;
    }

    if (Delta == 0) {
      BufferStrideBytes = Width * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    } else {
      BufferStrideBytes = Delta;
    }

    Dst8 = (UINT8 *)BltBuffer +
           DestinationY * BufferStrideBytes +
           DestinationX * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);

    for (Y = 0; Y < Height; Y++) {
      UINT32 *SrcPixel;
      EFI_GRAPHICS_OUTPUT_BLT_PIXEL *DstPixel;

      SrcPixel = Fb + (SourceY + Y) * Pitch + SourceX;
      DstPixel = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)(Dst8 + Y * BufferStrideBytes);

      for (X = 0; X < Width; X++) {
        DstPixel[X].Blue     = (UINT8)(SrcPixel[X] & 0xff);
        DstPixel[X].Green    = (UINT8)((SrcPixel[X] >> 8) & 0xff);
        DstPixel[X].Red      = (UINT8)((SrcPixel[X] >> 16) & 0xff);
        DstPixel[X].Reserved = (UINT8)((SrcPixel[X] >> 24) & 0xff);
      }
    }

    return EFI_SUCCESS;

  case EfiBltVideoToVideo:
    if (SourceX + Width > Private->GopModeInfo.HorizontalResolution ||
        SourceY + Height > Private->GopModeInfo.VerticalResolution ||
        DestinationX + Width > Private->GopModeInfo.HorizontalResolution ||
        DestinationY + Height > Private->GopModeInfo.VerticalResolution) {
      return EFI_INVALID_PARAMETER;
    }

    if (DestinationY > SourceY) {
      for (Y = Height; Y > 0; Y--) {
        UINT32 *SrcRow = Fb + (SourceY + Y - 1) * Pitch + SourceX;
        UINT32 *DstRow = Fb + (DestinationY + Y - 1) * Pitch + DestinationX;
        CopyMem(DstRow, SrcRow, Width * sizeof(UINT32));
      }
    } else {
      for (Y = 0; Y < Height; Y++) {
        UINT32 *SrcRow = Fb + (SourceY + Y) * Pitch + SourceX;
        UINT32 *DstRow = Fb + (DestinationY + Y) * Pitch + DestinationX;
        CopyMem(DstRow, SrcRow, Width * sizeof(UINT32));
      }
    }

    return EFI_SUCCESS;

  default:
    return EFI_UNSUPPORTED;
  }
}