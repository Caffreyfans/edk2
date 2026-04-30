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
    {PCI_CLASS_DISPLAY_OTHER,
     METAX_VENDOR_ID,
     N400_VENDOR_ID,
     METAX_VIDEO_N400,
     L"N400"}};

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

// STATIC
// EFI_STATUS
// MetaXGpuGetBarBase(
//     IN EFI_PCI_IO_PROTOCOL *PciIo,
//     IN UINT8 BarIndex,
//     OUT EFI_PHYSICAL_ADDRESS *Base,
//     OUT UINT64 *Size)
// {
//     EFI_STATUS Status;
//     UINT64 Supports;
//     VOID *Resources;
//     EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *Desc;

//     if (PciIo == NULL || Base == NULL || Size == NULL)
//     {
//         return EFI_INVALID_PARAMETER;
//     }

//     *Base = 0;
//     *Size = 0;
//     Supports = 0;
//     Resources = NULL;

//     Status = PciIo->GetBarAttributes(
//         PciIo,
//         BarIndex,
//         &Supports,
//         &Resources);

//     if (EFI_ERROR(Status))
//     {
//         return Status;
//     }

//     if (Resources == NULL)
//     {
//         return EFI_NOT_FOUND;
//     }

//     Desc = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Resources;

//     while (Desc->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR)
//     {
//         if (Desc->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM)
//         {
//             *Base = Desc->AddrRangeMin;
//             *Size = Desc->AddrLen;
//             FreePool(Resources);
//             return EFI_SUCCESS;
//         }

//         Desc++;
//     }

//     FreePool(Resources);
//     return EFI_NOT_FOUND;
// }

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