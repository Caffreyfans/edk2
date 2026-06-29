/** @file
  Graphics Output Protocol functions for the Metax video controller.

  Copyright (c) 2007 - 2018, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MetaXGpu.h"

#include <Library/DxeServicesTableLib.h>

STATIC
VOID
MetaXVideoFlush(
    IN METAX_VIDEO_PRIVATE_DATA *Private,
    IN UINTN X,
    IN UINTN Y,
    IN UINTN Width,
    IN UINTN Height)
{
  MetaXPciWrite(Private, REG_FLUSH_X, (UINT32)X);
  MetaXPciWrite(Private, REG_FLUSH_Y, (UINT32)Y);
  MetaXPciWrite(Private, REG_FLUSH_WIDTH, (UINT32)Width);
  MetaXPciWrite(Private, REG_FLUSH_HEIGHT, (UINT32)Height);
  MetaXPciWrite(Private, REG_FLUSH, 0);
}

STATIC
VOID MetaXVideoCompleteModeInfo(
    IN METAX_VIDEO_MODE_DATA *ModeData,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info)
{
  Info->Version = 0;
  if (ModeData->ColorDepth == 8)
  {
    Info->PixelFormat = PixelBitMask;
    Info->PixelInformation.RedMask = PIXEL_RED_MASK;
    Info->PixelInformation.GreenMask = PIXEL_GREEN_MASK;
    Info->PixelInformation.BlueMask = PIXEL_BLUE_MASK;
    Info->PixelInformation.ReservedMask = 0;
  }
  else if (ModeData->ColorDepth == 24)
  {
    Info->PixelFormat = PixelBitMask;
    Info->PixelInformation.RedMask = PIXEL24_RED_MASK;
    Info->PixelInformation.GreenMask = PIXEL24_GREEN_MASK;
    Info->PixelInformation.BlueMask = PIXEL24_BLUE_MASK;
    Info->PixelInformation.ReservedMask = 0;
  }
  else if (ModeData->ColorDepth == 32)
  {
    Info->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
    Info->PixelInformation.RedMask = 0;
    Info->PixelInformation.GreenMask = 0;
    Info->PixelInformation.BlueMask = 0;
    Info->PixelInformation.ReservedMask = 0;
  }
  else
  {
    DEBUG((DEBUG_ERROR, "%a: Invalid ColorDepth %u", __func__, ModeData->ColorDepth));
    ASSERT(FALSE);
  }

  Info->PixelsPerScanLine = Info->HorizontalResolution;
}

STATIC
EFI_STATUS
MetaXVideoCompleteModeData(
    IN METAX_VIDEO_PRIVATE_DATA *Private,
    OUT EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode)
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *FrameBufDesc;
  METAX_VIDEO_MODE_DATA *ModeData;
  // EFI_STATUS Status;

  ModeData = &Private->ModeData[Mode->Mode];
  Info = Mode->Info;
  MetaXVideoCompleteModeInfo(ModeData, Info);

  Private->PciIo->GetBarAttributes(
      Private->PciIo,
      PCI_BAR_IDX1,
      NULL,
      (VOID **)&FrameBufDesc);

  Mode->FrameBufferBase = FrameBufDesc->AddrRangeMin;
  Mode->FrameBufferSize = Info->HorizontalResolution * Info->VerticalResolution;
  Mode->FrameBufferSize = Mode->FrameBufferSize * ((ModeData->ColorDepth + 7) / 8);
  Mode->FrameBufferSize = EFI_PAGES_TO_SIZE(
      EFI_SIZE_TO_PAGES(Mode->FrameBufferSize));
  Print(L"FrameBufferBase: 0x%Lx, FrameBufferSize: 0x%Lx\n",
        Mode->FrameBufferBase,
        (UINT64)Mode->FrameBufferSize);

  // Status = gDS->SetMemorySpaceCapabilities(
  //     FrameBufDesc->AddrRangeMin,
  //     FrameBufDesc->AddrLen,
  //     EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_XP);
  // ASSERT_EFI_ERROR(Status);

  // Status = gDS->SetMemorySpaceAttributes(
  //     FrameBufDesc->AddrRangeMin,
  //     FrameBufDesc->AddrLen,
  //     EFI_MEMORY_WC | EFI_MEMORY_XP);
  // ASSERT_EFI_ERROR(Status);
  // Print(L"end\n");
  FreePool(FrameBufDesc);
  return EFI_SUCCESS;
}

//
// Graphics Output Protocol Member Functions
//
EFI_STATUS
EFIAPI
MetaXVideoGraphicsOutputQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)

/*++

Routine Description:

  Graphics Output protocol interface to query video mode

  Arguments:
    This                  - Protocol instance pointer.
    ModeNumber            - The mode number to return information on.
    Info                  - Caller allocated buffer that returns information about ModeNumber.
    SizeOfInfo            - A pointer to the size, in bytes, of the Info buffer.

  Returns:
    EFI_SUCCESS           - Mode information returned.
    EFI_BUFFER_TOO_SMALL  - The Info buffer was too small.
    EFI_DEVICE_ERROR      - A hardware error occurred trying to retrieve the video mode.
    EFI_NOT_STARTED       - Video display is not initialized. Call SetMode ()
    EFI_INVALID_PARAMETER - One of the input args was NULL.

--*/
{
  METAX_VIDEO_PRIVATE_DATA *Private;
  METAX_VIDEO_MODE_DATA *ModeData;

  Private = METAX_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS(This);

  if ((Info == NULL) || (SizeOfInfo == NULL) || (ModeNumber >= This->Mode->MaxMode))
  {
    return EFI_INVALID_PARAMETER;
  }

  *Info = AllocatePool(sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  if (*Info == NULL)
  {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  ModeData = &Private->ModeData[ModeNumber];
  (*Info)->HorizontalResolution = ModeData->HorizontalResolution;
  (*Info)->VerticalResolution = ModeData->VerticalResolution;
  MetaXVideoCompleteModeInfo(ModeData, *Info);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MetaXVideoGraphicsOutputSetMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber)

/*++

Routine Description:

  Graphics Output protocol interface to set video mode

  Arguments:
    This             - Protocol instance pointer.
    ModeNumber       - The mode number to be set.

  Returns:
    EFI_SUCCESS      - Graphics mode was changed.
    EFI_DEVICE_ERROR - The device had an error and could not complete the request.
    EFI_UNSUPPORTED  - ModeNumber is not supported by this device.

--*/
{
  METAX_VIDEO_PRIVATE_DATA *Private;
  METAX_VIDEO_MODE_DATA *ModeData;
  RETURN_STATUS Status;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black;
  Private = METAX_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS(This);

  if (ModeNumber >= This->Mode->MaxMode)
  {
    return EFI_UNSUPPORTED;
  }

  ModeData = &Private->ModeData[ModeNumber];

  MetaXPciWrite(Private, REG_PIPE_CONTROL, PIPE_CONTROL_BLANK);
  MetaXPciWrite(Private, REG_MODE, ModeNumber);

  This->Mode->Mode = ModeNumber;
  This->Mode->Info->HorizontalResolution = ModeData->HorizontalResolution;
  This->Mode->Info->VerticalResolution = ModeData->VerticalResolution;
  This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  MetaXVideoCompleteModeData(Private, This->Mode);

  //
  // Re-initialize the frame buffer configure when mode changes.
  //
  Status = FrameBufferBltConfigure(
      (VOID *)(UINTN)This->Mode->FrameBufferBase,
      This->Mode->Info,
      Private->FrameBufferBltConfigure,
      &Private->FrameBufferBltConfigureSize);
  if (Status == RETURN_BUFFER_TOO_SMALL)
  {
    //
    // Frame buffer configure may be larger in new mode.
    //
    if (Private->FrameBufferBltConfigure != NULL)
    {
      FreePool(Private->FrameBufferBltConfigure);
    }

    Private->FrameBufferBltConfigure =
        AllocatePool(Private->FrameBufferBltConfigureSize);
    ASSERT(Private->FrameBufferBltConfigure != NULL);

    //
    // Create the configuration for FrameBufferBltLib
    //
    Status = FrameBufferBltConfigure(
        (VOID *)(UINTN)This->Mode->FrameBufferBase,
        This->Mode->Info,
        Private->FrameBufferBltConfigure,
        &Private->FrameBufferBltConfigureSize);
  }

  ASSERT(Status == RETURN_SUCCESS);

  //
  // Per UEFI Spec, need to clear the visible portions of the output display to black.
  //
  ZeroMem(&Black, sizeof(Black));
  Status = FrameBufferBlt(
      Private->FrameBufferBltConfigure,
      &Black,
      EfiBltVideoFill,
      0,
      0,
      0,
      0,
      This->Mode->Info->HorizontalResolution,
      This->Mode->Info->VerticalResolution,
      0);
  ASSERT_RETURN_ERROR(Status);
  MetaXPciWrite(Private, REG_PIPE_CONTROL, PIPE_CONTROL_ENABLE);
  MetaXVideoFlush(
      Private,
      0,
      0,
      This->Mode->Info->HorizontalResolution,
      This->Mode->Info->VerticalResolution);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MetaXVideoGraphicsOutputBlt(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer OPTIONAL,
    IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    IN UINTN SourceX,
    IN UINTN SourceY,
    IN UINTN DestinationX,
    IN UINTN DestinationY,
    IN UINTN Width,
    IN UINTN Height,
    IN UINTN Delta)

/*++

Routine Description:

  Graphics Output protocol instance to block transfer for CirrusLogic device

Arguments:

  This          - Pointer to Graphics Output protocol instance
  BltBuffer     - The data to transfer to screen
  BltOperation  - The operation to perform
  SourceX       - The X coordinate of the source for BltOperation
  SourceY       - The Y coordinate of the source for BltOperation
  DestinationX  - The X coordinate of the destination for BltOperation
  DestinationY  - The Y coordinate of the destination for BltOperation
  Width         - The width of a rectangle in the blt rectangle in pixels
  Height        - The height of a rectangle in the blt rectangle in pixels
  Delta         - Not used for EfiBltVideoFill and EfiBltVideoToVideo operation.
                  If a Delta of 0 is used, the entire BltBuffer will be operated on.
                  If a subrectangle of the BltBuffer is used, then Delta represents
                  the number of bytes in a row of the BltBuffer.

Returns:

  EFI_INVALID_PARAMETER - Invalid parameter passed in
  EFI_SUCCESS - Blt operation success

--*/
{
  EFI_STATUS Status;
  EFI_TPL OriginalTPL;
  METAX_VIDEO_PRIVATE_DATA *Private;
  UINTN FlushX;
  UINTN FlushY;
  UINTN FlushWidth;
  UINTN FlushHeight;
  BOOLEAN FlushNeeded;

  Private = METAX_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS(This);
  FlushX = DestinationX;
  FlushY = DestinationY;
  FlushWidth = Width;
  FlushHeight = Height;
  FlushNeeded = (BltOperation != EfiBltVideoToBltBuffer);

  //
  // We have to raise to TPL Notify, so we make an atomic write the frame buffer.
  // We would not want a timer based event (Cursor, ...) to come in while we are
  // doing this operation.
  //
  OriginalTPL = gBS->RaiseTPL(TPL_NOTIFY);

  switch (BltOperation)
  {
  case EfiBltVideoToBltBuffer:
  case EfiBltBufferToVideo:
  case EfiBltVideoFill:
  case EfiBltVideoToVideo:
    Status = FrameBufferBlt(
        Private->FrameBufferBltConfigure,
        BltBuffer,
        BltOperation,
        SourceX,
        SourceY,
        DestinationX,
        DestinationY,
        Width,
        Height,
        Delta);
    break;

  default:
    Status = EFI_INVALID_PARAMETER;
    break;
  }

  gBS->RestoreTPL(OriginalTPL);

  if (!EFI_ERROR(Status))
  {
    if (FlushNeeded)
    {
      MetaXVideoFlush(Private, FlushX, FlushY, FlushWidth, FlushHeight);
    }
  }

  return Status;
}

EFI_STATUS
MetaXVideoGraphicsOutputConstructor(
    METAX_VIDEO_PRIVATE_DATA *Private)
{
  EFI_STATUS Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;
  UINT32 Mode;

  GraphicsOutput = &Private->GraphicsOutput;
  GraphicsOutput->QueryMode = MetaXVideoGraphicsOutputQueryMode;
  GraphicsOutput->SetMode = MetaXVideoGraphicsOutputSetMode;
  GraphicsOutput->Blt = MetaXVideoGraphicsOutputBlt;

  //
  // Initialize the private data
  //
  Status = gBS->AllocatePool(
      EfiBootServicesData,
      sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE),
      (VOID **)&Private->GraphicsOutput.Mode);
  if (EFI_ERROR(Status))
  {
    return Status;
  }

  Status = gBS->AllocatePool(
      EfiBootServicesData,
      sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
      (VOID **)&Private->GraphicsOutput.Mode->Info);
  if (EFI_ERROR(Status))
  {
    goto FreeMode;
  }

  Private->GraphicsOutput.Mode->MaxMode = (UINT32)Private->MaxMode;
  Private->GraphicsOutput.Mode->Mode = GRAPHICS_OUTPUT_INVALIDE_MODE_NUMBER;
  Private->FrameBufferBltConfigure = NULL;
  Private->FrameBufferBltConfigureSize = 0;

  //
  // Initialize the hardware
  //
  DEBUG((DEBUG_INFO, "Call MetaXVideoReadDpEdid\n"));
  Status = MetaXVideoReadDpEdid(Private);
  if (EFI_ERROR(Status))
  {
    DEBUG((DEBUG_WARN, "MetaXVideo DP: EDID read failed: %r\n", Status));
  }

  DEBUG((DEBUG_INFO, "Call MetaXgpuChooseDefaultModeFromEdid\n"));
  Mode = MetaXgpuChooseDefaultModeFromEdid(
      Private->Edid,
      sizeof(Private->Edid),
      Private->ModeData,
      Private->MaxMode);
  Status = GraphicsOutput->SetMode(GraphicsOutput, Mode);
  if (EFI_ERROR(Status))
  {
    goto FreeInfo;
  }

  return EFI_SUCCESS;

FreeInfo:
  FreePool(Private->GraphicsOutput.Mode->Info);

FreeMode:
  FreePool(Private->GraphicsOutput.Mode);
  Private->GraphicsOutput.Mode = NULL;

  return Status;
}

EFI_STATUS
MetaXVideoGraphicsOutputDestructor(
    METAX_VIDEO_PRIVATE_DATA *Private)

/*++

Routine Description:

Arguments:

Returns:

  None

--*/
{
  if (Private->FrameBufferBltConfigure != NULL)
  {
    FreePool(Private->FrameBufferBltConfigure);
  }

  if (Private->GraphicsOutput.Mode != NULL)
  {
    if (Private->GraphicsOutput.Mode->Info != NULL)
    {
      gBS->FreePool(Private->GraphicsOutput.Mode->Info);
    }

    gBS->FreePool(Private->GraphicsOutput.Mode);
  }

  return EFI_SUCCESS;
}
