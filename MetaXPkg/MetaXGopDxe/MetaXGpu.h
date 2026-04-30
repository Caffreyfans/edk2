/** @file
  Metax Video Controller Driver

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __METAX_GPU__
#define __METAX_GPU__

#include <PiDxe.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/DevicePath.h>

#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/TimerLib.h>
#include <Library/FrameBufferBltLib.h>

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

#define METAXGPU_BPP 4

/**
 * MMIO register offsets
 */
#define REG_WIDTH (0x00)
#define REG_HEIGHT (0x04)
#define REG_FB_SIZE (0x08)
#define REG_FLUSH (0x0C)

#define REG_MODE (0x10)
#define REG_MAX_MODE (0x14)
#define REG_STRIDE (0x18)

#define PIXEL_RED_SHIFT 0
#define PIXEL_GREEN_SHIFT 3
#define PIXEL_BLUE_SHIFT 6

#define PIXEL_RED_MASK (BIT7 | BIT6 | BIT5)
#define PIXEL_GREEN_MASK (BIT4 | BIT3 | BIT2)
#define PIXEL_BLUE_MASK (BIT1 | BIT0)

#define PIXEL_TO_COLOR_BYTE(pixel, mask, shift) ((UINT8)((pixel & mask) << shift))
#define PIXEL_TO_RED_BYTE(pixel) PIXEL_TO_COLOR_BYTE(pixel, PIXEL_RED_MASK, PIXEL_RED_SHIFT)
#define PIXEL_TO_GREEN_BYTE(pixel) PIXEL_TO_COLOR_BYTE(pixel, PIXEL_GREEN_MASK, PIXEL_GREEN_SHIFT)
#define PIXEL_TO_BLUE_BYTE(pixel) PIXEL_TO_COLOR_BYTE(pixel, PIXEL_BLUE_MASK, PIXEL_BLUE_SHIFT)

#define RGB_BYTES_TO_PIXEL(Red, Green, Blue)                    \
  (UINT8)((((Red) >> PIXEL_RED_SHIFT) & PIXEL_RED_MASK) |       \
          (((Green) >> PIXEL_GREEN_SHIFT) & PIXEL_GREEN_MASK) | \
          (((Blue) >> PIXEL_BLUE_SHIFT) & PIXEL_BLUE_MASK))

#define PIXEL24_RED_MASK 0x00ff0000
#define PIXEL24_GREEN_MASK 0x0000ff00
#define PIXEL24_BLUE_MASK 0x000000ff

#define GRAPHICS_OUTPUT_INVALIDE_MODE_NUMBER 0xffff

typedef enum
{
  METAX_VIDEO_G100 = 1,
  METAX_VIDEO_N400,
} METAX_VIDEO_VARIANT;

typedef struct
{
  UINT8 SubClass;
  UINT16 VendorId;
  UINT16 DeviceId;
  METAX_VIDEO_VARIANT Variant;
  CHAR16 *Name;
} METAX_VIDEO_CARD;

typedef struct
{
  UINT32 InternalModeIndex; // points into card-specific mode table
  UINT32 HorizontalResolution;
  UINT32 VerticalResolution;
  UINT32 ColorDepth;
} METAX_VIDEO_MODE_DATA;

typedef struct
{
  UINT16 VendorId;
  UINT16 DeviceId;
  UINT16 Command;
  UINT16 Status;
  UINT8 RevisionId;
  UINT8 ProgIf;
  UINT8 SubClass;
  UINT8 BaseClass;
} PCI_TYPE0_HEAD_MIN;

typedef struct
{
  EFI_HANDLE Handle;
  EFI_PCI_IO_PROTOCOL *PciIo;
  UINT64 OriginalPciAttributes;
  EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOutput;
  EFI_DEVICE_PATH_PROTOCOL *GopDevicePath;

  UINTN MaxMode;
  METAX_VIDEO_MODE_DATA *ModeData;

  METAX_VIDEO_VARIANT Variant;
  FRAME_BUFFER_CONFIGURE *FrameBufferBltConfigure;
  UINTN FrameBufferBltConfigureSize;
  UINT8 FrameBufferVramBarIndex;

  UINT8 Edid[128];
} METAX_VIDEO_PRIVATE_DATA;

///
/// Card-specific Video Mode structures
///
typedef struct
{
  UINT32 Width;
  UINT32 Height;
  UINT32 ColorDepth;
} METAX_VIDEO_MODES;

#define METAX_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS(a) \
  BASE_CR(a, METAX_VIDEO_PRIVATE_DATA, GraphicsOutput)

extern EFI_COMPONENT_NAME_PROTOCOL gMetaXVideoComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL gMetaXVideoComponentName2;

EFI_STATUS
EFIAPI
MetaXVideoGraphicsOutputQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo,
    OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);

EFI_STATUS
EFIAPI
MetaXVideoGraphicsOutputSetMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN UINT32 ModeNumber);

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
    IN UINTN Delta);

//
// EFI Component Name Functions
//

/**
  Retrieves a Unicode string that is the user readable name of the driver.

  This function retrieves the user readable name of a driver in the form of a
  Unicode string. If the driver specified by This has a user readable name in
  the language specified by Language, then a pointer to the driver name is
  returned in DriverName, and EFI_SUCCESS is returned. If the driver specified
  by This does not support the language specified by Language,
  then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language. This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified
                                in RFC 4646 or ISO 639-2 language code format.

  @param  DriverName[out]       A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                driver specified by This in the language
                                specified by Language.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by
                                This and the language specified by Language was
                                returned in DriverName.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER DriverName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
MetaXVideoComponentNameGetDriverName(
    IN EFI_COMPONENT_NAME_PROTOCOL *This,
    IN CHAR8 *Language,
    OUT CHAR16 **DriverName);

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver.

  This function retrieves the user readable name of the controller specified by
  ControllerHandle and ChildHandle in the form of a Unicode string. If the
  driver specified by This has a user readable name in the language specified by
  Language, then a pointer to the controller name is returned in ControllerName,
  and EFI_SUCCESS is returned.  If the driver specified by This is not currently
  managing the controller specified by ControllerHandle and ChildHandle,
  then EFI_UNSUPPORTED is returned.  If the driver specified by This does not
  support the language specified by Language, then EFI_UNSUPPORTED is returned.

  @param  This[in]              A pointer to the EFI_COMPONENT_NAME2_PROTOCOL or
                                EFI_COMPONENT_NAME_PROTOCOL instance.

  @param  ControllerHandle[in]  The handle of a controller that the driver
                                specified by This is managing.  This handle
                                specifies the controller whose name is to be
                                returned.

  @param  ChildHandle[in]       The handle of the child controller to retrieve
                                the name of.  This is an optional parameter that
                                may be NULL.  It will be NULL for device
                                drivers.  It will also be NULL for a bus drivers
                                that wish to retrieve the name of the bus
                                controller.  It will not be NULL for a bus
                                driver that wishes to retrieve the name of a
                                child controller.

  @param  Language[in]          A pointer to a Null-terminated ASCII string
                                array indicating the language.  This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified in
                                RFC 4646 or ISO 639-2 language code format.

  @param  ControllerName[out]   A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                controller specified by ControllerHandle and
                                ChildHandle in the language specified by
                                Language from the point of view of the driver
                                specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user readable name in
                                the language specified by Language for the
                                driver specified by This was returned in
                                DriverName.

  @retval EFI_INVALID_PARAMETER ControllerHandle is not a valid EFI_HANDLE.

  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid
                                EFI_HANDLE.

  @retval EFI_INVALID_PARAMETER Language is NULL.

  @retval EFI_INVALID_PARAMETER ControllerName is NULL.

  @retval EFI_UNSUPPORTED       The driver specified by This is not currently
                                managing the controller specified by
                                ControllerHandle and ChildHandle.

  @retval EFI_UNSUPPORTED       The driver specified by This does not support
                                the language specified by Language.

**/
EFI_STATUS
EFIAPI
MetaXVideoComponentNameGetControllerName(
    IN EFI_COMPONENT_NAME_PROTOCOL *This,
    IN EFI_HANDLE ControllerHandle,
    IN EFI_HANDLE ChildHandle OPTIONAL,
    IN CHAR8 *Language,
    OUT CHAR16 **ControllerName);

EFI_STATUS
MetaXVideoGraphicsOutputConstructor(
    METAX_VIDEO_PRIVATE_DATA *Private);

EFI_STATUS
MetaXVideoGraphicsOutputDestructor(
    METAX_VIDEO_PRIVATE_DATA *Private);

/**
  Construct the valid video modes for MetaXVideo.

**/
EFI_STATUS
MetaXVideoModeSetup(
    METAX_VIDEO_PRIVATE_DATA *Private);

VOID MetaXPciWrite(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT32 Reg,
    UINT32 Data);

#endif // __METAX_GPU__