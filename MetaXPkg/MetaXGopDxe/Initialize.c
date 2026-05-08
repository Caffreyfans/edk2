/** @file
  Graphics Output Protocol functions for the MetaX video controller.

  Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MetaXGpu.h"

///
/// Table of supported video modes
///
METAX_VIDEO_MODES MetaXVideoModes[] = {
    {800, 600, 32},
    {1024, 768, 32},
    {1280, 800, 32},
    {1920, 1080, 32},
    {2560, 1440, 32},
    {3840, 2160, 32},
};

#define METAX_VIDEO_MODE_COUNT \
  (ARRAY_SIZE(MetaXVideoModes))

/**
  Construct the valid video modes for MetaXVideo.

**/
EFI_STATUS
MetaXVideoModeSetup(
    METAX_VIDEO_PRIVATE_DATA *Private)
{
  UINT32 Index;
  METAX_VIDEO_MODE_DATA *ModeData;
  METAX_VIDEO_MODES *VideoMode;

  //
  // Setup Video Modes
  //
  Private->ModeData = AllocatePool(
      sizeof(Private->ModeData[0]) * METAX_VIDEO_MODE_COUNT);
  if (Private->ModeData == NULL)
  {
    return EFI_OUT_OF_RESOURCES;
  }

  ModeData = Private->ModeData;
  VideoMode = &MetaXVideoModes[0];
  for (Index = 0; Index < METAX_VIDEO_MODE_COUNT; Index++)
  {
    ModeData->InternalModeIndex = Index;
    ModeData->HorizontalResolution = VideoMode->Width;
    ModeData->VerticalResolution = VideoMode->Height;
    ModeData->ColorDepth = VideoMode->ColorDepth;
    Print(L"Adding Mode %d as  Internal Mode %d: %dx%d, %d-bit\n",
          (INT32)(ModeData - Private->ModeData),
          ModeData->InternalModeIndex,
          ModeData->HorizontalResolution,
          ModeData->VerticalResolution,
          ModeData->ColorDepth);

    ModeData++;
    VideoMode++;
  }

  Private->MaxMode = ModeData - Private->ModeData;

  return EFI_SUCCESS;
}