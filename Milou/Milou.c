#include <ntddk.h>

#include "Milou.h"


#define MILOU_NT_DEVICE_NAME    L"\\Device\\MilouDrv"
#define MILOU_WIN32_DEVICE_NAME L"\\DosDevices\\MilouDrv"

// Driver init and unload functions
DRIVER_INITIALIZE   DriverEntry;
DRIVER_UNLOAD       DriverUnload;
// Driver dispatch functions
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH MilouDispatchOpenClose;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH MilouDispatchDeviceControl;


_Success_(return == TRUE)
BOOLEAN
RegisterCallbacks(
    _In_    PDEVICE_OBJECT  DeviceObject
)
{
    if (RegisterRegistryCallback(DeviceObject, L"Tintin")) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Registry callback enabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Registry callback not enabled!\n");
    }

    if (RegisterProcessCallback(FALSE)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Process creation/exit callback enabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Process creation/exit callback not enabled!\n");
    }

    if (RegisterThreadCallback()) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Thread creation/exit callback enabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Thread creation/exit callback not enabled!\n");
    }

    if (RegisterLoadImageCallback()) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Load image callback enabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Load image callback not enabled!\n");
    }

    return TRUE;
}

_Success_(return == TRUE)
BOOLEAN
UnregisterCallbacks(VOID)
{
    if (UnregisterRegistryCallback()) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Registry callback disabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Registry callback not disabled!\n");
    }

    if (UnregisterProcessCallback()) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Process creation/exit callback disabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Process creation/exit callback not disabled!\n");
    }

    if (UnregisterThreadCallback()) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Thread creation/exit callback disabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Thread creation/exit callback not disabled!\n");
    }

    if (UnregisterLoadImageCallback()) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Load image callback disabled!\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ " Load image callback not disabled!\n");
    }

    return TRUE;
}

NTSTATUS
MilouDispatchOpenClose(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
MilouDispatchDeviceControl(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _Inout_ PIRP            Irp
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION  pIoCurrStackLocation = IoGetCurrentIrpStackLocation(Irp);
    ULONG               ioControlCode = pIoCurrStackLocation->Parameters.DeviceIoControl.IoControlCode;

    switch (ioControlCode) {
    default:
        Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
        Irp->IoStatus.Information = 0;

        break;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
MilouInitCoreObjects(
    VOID
)
{
    InitializeListHead(&g_CallbackCtxListHead);
    ExInitializeFastMutex(&g_CallbackCtxListMutex);
    DetectOSVersion();
}

NTSTATUS
DriverEntry(
    _In_    PDRIVER_OBJECT  DriverObject,
    _In_    PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING  DeviceName;
    UNICODE_STRING  DeviceSymlinkName;
    PDEVICE_OBJECT  pDeviceObject = NULL;
    NTSTATUS        ntStatus = STATUS_SUCCESS;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou] Milou loading started\n");

    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = MilouDispatchOpenClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MilouDispatchOpenClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MilouDispatchDeviceControl;

    RtlInitUnicodeString(&DeviceName, MILOU_NT_DEVICE_NAME);
    ntStatus = IoCreateDevice(DriverObject,
                              0,
                              &DeviceName,
                              FILE_DEVICE_UNKNOWN,
                              FILE_DEVICE_SECURE_OPEN,
                              FALSE,
                              &pDeviceObject);
    if (!NT_SUCCESS(ntStatus)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to create a device: %#X\n", ntStatus);

        return ntStatus;
    }

    RtlInitUnicodeString(&DeviceSymlinkName, MILOU_WIN32_DEVICE_NAME);
    ntStatus = IoCreateSymbolicLink(&DeviceSymlinkName, &DeviceName);
    if (!NT_SUCCESS(ntStatus)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to create symbolic link: %#X\n", ntStatus);

        IoDeleteDevice(pDeviceObject);

        return ntStatus;
    }

    pDeviceObject->Flags |= DO_BUFFERED_IO;

    EventRegisterMilou_Driver();

    EventWriteMilouEvent(NULL, L"ETW log start");

    MilouInitCoreObjects();

    RegisterCallbacks(pDeviceObject);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou] Milou loading finished\n");

    return ntStatus;
}

VOID
DriverUnload(
    _In_    PDRIVER_OBJECT  DriverObject
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DriverObject);

    PDEVICE_OBJECT  pDeviceObject = NULL;
    UNICODE_STRING  DeviceSymlinkName;
    NTSTATUS        ntStatus;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou] Milou unloading started\n");

    UnregisterCallbacks();

    pDeviceObject = DriverObject->DeviceObject;
    RtlInitUnicodeString(&DeviceSymlinkName, MILOU_WIN32_DEVICE_NAME);

    ntStatus = IoDeleteSymbolicLink(&DeviceSymlinkName);
    if (!NT_SUCCESS(ntStatus)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to delete symbolic link: %#X\n", ntStatus);
    }
    IoDeleteDevice(pDeviceObject);

    EventWriteMilouEvent(NULL, L"ETW log end");
    EventUnregisterMilou_Driver();

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou] Milou unloading finished\n");
}