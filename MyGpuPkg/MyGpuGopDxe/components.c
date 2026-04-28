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

#include "driver.h"
#define MYGPU_VENDOR_ID  0xAAAA
#define MYGPU_DEVICE_ID  0xBBBB

//
// 'M' 'G' 'P' 'U'
//
#define MYGPU_PRIVATE_SIGNATURE  SIGNATURE_32('M', 'G', 'P', 'U')

#define MYGPU_PRIVATE_FROM_GOP(a) \
  BASE_CR(a, MYGPU_PRIVATE, Gop)

typedef struct {
    UINT16 VendorId;
    UINT16 DeviceId;
    UINT16 Command;
    UINT16 Status;
    UINT8  RevisionId;
    UINT8  ProgIf;
    UINT8  SubClass;
    UINT8  BaseClass;
} PCI_TYPE0_HEAD_MIN;

STATIC
EFI_STATUS
MyGpuGetBarBase (
    IN  EFI_PCI_IO_PROTOCOL  *PciIo,
    IN  UINT8                BarIndex,
    OUT EFI_PHYSICAL_ADDRESS *Base,
    OUT UINT64               *Size
    )
{
    EFI_STATUS                         Status;
    UINT64                             Supports;
    VOID                               *Resources;
    EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Desc;
    EFI_ACPI_END_TAG_DESCRIPTOR        *End;

    if (PciIo == NULL || Base == NULL || Size == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    *Base     = 0;
    *Size     = 0;
    Supports  = 0;
    Resources = NULL;

    Status = PciIo->GetBarAttributes (
                      PciIo,
                      BarIndex,
                      &Supports,
                      &Resources
                      );

    Print (
      L"BAR%d GetBarAttributes Status=%r Supports=0x%lx Resources=%p\n",
      BarIndex,
      Status,
      Supports,
      Resources
      );

    if (EFI_ERROR (Status)) {
        return Status;
    }

    if (Resources == NULL) {
        return EFI_NOT_FOUND;
    }

    Desc = (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)Resources;

    while (Desc->Desc == ACPI_ADDRESS_SPACE_DESCRIPTOR) {
        Print (
          L"BAR%d Desc ResType=%x AddrMin=0x%lx AddrLen=0x%lx\n",
          BarIndex,
          Desc->ResType,
          Desc->AddrRangeMin,
          Desc->AddrLen
          );

        if (Desc->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM) {
            *Base = Desc->AddrRangeMin;
            *Size = Desc->AddrLen;
            FreePool (Resources);
            return EFI_SUCCESS;
        }

        Desc++;
    }

    End = (EFI_ACPI_END_TAG_DESCRIPTOR *)Desc;
    Print (
      L"BAR%d EndTag Desc=0x%x Checksum=0x%x\n",
      BarIndex,
      End->Desc,
      End->Checksum
      );

    FreePool (Resources);
    return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
MyGpuDriverBindingSupoorted (
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE                  ControllerHandle,
    IN EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath OPTIONAL
    )
{
    EFI_STATUS          Status;
    EFI_PCI_IO_PROTOCOL *PciIo;
    PCI_TYPE0_HEAD_MIN  Pci;

    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
    }

    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint16,
                          0x00,
                          1,
                          &Pci.VendorId
                          );
    if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
    }

    Status = PciIo->Pci.Read (
                          PciIo,
                          EfiPciIoWidthUint16,
                          0x02,
                          1,
                          &Pci.DeviceId
                          );
    if (EFI_ERROR (Status)) {
        return EFI_UNSUPPORTED;
    }

    if (Pci.VendorId == MYGPU_VENDOR_ID &&
        Pci.DeviceId == MYGPU_DEVICE_ID) {
        Print (
          L"MyGpu PCI Vendor=%04x Device=%04x\n",
          Pci.VendorId,
          Pci.DeviceId
          );
        Print (L"MyGpu Supported match!\n");
        return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
MyGpuDriverBindingStart (
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE                  ControllerHandle,
    IN EFI_DEVICE_PATH_PROTOCOL    *RemainingDevicePath OPTIONAL
    )
{
    EFI_STATUS                    Status;
    EFI_PCI_IO_PROTOCOL           *PciIo;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *ExistingGop;
    MYGPU_PRIVATE                 *Private;
    MYGPU_PRIVATE                 *ExistingPrivate;
    UINT64                        Supports;

    PciIo           = NULL;
    ExistingGop     = NULL;
    Private         = NULL;
    ExistingPrivate = NULL;
    Supports        = 0;

    Print (L"MyGpu Start called, Controller=%p\n", ControllerHandle);

    //
    // 先看这个 ControllerHandle 上是否已经有 GOP。
    // 如果是自己的 GOP，说明重复连接，直接成功返回。
    // 如果是别人的 GOP，不要绑定这个 Controller。
    //
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **)&ExistingGop,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );

    Print (
      L"MyGpu initial GOP check Status=%r ExistingGop=%p\n",
      Status,
      ExistingGop
      );

    if (!EFI_ERROR (Status)) {
        ExistingPrivate = BASE_CR (ExistingGop, MYGPU_PRIVATE, Gop);

        Print (
          L"MyGpu ExistingPrivate=%p Signature=0x%x Expected=0x%x\n",
          ExistingPrivate,
          ExistingPrivate->Signature,
          MYGPU_PRIVATE_SIGNATURE
          );

        if (ExistingPrivate->Signature == MYGPU_PRIVATE_SIGNATURE) {
            Print (L"MyGpu GOP already installed by me\n");
            return EFI_SUCCESS;
        }

        Print (L"MyGpu GOP exists but not mine, skip this controller\n");
        return EFI_UNSUPPORTED;
    }

    //
    // 正式绑定 PCI controller。
    //
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiPciIoProtocolGuid,
                    (VOID **)&PciIo,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );

    Print (L"MyGpu OpenProtocol BY_DRIVER Status=%r\n", Status);

    if (Status == EFI_ALREADY_STARTED) {
        Print (L"MyGpu already started, skip duplicate init\n");
        return EFI_SUCCESS;
    }

    if (EFI_ERROR (Status)) {
        return Status;
    }

    Private = AllocateZeroPool (sizeof (MYGPU_PRIVATE));
    if (Private == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        Print (L"MyGpu Start %d Status=%r\n", __LINE__, Status);
        goto ErrorClose;
    }

    //
    // Signature 必须在 InstallProtocolInterface 之前设置。
    //
    Private->Signature = MYGPU_PRIVATE_SIGNATURE;
    Private->Handle    = ControllerHandle;
    Private->PciIo     = PciIo;

    Status = PciIo->Attributes (
                      PciIo,
                      EfiPciIoAttributeOperationSupported,
                      0,
                      &Supports
                      );
    if (EFI_ERROR (Status)) {
        Print (L"MyGpu Attributes Supported %d Status=%r\n", __LINE__, Status);
        goto ErrorFree;
    }

    //
    // 你当前 QEMU 设计是 BAR0 = framebuffer。
    //
    Status = MyGpuGetBarBase (
               PciIo,
               0,
               &Private->FramebufferBase,
               &Private->FrameBufferSize
               );
    if (EFI_ERROR (Status)) {
        Print (L"MyGpu Get BAR0 %d Status=%r\n", __LINE__, Status);
        goto ErrorFree;
    }

    //
    // GOP 对外报告实际画面使用大小，不一定等于 BAR 窗口大小。
    //
    Private->FrameBufferSize = MYGPU_FB_SIZE;

    Private->GopModeInfo.Version              = 0;
    Private->GopModeInfo.HorizontalResolution = MYGPU_WIDTH;
    Private->GopModeInfo.VerticalResolution   = MYGPU_HEIGHT;
    Private->GopModeInfo.PixelFormat          = PixelBlueGreenRedReserved8BitPerColor;
    Private->GopModeInfo.PixelsPerScanLine    = MYGPU_WIDTH;

    Private->GopMode.MaxMode         = 1;
    Private->GopMode.Mode            = 0;
    Private->GopMode.Info            = &Private->GopModeInfo;
    Private->GopMode.SizeOfInfo      = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    Private->GopMode.FrameBufferBase = Private->FramebufferBase;
    Private->GopMode.FrameBufferSize = Private->FrameBufferSize;

    Private->Gop.QueryMode = MyGpuGopQueryMode;
    Private->Gop.SetMode   = MyGpuGopSetMode;
    Private->Gop.Blt       = MyGpuGopBlt;
    Private->Gop.Mode      = &Private->GopMode;

    Status = gBS->InstallProtocolInterface (
                    &ControllerHandle,
                    &gEfiGraphicsOutputProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &Private->Gop
                    );

    Print (
      L"MyGpu Install GOP Status=%r Gop=%p Signature=0x%x\n",
      Status,
      &Private->Gop,
      Private->Signature
      );

    if (EFI_ERROR (Status)) {
        Print (L"MyGpu Start %d Status=%r\n", __LINE__, Status);
        goto ErrorUninstallGop;
    }

    Print (L"MyGpu Start success\n");
    return EFI_SUCCESS;

ErrorUninstallGop:
    gBS->UninstallProtocolInterface (
           ControllerHandle,
           &gEfiGraphicsOutputProtocolGuid,
           &Private->Gop
           );

ErrorFree:
    if (Private != NULL) {
        FreePool (Private);
    }

ErrorClose:
    gBS->CloseProtocol (
           ControllerHandle,
           &gEfiPciIoProtocolGuid,
           This->DriverBindingHandle,
           ControllerHandle
           );

    Print (L"MyGpu Start Status=%r\n", Status);
    return Status;
}

EFI_STATUS
EFIAPI
MyGpuDriverBindingStop (
    IN EFI_DRIVER_BINDING_PROTOCOL *This,
    IN EFI_HANDLE                  ControllerHandle,
    IN UINTN                       NumberOfChildren,
    IN EFI_HANDLE                  *ChildHandleBuffer OPTIONAL
    )
{
    EFI_STATUS                    Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;
    MYGPU_PRIVATE                 *Private;

    Gop     = NULL;
    Private = NULL;

    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **)&Gop,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
        return Status;
    }

    Private = MYGPU_PRIVATE_FROM_GOP (Gop);

    Status = gBS->UninstallProtocolInterface (
                    ControllerHandle,
                    &gEfiGraphicsOutputProtocolGuid,
                    &Private->Gop
                    );
    if (EFI_ERROR (Status)) {
        return Status;
    }

    Status = gBS->CloseProtocol (
                    ControllerHandle,
                    &gEfiPciIoProtocolGuid,
                    This->DriverBindingHandle,
                    ControllerHandle
                    );

    FreePool (Private);

    return Status;
}

EFI_DRIVER_BINDING_PROTOCOL gMyGpuDriverBinding = {
    MyGpuDriverBindingSupoorted,
    MyGpuDriverBindingStart,
    MyGpuDriverBindingStop,
    0x10,
    NULL,
    NULL
};

EFI_STATUS
EFIAPI
MyGpuGopDxeEntryPoint (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
    )
{
    return EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gMyGpuDriverBinding,
             ImageHandle,
             &gQemuVideoComponentName,
             &gQemuVideoComponentName2
             );
}