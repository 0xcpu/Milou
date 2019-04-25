#include <fltKernel.h>

#include "MilouFsFilter.h"

PFLT_FILTER gFilterHandle;

NTSTATUS 
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter loading started!\n");

    status = FltRegisterFilter(DriverObject,
                               &FilterRegistration,
                               &gFilterHandle);

    if (NT_SUCCESS(status)) {
        
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter loaded!\n");

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter starting to filter!\n");

        status = FltStartFiltering(gFilterHandle);

        if (!NT_SUCCESS(status)) {

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] FS Minifilter filtering!!!\n");

            FltUnregisterFilter(gFilterHandle);
        }
        else {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter filtering!!!\n");
        }
    }

    return status;
}

NTSTATUS
FsFilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
{
	
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();
	
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter unloading started!\n");

    FltUnregisterFilter(gFilterHandle);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter unloaded!\n");

    return STATUS_SUCCESS;
}

NTSTATUS
FsFilterQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();

    return STATUS_SUCCESS;
}


FLT_PREOP_CALLBACK_STATUS
FsFilterPreOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    UNICODE_STRING fileName;

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);


    PFILE_OBJECT fileObject = FltObjects->FileObject;
    fileName = fileObject->FileName;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] File: %wZ\n", &fileName);


    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

// We are not using it right now but is needed, just pass through
FLT_POSTOP_CALLBACK_STATUS
FsFilterPostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    return FLT_POSTOP_FINISHED_PROCESSING;
}
