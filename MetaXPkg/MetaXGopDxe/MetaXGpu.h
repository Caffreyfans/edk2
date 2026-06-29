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

#define REG_DP_HPD (0x20)
#define REG_DP_LINK_RATE (0x24)
#define REG_DP_LANE_COUNT (0x28)
#define REG_DP_TRAINING_STATUS (0x2C)
#define REG_DP_AUX_ADDR (0x30)
#define REG_DP_AUX_DATA (0x34)
#define REG_DP_AUX_STATUS (0x38)
#define REG_DP_EDID_SIZE (0x3C)

#define REG_PIPE_CONTROL (0x40)
#define REG_PIXEL_CLOCK_KHZ (0x44)
#define REG_H_TOTAL (0x48)
#define REG_H_SYNC (0x4C)
#define REG_V_TOTAL (0x50)
#define REG_V_SYNC (0x54)

#define REG_FLUSH_X (0x60)
#define REG_FLUSH_Y (0x64)
#define REG_FLUSH_WIDTH (0x68)
#define REG_FLUSH_HEIGHT (0x6C)

#define REG_DP_MAIN_LINK_STATUS (0x70)
#define REG_DP_MAIN_LINK_BYTES_LO (0x74)
#define REG_DP_MAIN_LINK_BYTES_HI (0x78)
#define REG_DP_MSA_REQUIRED_KBPS (0x7C)
#define REG_DP_MSA_AVAILABLE_KBPS (0x80)
#define REG_DP_TRAINING_STATE (0x84)
#define REG_DP_FRAME_COUNT (0x88)
#define REG_DP_FRAME_CRC (0x8C)
#define REG_DP_TEST_PATTERN (0x90)

#define PIPE_CONTROL_ENABLE BIT0
#define PIPE_CONTROL_BLANK BIT1

#define DP_HPD_CONNECTED BIT0
#define DP_TRAINING_DONE BIT0
#define DP_AUX_STATUS_ACK BIT0
#define DP_MAIN_LINK_ACTIVE BIT0
#define DP_MAIN_LINK_TRAINED BIT1
#define DP_MAIN_LINK_BANDWIDTH_OK BIT2
#define DP_AUX_EDID_BASE 0x5000

#define DP_TRAINING_STATE_IDLE 0
#define DP_TRAINING_STATE_CLOCK_RECOVERY 1
#define DP_TRAINING_STATE_CHANNEL_EQUALIZATION 2
#define DP_TRAINING_STATE_TRAINED 3
#define DP_TRAINING_STATE_FAILED 4

#define DP_TEST_PATTERN_FRAMEBUFFER 0
#define DP_TEST_PATTERN_COLOR_BARS 1
#define DP_TEST_PATTERN_CHECKERBOARD 2
#define DP_TEST_PATTERN_GRADIENT 3

#define DP_DPCD_REVISION 0x00
#define DP_DPCD_MAX_LINK_RATE 0x01
#define DP_DPCD_MAX_LANE_COUNT 0x02
#define DP_DPCD_REV_1_0 0x10
#define DP_DPCD_LINK_BW_SET 0x100
#define DP_DPCD_LANE_COUNT_SET 0x101
#define DP_DPCD_TRAINING_PATTERN_SET 0x102
#define DP_DPCD_TRAINING_LANE0_SET 0x103
#define DP_DPCD_TRAINING_LANE1_SET 0x104
#define DP_DPCD_TRAINING_LANE2_SET 0x105
#define DP_DPCD_TRAINING_LANE3_SET 0x106
#define DP_DPCD_LANE0_1_STATUS 0x202
#define DP_DPCD_LANE2_3_STATUS 0x203
#define DP_DPCD_LANE_ALIGN_STATUS_UPDATED 0x204

#define DP_DPCD_LANE_COUNT_MASK 0x1F
#define DP_DPCD_MAX_LANE_COUNT_SUPPORTED 4
#define DP_DPCD_LINK_RATE_1_62GBPS 0x06
#define DP_DPCD_LINK_RATE_2_7GBPS 0x0A
#define DP_DPCD_LINK_RATE_5_4GBPS 0x14

#define DP_TRAINING_PATTERN_DISABLE 0x00
#define DP_TRAINING_PATTERN_1 0x01
#define DP_TRAINING_PATTERN_2 0x02
#define DP_TRAINING_LANE_SET_DEFAULT 0x00

#define DP_LANE0_CR_DONE BIT0
#define DP_LANE0_CHANNEL_EQ_DONE BIT1
#define DP_LANE0_SYMBOL_LOCKED BIT2
#define DP_LANE1_CR_DONE BIT4
#define DP_LANE1_CHANNEL_EQ_DONE BIT5
#define DP_LANE1_SYMBOL_LOCKED BIT6
#define DP_LANE2_CR_DONE BIT0
#define DP_LANE2_CHANNEL_EQ_DONE BIT1
#define DP_LANE2_SYMBOL_LOCKED BIT2
#define DP_LANE3_CR_DONE BIT4
#define DP_LANE3_CHANNEL_EQ_DONE BIT5
#define DP_LANE3_SYMBOL_LOCKED BIT6
#define DP_INTERLANE_ALIGN_DONE BIT0

#define DP_LINK_TRAINING_MAX_ATTEMPTS 5
#define DP_LINK_TRAINING_POLL_DELAY_US 1000

#define MXGPU_EDID_SIZE 256

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

  UINT8 Edid[MXGPU_EDID_SIZE];
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

UINT32
MetaXgpuChooseDefaultModeFromEdid(
    IN CONST UINT8 *Edid,
    IN UINT32 EdidSize,
    IN CONST METAX_VIDEO_MODE_DATA *ModeTable,
    IN UINT32 ModeCount);
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

UINT32
MetaXPciRead(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT32 Reg);

EFI_STATUS
MetaXVideoReadDpEdid(
    METAX_VIDEO_PRIVATE_DATA *Private);

#endif // __METAX_GPU__
