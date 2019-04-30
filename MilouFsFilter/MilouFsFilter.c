#include "pch.h"


MILOU_FILTER_GLOBALS MilouFilter;


/*************************************************************************
   Extensions
*************************************************************************/

CONST UNICODE_STRING InterestingExtensions[] = {
//    RTL_CONSTANT_STRING(L"exe"),
    RTL_CONSTANT_STRING(L"txt"),
    RTL_CONSTANT_STRING(L"pdf"),
    RTL_CONSTANT_STRING(L"doc"),
    RTL_CONSTANT_STRING(L"js"),
    RTL_CONSTANT_STRING(L"ps")
};


/*************************************************************************
   Operation registration
*************************************************************************/

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE,
      0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_READ,
      0,
      FsFilterPreOperation,
      NULL },

    { IRP_MJ_WRITE,
      0,
      FsFilterPreOperation,
      NULL },

    { IRP_MJ_SET_INFORMATION,
      FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_CLEANUP,
      0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_OPERATION_END }
};


extern const FLT_CONTEXT_REGISTRATION Contexts[];

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),           //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    Contexts,                           //  Context
    Callbacks,                          //  Operation callbacks

    FsFilterUnload,                     //  MiniFilterUnload

    FsInstanceSetup,                    //  InstanceSetup
    NULL,                               //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};


NTSTATUS 
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;

    ExInitializeDriverRuntime(DrvRtPoolNxOptIn);

    RtlZeroMemory(&MilouFilter, sizeof(MILOU_FILTER_GLOBALS));

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter loading started!\n");

    status = FltRegisterFilter(DriverObject,
                               &FilterRegistration,
                               &MilouFilter.gFilterHandle);

    if (NT_SUCCESS(status)) {
        
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter loaded!\n");

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter starting to filter!\n");

        status = FltStartFiltering(MilouFilter.gFilterHandle);

        if (!NT_SUCCESS(status)) {

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] FS Minifilter filtering!!!\n");

            FltUnregisterFilter(MilouFilter.gFilterHandle);
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

    FltUnregisterFilter(MilouFilter.gFilterHandle);
    MilouFilter.gFilterHandle = NULL;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] FS Minifilter unloaded!\n");

    return STATUS_SUCCESS;
}

NTSTATUS
FsInstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);

    PAGED_CODE();

    //
    //  Don't attach to network volumes.
    //

    if (VolumeDeviceType == FILE_DEVICE_NETWORK_FILE_SYSTEM) {

        return STATUS_FLT_DO_NOT_ATTACH;
    }

    return STATUS_SUCCESS;
}


// Probably moving every IRP to his own function whou be better for perf.
FLT_PREOP_CALLBACK_STATUS
FsFilterPreOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    BOOLEAN race;
    NTSTATUS status;
    PEPROCESS process;
    PCTX_STREAMHANDLE_CONTEXT streamHandleContext = NULL;

    UNREFERENCED_PARAMETER(FltObjects);

    PAGED_CODE();

    // Getting a pointer to the EPROCESS, still wont use this but it may be usefull
    process = IoThreadToProcess(Data->Thread);


    switch (Data->Iopb->MajorFunction) {
    case IRP_MJ_CREATE:
        // We will check eveyrhtin on the post-op and create the CTX if necessary
        return FLT_PREOP_SUCCESS_WITH_CALLBACK;
        break;
    case IRP_MJ_READ:
        // Here we want to log what the process is trying to read
        // We obtain the context to see if we are interested in this file (Context should've been init on IRP_MJ_CREATE, if not we dont return)
        status = CtxFindOrCreateStreamContext(Data, FALSE, &streamHandleContext, NULL);

        if (!NT_SUCCESS(status)) {
            // DbgPrintEx
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] No context for IRP_MJ_READ\n");
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        if (streamHandleContext->ExtensionMatch) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] File: %wZ from folder: %wZ going to be read\n", 
                streamHandleContext->NameInfo->FinalComponent,
                streamHandleContext->NameInfo->ParentDir);

            FltReleaseContext(streamHandleContext);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
        break;
    case IRP_MJ_WRITE:

        status = CtxFindOrCreateStreamContext(Data, FALSE, &streamHandleContext, NULL);

        if (!NT_SUCCESS(status)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] No context for IRP_MJ_WRITE\n");
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        if (streamHandleContext->ExtensionMatch) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] File: %wZ from folder: %wZ going to be written\n",
                streamHandleContext->NameInfo->FinalComponent,
                streamHandleContext->NameInfo->ParentDir);

            FltReleaseContext(streamHandleContext);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
        break;
    case IRP_MJ_SET_INFORMATION:

        switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass) {
         // Is possible to put a file into pendig-delete by setting this two values
        case FileDispositionInformation:
        case FileDispositionInformationEx:

            status = CtxFindOrCreateStreamContext(Data, FALSE, &streamHandleContext, NULL);

            if (!NT_SUCCESS(status)) {
                DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] No context for IRP_MJ_SET_INFORMATION\n");
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }

            race = (InterlockedIncrement(&streamHandleContext->NumOps) > 1);

            if (!race) {

                *CompletionContext = (PVOID)streamHandleContext;

                return FLT_PREOP_SYNCHRONIZE;
            }
            else {
                FltReleaseContext(streamHandleContext);
            }

        default:
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
            break;
        }
    case IRP_MJ_CLEANUP:

        status = CtxFindOrCreateStreamContext(Data, FALSE, &streamHandleContext, NULL);

        if (!NT_SUCCESS(status)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] No context for IRP_MJ_CLEANUP\n");
            return FLT_PREOP_SUCCESS_NO_CALLBACK;
        }

        status = GetFileNameInformation(Data, streamHandleContext);

        if (NT_SUCCESS(status)) {

            *CompletionContext = (PVOID) streamHandleContext;

            return FLT_PREOP_SYNCHRONIZE;
        }
        else {
            FltReleaseContext(streamHandleContext);
        }

        return FLT_PREOP_SUCCESS_NO_CALLBACK;
        break;
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}


FLT_POSTOP_CALLBACK_STATUS
FsFilterPostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{

    NTSTATUS status;
    BOOLEAN contextCreated;
    PCTX_STREAMHANDLE_CONTEXT streamHandleContext;

    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    
    PAGED_CODE();

    streamHandleContext = (PCTX_STREAMHANDLE_CONTEXT) CompletionContext;

    switch (Data->Iopb->MajorFunction) {
    case IRP_MJ_CREATE:
        // We only get here if the file was open with option FILE_DELETE_ON_CLOSE or the extension matches
        if (NT_SUCCESS(Data->IoStatus.Status) &&
            (STATUS_REPARSE != Data->IoStatus.Status)) {

            if (Data->Iopb->Parameters.Create.Options == FILE_DELETE_ON_CLOSE || CheckExtension(Data)) {

                status = CtxFindOrCreateStreamContext(Data, TRUE, &streamHandleContext, &contextCreated);

                if (!NT_SUCCESS(status) || !contextCreated) {
                    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] Error creating context on IRP_MJ_CREATE\n");
                    return FLT_PREOP_SUCCESS_NO_CALLBACK;
                }

                CtxAcquireResourceExclusive(streamHandleContext->Resource);
                if (Data->Iopb->Parameters.Create.Options == FILE_DELETE_ON_CLOSE) {
                    streamHandleContext->DeleteOnClose = BooleanFlagOn(Data->Iopb->Parameters.Create.Options, FILE_DELETE_ON_CLOSE);
                }
                else {
                    GetFileNameInformation(Data, streamHandleContext);
                    streamHandleContext->ExtensionMatch = TRUE;
                }
                CtxReleaseResource(streamHandleContext->Resource);

                FltReleaseContext(streamHandleContext);
            }
        }

        return FLT_POSTOP_FINISHED_PROCESSING;
        break;
    case IRP_MJ_SET_INFORMATION:
        // Exact copy from Delete example from Microsoft Drivers Samples
        // https://github.com/Microsoft/Windows-driver-samples/blob/41c29cb92feff490270b4ce31f67d7baddecc457/filesys/miniFilter/delete/delete.c#L2929
        
        if (Data->IoStatus.Status) {
            if (Data->Iopb->Parameters.SetFileInformation.FileInformationClass == FileDispositionInformationEx) {
                
                ULONG flags = ((PFILE_DISPOSITION_INFORMATION_EX)Data->Iopb->Parameters.SetFileInformation.InfoBuffer)->Flags;

                if (FlagOn(flags, FILE_DISPOSITION_ON_CLOSE)) {

                    streamHandleContext->DeleteOnClose = BooleanFlagOn(flags, FILE_DISPOSITION_DELETE);
                }
                else {

                    streamHandleContext->SetDisp = BooleanFlagOn(flags, FILE_DISPOSITION_DELETE);
                }

            }
            else {

                streamHandleContext->SetDisp = ((PFILE_DISPOSITION_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer)->DeleteFile;
            }

        }

        InterlockedDecrement(&streamHandleContext->NumOps);

        FltReleaseContext(streamHandleContext);

        return FLT_POSTOP_FINISHED_PROCESSING;
        break;
    case IRP_MJ_CLEANUP:
        if (NT_SUCCESS(Data->IoStatus.Status)) {

            FILE_STANDARD_INFORMATION fileInfo;

            if (streamHandleContext->DeleteOnClose || streamHandleContext->NumOps || streamHandleContext->SetDisp) {
                
                status = FltQueryInformationFile(Data->Iopb->TargetInstance,
                    Data->Iopb->TargetFileObject,
                    &fileInfo,
                    sizeof(fileInfo),
                    FileStandardInformation,
                    NULL);
                
                if (status == STATUS_FILE_DELETED) {
                    // Check if is really deleted (Try to open the file)
                    // Log if it was really deletedsii

                }
            }
        }
        return FLT_POSTOP_FINISHED_PROCESSING;
        break;
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}


// Probably should be on a FsFilterUtils...
BOOLEAN
CheckExtension(
    _In_ PFLT_CALLBACK_DATA Data
)
{

    NTSTATUS status;

    PFLT_FILE_NAME_INFORMATION nameInfo;

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Error checking extension");
        return FALSE;
    }

    FltParseFileNameInformation(nameInfo);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Checking file: %wZ extension: %wZ\n", &nameInfo->Name, &nameInfo->Extension);

    if (nameInfo->Extension.Length == 0) {
        FltReleaseFileNameInformation(nameInfo);
        return FALSE;
    }

    for (ULONG i = 0; i < sizeof(InterestingExtensions)/sizeof(InterestingExtensions[0]); i++) {
        if (RtlCompareUnicodeString(&InterestingExtensions[i], &nameInfo->Extension, TRUE) == 0) {
            FltReleaseFileNameInformation(nameInfo);
            return TRUE;
        }
    }

    FltReleaseFileNameInformation(nameInfo);
    return FALSE;
}
