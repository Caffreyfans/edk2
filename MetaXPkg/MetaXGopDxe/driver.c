/** @file
  Metax Video Controller Driver

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>

#include <Protocol/PciIo.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/GraphicsOutput.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>

#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Pci.h>

#include "MetaXGpu.h"

#define METAX_VENDOR_ID 0x9999
#define N400_VENDOR_ID 0x0001

#define METAX_VIDEO_PRIVATE_DATA_FROM_GOP(a) \
    BASE_CR(a, METAX_VIDEO_PRIVATE_DATA, GraphicsOutput)

EFI_STATUS
EFIAPI
MetaXGpuDriverBindingSupoorted(
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE ControllerHandle,
    IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL);

EFI_STATUS
EFIAPI
MetaXGpuDriverBindingStart(
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE ControllerHandle,
    IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL);

EFI_STATUS
EFIAPI
MetaXGpuDriverBindingStop(
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE ControllerHandle,
    IN UINTN NumberOfChildren,
    IN EFI_HANDLE *ChildHandleBuffer OPTIONAL);

EFI_DRIVER_BINDING_PROTOCOL gMetaXGpuDriverBinding = {
    MetaXGpuDriverBindingSupoorted,
    MetaXGpuDriverBindingStart,
    MetaXGpuDriverBindingStop,
    0x10,
    NULL,
    NULL};

METAX_VIDEO_CARD gMetaXVideoCardList[] = {
    {PCI_CLASS_DISPLAY_VGA,
     METAX_VENDOR_ID,
     N400_VENDOR_ID,
     METAX_VIDEO_N400,
     L"N400"},
    {0, 0, 0, 0, NULL}};

static METAX_VIDEO_CARD *
MetaxVideoDetect(IN UINT8 SubClass, IN UINT16 VendorId, IN UINT16 DeviceId)
{
    UINTN Index = 0;

    while (gMetaXVideoCardList[Index].VendorId != 0)
    {
        if ((gMetaXVideoCardList[Index].SubClass == SubClass) &&
            (gMetaXVideoCardList[Index].VendorId == VendorId) &&
            (gMetaXVideoCardList[Index].DeviceId == DeviceId))
        {
            return gMetaXVideoCardList + Index;
        }

        Index++;
    }

    return NULL;
}

VOID MetaXPciWrite(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT32 Reg,
    UINT32 Data)
{
    Private->PciIo->Mem.Write(Private->PciIo,
                              EfiPciIoWidthUint32,
                              PCI_BAR_IDX0,
                              Reg,
                              1,
                              &Data);
}

UINT32
MetaXPciRead(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT32 Reg)
{
    UINT32 Data;

    Data = 0;
    Private->PciIo->Mem.Read(Private->PciIo,
                             EfiPciIoWidthUint32,
                             PCI_BAR_IDX0,
                             Reg,
                             1,
                             &Data);

    return Data;
}

STATIC
CONST CHAR8 *
MetaXVideoDpTrainingStateName(
    UINT32 State)
{
    switch (State)
    {
    case DP_TRAINING_STATE_IDLE:
        return "idle";
    case DP_TRAINING_STATE_CLOCK_RECOVERY:
        return "clock-recovery";
    case DP_TRAINING_STATE_CHANNEL_EQUALIZATION:
        return "channel-equalization";
    case DP_TRAINING_STATE_TRAINED:
        return "trained";
    case DP_TRAINING_STATE_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

STATIC
VOID
MetaXVideoDpDumpLinkSummary(
    METAX_VIDEO_PRIVATE_DATA *Private)
{
    UINT32 MainLinkStatus;
    UINT32 RequiredKbps;
    UINT32 AvailableKbps;
    UINT32 TrainingState;

    MainLinkStatus = MetaXPciRead(Private, REG_DP_MAIN_LINK_STATUS);
    RequiredKbps = MetaXPciRead(Private, REG_DP_MSA_REQUIRED_KBPS);
    AvailableKbps = MetaXPciRead(Private, REG_DP_MSA_AVAILABLE_KBPS);
    TrainingState = MetaXPciRead(Private, REG_DP_TRAINING_STATE);

    DEBUG((
        DEBUG_INFO,
        "MetaXVideo DP: main-link status=0x%08x state=%a required=%u kbps available=%u kbps\n",
        MainLinkStatus,
        MetaXVideoDpTrainingStateName(TrainingState),
        RequiredKbps,
        AvailableKbps));
}

STATIC
EFI_STATUS
MetaXVideoDpAuxReadByte(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT32 AuxAddress,
    UINT8 *Data)
{
    UINT32 Value;
    UINT32 Status;

    if ((Private == NULL) || (Data == NULL))
    {
        return EFI_INVALID_PARAMETER;
    }

    MetaXPciWrite(Private, REG_DP_AUX_ADDR, AuxAddress);
    Value = MetaXPciRead(Private, REG_DP_AUX_DATA);
    Status = MetaXPciRead(Private, REG_DP_AUX_STATUS);

    if ((Status & DP_AUX_STATUS_ACK) == 0)
    {
        DEBUG((
            DEBUG_ERROR,
            "MetaXVideo DP: AUX read failed, Addr=0x%08x Status=0x%08x\n",
            AuxAddress,
            Status));
        return EFI_DEVICE_ERROR;
    }

    *Data = (UINT8)Value;
    return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MetaXVideoDpAuxWriteByte(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT32 AuxAddress,
    UINT8 Data)
{
    UINT32 Status;

    if (Private == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    MetaXPciWrite(Private, REG_DP_AUX_ADDR, AuxAddress);
    MetaXPciWrite(Private, REG_DP_AUX_DATA, Data);
    Status = MetaXPciRead(Private, REG_DP_AUX_STATUS);

    if ((Status & DP_AUX_STATUS_ACK) == 0)
    {
        DEBUG((
            DEBUG_ERROR,
            "MetaXVideo DP: AUX write failed, Addr=0x%08x Data=0x%02x Status=0x%08x\n",
            AuxAddress,
            Data,
            Status));
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

STATIC
UINT8
MetaXVideoDpSelectLinkRate(
    UINT8 DpcdMaxLinkRate)
{
    if (DpcdMaxLinkRate >= DP_DPCD_LINK_RATE_5_4GBPS)
    {
        return DP_DPCD_LINK_RATE_5_4GBPS;
    }

    if (DpcdMaxLinkRate >= DP_DPCD_LINK_RATE_2_7GBPS)
    {
        return DP_DPCD_LINK_RATE_2_7GBPS;
    }

    return DP_DPCD_LINK_RATE_1_62GBPS;
}

STATIC
UINT8
MetaXVideoDpSelectLaneCount(
    UINT8 DpcdMaxLaneCount)
{
    UINT8 LaneCount;

    LaneCount = DpcdMaxLaneCount & DP_DPCD_LANE_COUNT_MASK;
    if (LaneCount >= DP_DPCD_MAX_LANE_COUNT_SUPPORTED)
    {
        return DP_DPCD_MAX_LANE_COUNT_SUPPORTED;
    }

    if (LaneCount >= 2)
    {
        return 2;
    }

    return 1;
}

STATIC
BOOLEAN
MetaXVideoDpClockRecoveryDone(
    UINT8 Lane01Status,
    UINT8 Lane23Status,
    UINT8 LaneCount)
{
    if ((Lane01Status & DP_LANE0_CR_DONE) == 0)
    {
        return FALSE;
    }

    if ((LaneCount >= 2) && ((Lane01Status & DP_LANE1_CR_DONE) == 0))
    {
        return FALSE;
    }

    if ((LaneCount >= 3) && ((Lane23Status & DP_LANE2_CR_DONE) == 0))
    {
        return FALSE;
    }

    if ((LaneCount >= 4) && ((Lane23Status & DP_LANE3_CR_DONE) == 0))
    {
        return FALSE;
    }

    return TRUE;
}

STATIC
BOOLEAN
MetaXVideoDpChannelEqualizationDone(
    UINT8 Lane01Status,
    UINT8 Lane23Status,
    UINT8 AlignStatus,
    UINT8 LaneCount)
{
    if ((AlignStatus & DP_INTERLANE_ALIGN_DONE) == 0)
    {
        return FALSE;
    }

    if ((Lane01Status & (DP_LANE0_CHANNEL_EQ_DONE | DP_LANE0_SYMBOL_LOCKED)) !=
        (DP_LANE0_CHANNEL_EQ_DONE | DP_LANE0_SYMBOL_LOCKED))
    {
        return FALSE;
    }

    if ((LaneCount >= 2) &&
        ((Lane01Status & (DP_LANE1_CHANNEL_EQ_DONE | DP_LANE1_SYMBOL_LOCKED)) !=
         (DP_LANE1_CHANNEL_EQ_DONE | DP_LANE1_SYMBOL_LOCKED)))
    {
        return FALSE;
    }

    if ((LaneCount >= 3) &&
        ((Lane23Status & (DP_LANE2_CHANNEL_EQ_DONE | DP_LANE2_SYMBOL_LOCKED)) !=
         (DP_LANE2_CHANNEL_EQ_DONE | DP_LANE2_SYMBOL_LOCKED)))
    {
        return FALSE;
    }

    if ((LaneCount >= 4) &&
        ((Lane23Status & (DP_LANE3_CHANNEL_EQ_DONE | DP_LANE3_SYMBOL_LOCKED)) !=
         (DP_LANE3_CHANNEL_EQ_DONE | DP_LANE3_SYMBOL_LOCKED)))
    {
        return FALSE;
    }

    return TRUE;
}

STATIC
EFI_STATUS
MetaXVideoDpReadLinkStatus(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT8 *Lane01Status,
    UINT8 *Lane23Status,
    UINT8 *AlignStatus)
{
    EFI_STATUS Status;

    Status = MetaXVideoDpAuxReadByte(
        Private,
        DP_DPCD_LANE0_1_STATUS,
        Lane01Status);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = MetaXVideoDpAuxReadByte(
        Private,
        DP_DPCD_LANE2_3_STATUS,
        Lane23Status);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    return MetaXVideoDpAuxReadByte(
        Private,
        DP_DPCD_LANE_ALIGN_STATUS_UPDATED,
        AlignStatus);
}

STATIC
EFI_STATUS
MetaXVideoDpSetTrainingPattern(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT8 Pattern)
{
    return MetaXVideoDpAuxWriteByte(
        Private,
        DP_DPCD_TRAINING_PATTERN_SET,
        Pattern);
}

STATIC
EFI_STATUS
MetaXVideoDpConfigureLaneDrive(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT8 LaneCount)
{
    EFI_STATUS Status;
    UINT8 Index;

    for (Index = 0; Index < LaneCount; Index++)
    {
        Status = MetaXVideoDpAuxWriteByte(
            Private,
            DP_DPCD_TRAINING_LANE0_SET + Index,
            DP_TRAINING_LANE_SET_DEFAULT);
        if (EFI_ERROR(Status))
        {
            return Status;
        }
    }

    return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MetaXVideoDpLinkTrain(
    METAX_VIDEO_PRIVATE_DATA *Private,
    UINT8 DpcdMaxLinkRate,
    UINT8 DpcdMaxLaneCount)
{
    EFI_STATUS Status;
    UINT8 LinkRate;
    UINT8 LaneCount;
    UINT8 Attempt;
    UINT8 Lane01Status;
    UINT8 Lane23Status;
    UINT8 AlignStatus;

    LinkRate = MetaXVideoDpSelectLinkRate(DpcdMaxLinkRate);
    LaneCount = MetaXVideoDpSelectLaneCount(DpcdMaxLaneCount);
    Lane01Status = 0;
    Lane23Status = 0;
    AlignStatus = 0;

    DEBUG((
        DEBUG_INFO,
        "MetaXVideo DP: link training start, link=0x%02x lanes=%u\n",
        LinkRate,
        LaneCount));

    MetaXPciWrite(Private, REG_DP_TRAINING_STATUS, 0);
    MetaXPciWrite(Private, REG_DP_LINK_RATE, LinkRate);
    MetaXPciWrite(Private, REG_DP_LANE_COUNT, LaneCount);

    Status = MetaXVideoDpAuxWriteByte(Private, DP_DPCD_LINK_BW_SET, LinkRate);
    if (EFI_ERROR(Status))
    {
        goto Done;
    }

    Status = MetaXVideoDpAuxWriteByte(Private, DP_DPCD_LANE_COUNT_SET, LaneCount);
    if (EFI_ERROR(Status))
    {
        goto Done;
    }

    Status = MetaXVideoDpConfigureLaneDrive(Private, LaneCount);
    if (EFI_ERROR(Status))
    {
        goto Done;
    }

    Status = MetaXVideoDpSetTrainingPattern(Private, DP_TRAINING_PATTERN_1);
    if (EFI_ERROR(Status))
    {
        goto Done;
    }

    for (Attempt = 0; Attempt < DP_LINK_TRAINING_MAX_ATTEMPTS; Attempt++)
    {
        MicroSecondDelay(DP_LINK_TRAINING_POLL_DELAY_US);
        Status = MetaXVideoDpReadLinkStatus(
            Private,
            &Lane01Status,
            &Lane23Status,
            &AlignStatus);
        if (EFI_ERROR(Status))
        {
            goto Done;
        }

        if (MetaXVideoDpClockRecoveryDone(Lane01Status, Lane23Status, LaneCount))
        {
            break;
        }
    }

    if (Attempt == DP_LINK_TRAINING_MAX_ATTEMPTS)
    {
        DEBUG((
            DEBUG_ERROR,
            "MetaXVideo DP: clock recovery failed, lane01=0x%02x lane23=0x%02x\n",
            Lane01Status,
            Lane23Status));
        Status = EFI_TIMEOUT;
        goto Done;
    }

    Status = MetaXVideoDpSetTrainingPattern(Private, DP_TRAINING_PATTERN_2);
    if (EFI_ERROR(Status))
    {
        goto Done;
    }

    for (Attempt = 0; Attempt < DP_LINK_TRAINING_MAX_ATTEMPTS; Attempt++)
    {
        MicroSecondDelay(DP_LINK_TRAINING_POLL_DELAY_US);
        Status = MetaXVideoDpReadLinkStatus(
            Private,
            &Lane01Status,
            &Lane23Status,
            &AlignStatus);
        if (EFI_ERROR(Status))
        {
            goto Done;
        }

        if (MetaXVideoDpChannelEqualizationDone(
                Lane01Status,
                Lane23Status,
                AlignStatus,
                LaneCount))
        {
            break;
        }
    }

    if (Attempt == DP_LINK_TRAINING_MAX_ATTEMPTS)
    {
        DEBUG((
            DEBUG_ERROR,
            "MetaXVideo DP: channel equalization failed, lane01=0x%02x lane23=0x%02x align=0x%02x\n",
            Lane01Status,
            Lane23Status,
            AlignStatus));
        Status = EFI_TIMEOUT;
        goto Done;
    }

    MetaXPciWrite(Private, REG_DP_TRAINING_STATUS, DP_TRAINING_DONE);
    DEBUG((DEBUG_INFO, "MetaXVideo DP: link training done\n"));
    MetaXVideoDpDumpLinkSummary(Private);
    Status = EFI_SUCCESS;

Done:
    MetaXVideoDpSetTrainingPattern(Private, DP_TRAINING_PATTERN_DISABLE);
    if (EFI_ERROR(Status))
    {
        MetaXPciWrite(Private, REG_DP_TRAINING_STATUS, 0);
    }

    return Status;
}

EFI_STATUS
MetaXVideoReadDpEdid(
    METAX_VIDEO_PRIVATE_DATA *Private)
{
    EFI_STATUS Status;
    UINT32 Hpd;
    UINT32 EdidSize;
    UINT32 Index;
    UINT8 DpcdRevision;
    UINT8 DpcdMaxLinkRate;
    UINT8 DpcdMaxLaneCount;

    if (Private == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    ZeroMem(Private->Edid, sizeof(Private->Edid));

    Hpd = MetaXPciRead(Private, REG_DP_HPD);
    if ((Hpd & DP_HPD_CONNECTED) == 0)
    {
        DEBUG((DEBUG_WARN, "MetaXVideo DP: no sink connected, HPD=0x%08x\n", Hpd));
        return EFI_NOT_FOUND;
    }

    Status = MetaXVideoDpAuxReadByte(Private, DP_DPCD_REVISION, &DpcdRevision);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = MetaXVideoDpAuxReadByte(Private, DP_DPCD_MAX_LINK_RATE, &DpcdMaxLinkRate);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = MetaXVideoDpAuxReadByte(Private, DP_DPCD_MAX_LANE_COUNT, &DpcdMaxLaneCount);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    DEBUG((
        DEBUG_INFO,
        "MetaXVideo DP: DPCD rev=0x%02x max-link=0x%02x lanes=%u\n",
        DpcdRevision,
        DpcdMaxLinkRate,
        DpcdMaxLaneCount));

    if (DpcdRevision < DP_DPCD_REV_1_0)
    {
        DEBUG((DEBUG_WARN, "MetaXVideo DP: unsupported DPCD revision 0x%02x\n", DpcdRevision));
        return EFI_UNSUPPORTED;
    }

    Status = MetaXVideoDpLinkTrain(Private, DpcdMaxLinkRate, DpcdMaxLaneCount);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    EdidSize = MetaXPciRead(Private, REG_DP_EDID_SIZE);
    if ((EdidSize == 0) || (EdidSize > sizeof(Private->Edid)))
    {
        EdidSize = sizeof(Private->Edid);
    }

    for (Index = 0; Index < EdidSize; Index++)
    {
        Status = MetaXVideoDpAuxReadByte(
            Private,
            DP_AUX_EDID_BASE + Index,
            &Private->Edid[Index]);
        if (EFI_ERROR(Status))
        {
            return Status;
        }
    }

    DEBUG((
        DEBUG_INFO,
        "MetaXVideo DP: EDID %u bytes, header=%02x %02x %02x %02x %02x %02x %02x %02x\n",
        EdidSize,
        Private->Edid[0],
        Private->Edid[1],
        Private->Edid[2],
        Private->Edid[3],
        Private->Edid[4],
        Private->Edid[5],
        Private->Edid[6],
        Private->Edid[7]));

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MetaXGpuDriverBindingSupoorted(
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE ControllerHandle,
    IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL)
{
    EFI_STATUS Status;
    EFI_PCI_IO_PROTOCOL *PciIo;
    PCI_TYPE00 Pci;
    METAX_VIDEO_CARD *Card;

    //
    // Open the PCI I/O Protocol
    //
    Status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiPciIoProtocolGuid,
        (VOID **)&PciIo,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    //
    // Read the PCI Configuration Header from the PCI Device
    //
    Status = PciIo->Pci.Read(
        PciIo,
        EfiPciIoWidthUint32,
        0,
        sizeof(Pci) / sizeof(UINT32),
        &Pci);
    if (EFI_ERROR(Status))
    {
        goto Done;
    }

    Status = EFI_UNSUPPORTED;
    if (!IS_PCI_DISPLAY(&Pci))
    {
        goto Done;
    }

    Card = MetaxVideoDetect(Pci.Hdr.ClassCode[1], Pci.Hdr.VendorId, Pci.Hdr.DeviceId);
    if (Card != NULL)
    {
        DEBUG((DEBUG_INFO, "MetaXVideo: %s detected\n", Card->Name));
        Status = EFI_SUCCESS;
    }

Done:
    //
    // Close the PCI I/O Protocol
    //
    gBS->CloseProtocol(
        ControllerHandle,
        &gEfiPciIoProtocolGuid,
        This->DriverBindingHandle,
        ControllerHandle);

    return Status;
}

EFI_STATUS
EFIAPI
MetaXGpuDriverBindingStart(
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE ControllerHandle,
    IN EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL)
{
    EFI_TPL OldTpl;
    EFI_STATUS Status;
    METAX_VIDEO_PRIVATE_DATA *Private;
    EFI_DEVICE_PATH_PROTOCOL *ParentDevicePath;
    ACPI_ADR_DEVICE_PATH AcpiDeviceNode;
    PCI_TYPE00 Pci;
    METAX_VIDEO_CARD *Card;
    EFI_PCI_IO_PROTOCOL *ChildPciIo;
    UINT64 SupportedVgaIo;

    OldTpl = gBS->RaiseTPL(TPL_CALLBACK);
    //
    // Allocate Private context data for GOP interface.
    //
    Private = AllocateZeroPool(sizeof(METAX_VIDEO_PRIVATE_DATA));
    if (Private == NULL)
    {
        Status = EFI_OUT_OF_RESOURCES;
        goto RestoreTpl;
    }

    //
    // Open PCI I/O Protocol
    //
    Status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiPciIoProtocolGuid,
        (VOID **)&Private->PciIo,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status))
    {
        goto FreePrivate;
    }

    //
    // Read the PCI Configuration Header from the PCI Device
    //
    Status = Private->PciIo->Pci.Read(
        Private->PciIo,
        EfiPciIoWidthUint32,
        0,
        sizeof(Pci) / sizeof(UINT32),
        &Pci);
    if (EFI_ERROR(Status))
    {
        goto ClosePciIo;
    }

    //
    // Determine card variant.
    //
    Card = MetaxVideoDetect(Pci.Hdr.ClassCode[1], Pci.Hdr.VendorId, Pci.Hdr.DeviceId);
    if (Card == NULL)
    {
        Status = EFI_DEVICE_ERROR;
        goto ClosePciIo;
    }

    Private->Variant = Card->Variant;

    //
    // Save original PCI attributes
    //
    Status = Private->PciIo->Attributes(
        Private->PciIo,
        EfiPciIoAttributeOperationGet,
        0,
        &Private->OriginalPciAttributes);

    if (EFI_ERROR(Status))
    {
        goto ClosePciIo;
    }

    //
    // Get supported PCI attributes
    //
    Status = Private->PciIo->Attributes(
        Private->PciIo,
        EfiPciIoAttributeOperationSupported,
        0,
        &SupportedVgaIo);
    if (EFI_ERROR(Status))
    {
        goto ClosePciIo;
    }

    SupportedVgaIo &= (UINT64)(EFI_PCI_IO_ATTRIBUTE_VGA_IO | EFI_PCI_IO_ATTRIBUTE_VGA_IO_16);
    if ((SupportedVgaIo == 0) && IS_PCI_VGA(&Pci))
    {
        Status = EFI_UNSUPPORTED;
        goto ClosePciIo;
    }

    //
    // Set new PCI attributes
    //
    Status = Private->PciIo->Attributes(
        Private->PciIo,
        EfiPciIoAttributeOperationEnable,
        EFI_PCI_DEVICE_ENABLE | EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY | SupportedVgaIo,
        NULL);
    if (EFI_ERROR(Status))
    {
        goto ClosePciIo;
    }

    //
    // Get ParentDevicePath
    //
    Status = gBS->HandleProtocol(
        ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        (VOID **)&ParentDevicePath);
    if (EFI_ERROR(Status))
    {
        goto RestoreAttributes;
    }

    //
    // Set Gop Device Path
    //
    ZeroMem(&AcpiDeviceNode, sizeof(ACPI_ADR_DEVICE_PATH));
    AcpiDeviceNode.Header.Type = ACPI_DEVICE_PATH;
    AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
    AcpiDeviceNode.ADR = ACPI_DISPLAY_ADR(1, 0, 0, 1, 0, ACPI_ADR_DISPLAY_TYPE_VGA, 0, 0);
    SetDevicePathNodeLength(&AcpiDeviceNode.Header, sizeof(ACPI_ADR_DEVICE_PATH));

    Private->GopDevicePath = AppendDevicePathNode(
        ParentDevicePath,
        (EFI_DEVICE_PATH_PROTOCOL *)&AcpiDeviceNode);
    if (Private->GopDevicePath == NULL)
    {
        Status = EFI_OUT_OF_RESOURCES;
        goto RestoreAttributes;
    }

    //
    // Create new child handle and install the device path protocol on it.
    //
    Status = gBS->InstallMultipleProtocolInterfaces(
        &Private->Handle,
        &gEfiDevicePathProtocolGuid,
        Private->GopDevicePath,
        NULL);
    if (EFI_ERROR(Status))
    {
        goto FreeGopDevicePath;
    }

    //
    // Construct video mode buffer
    //
    switch (Private->Variant)
    {
    case METAX_VIDEO_G100:
    case METAX_VIDEO_N400:
        Status = MetaXVideoModeSetup(Private);
        break;
    default:
        ASSERT(FALSE);
        Status = EFI_DEVICE_ERROR;
        break;
    }

    if (EFI_ERROR(Status))
    {
        goto UninstallGopDevicePath;
    }

    //
    // Start the GOP software stack.
    //
    Status = MetaXVideoGraphicsOutputConstructor(Private);
    if (EFI_ERROR(Status))
    {
        goto FreeModeData;
    }

    Status = gBS->InstallMultipleProtocolInterfaces(
        &Private->Handle,
        &gEfiGraphicsOutputProtocolGuid,
        &Private->GraphicsOutput,
        NULL);
    if (EFI_ERROR(Status))
    {
        goto DestructMetaXVideoGraphics;
    }

    //
    // Reference parent handle from child handle.
    //
    Status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiPciIoProtocolGuid,
        (VOID **)&ChildPciIo,
        This->DriverBindingHandle,
        Private->Handle,
        EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);
    if (EFI_ERROR(Status))
    {
        goto UninstallGop;
    }

    gBS->RestoreTPL(OldTpl);
    return EFI_SUCCESS;

UninstallGop:
    gBS->UninstallProtocolInterface(
        Private->Handle,
        &gEfiGraphicsOutputProtocolGuid,
        &Private->GraphicsOutput);

DestructMetaXVideoGraphics:
    MetaXVideoGraphicsOutputDestructor(Private);

FreeModeData:
    FreePool(Private->ModeData);

UninstallGopDevicePath:
    gBS->UninstallProtocolInterface(
        Private->Handle,
        &gEfiDevicePathProtocolGuid,
        Private->GopDevicePath);

FreeGopDevicePath:
    FreePool(Private->GopDevicePath);

RestoreAttributes:
    Private->PciIo->Attributes(
        Private->PciIo,
        EfiPciIoAttributeOperationSet,
        Private->OriginalPciAttributes,
        NULL);

ClosePciIo:
    gBS->CloseProtocol(
        ControllerHandle,
        &gEfiPciIoProtocolGuid,
        This->DriverBindingHandle,
        ControllerHandle);

FreePrivate:
    FreePool(Private);

RestoreTpl:
    gBS->RestoreTPL(OldTpl);

    return Status;
}

EFI_STATUS
EFIAPI
MetaXGpuDriverBindingStop(
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE ControllerHandle,
    IN UINTN NumberOfChildren,
    IN EFI_HANDLE *ChildHandleBuffer OPTIONAL)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    METAX_VIDEO_PRIVATE_DATA *Private;

    Gop = NULL;
    Private = NULL;

    Status = gBS->OpenProtocol(
        ControllerHandle,
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **)&Gop,
        This->DriverBindingHandle,
        ControllerHandle,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Private = METAX_VIDEO_PRIVATE_DATA_FROM_GOP(This);

    Status = gBS->UninstallProtocolInterface(
        ControllerHandle,
        &gEfiGraphicsOutputProtocolGuid,
        &Private->GraphicsOutput);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = gBS->CloseProtocol(
        ControllerHandle,
        &gEfiPciIoProtocolGuid,
        This->DriverBindingHandle,
        ControllerHandle);

    FreePool(Private);

    return Status;
}

EFI_STATUS
EFIAPI
MetaXGopDxeEntryPoint(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    return EfiLibInstallDriverBindingComponentName2(
        ImageHandle,
        SystemTable,
        &gMetaXGpuDriverBinding,
        ImageHandle,
        &gMetaXVideoComponentName,
        &gMetaXVideoComponentName2);
}
