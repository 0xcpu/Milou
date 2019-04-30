#include "pch.h"



/*************************************************************************
    Context Registration
*************************************************************************/

CONST FLT_CONTEXT_REGISTRATION Contexts[] = {

    { FLT_STREAMHANDLE_CONTEXT,          //ContextType
      0,                                 //Flags
      CtxContextCleanup,                 //ContextCleanupCallback
      CTX_STREAMHANDLE_CONTEXT_SIZE,     //Size
      CTX_STREAMHANDLE_CONTEXT_TAG       //PoolTag
    },

    { FLT_CONTEXT_END }

};


VOID
CtxContextCleanup(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
)
{

    PCTX_STREAMHANDLE_CONTEXT streamHandleContext;

    PAGED_CODE();

    // Just-in-case we add more CTXs
    switch (ContextType) {
    case FLT_STREAM_CONTEXT:

        streamHandleContext = (PCTX_STREAMHANDLE_CONTEXT) Context;


        if (streamHandleContext->Resource != NULL) {

            ExDeleteResourceLite(streamHandleContext->Resource);
            CtxFreeResource(streamHandleContext->Resource);
        }

        if (streamHandleContext->NameInfo != NULL) {

            FltReleaseFileNameInformation(streamHandleContext->NameInfo);
            streamHandleContext->NameInfo = NULL;
        }

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Clean up StreamHandle context completed!\n");

        break;
    }
}


NTSTATUS
CtxFindOrCreateStreamContext(
    _In_ PFLT_CALLBACK_DATA Cbd,
    _In_ BOOLEAN CreateIfNotFound,
    _Outptr_ PCTX_STREAMHANDLE_CONTEXT *StreamHandleContext,
    _Out_opt_ PBOOLEAN ContextCreated
)
{
    NTSTATUS status;
    PCTX_STREAMHANDLE_CONTEXT streamHandleContext;
    PCTX_STREAMHANDLE_CONTEXT oldStreamHandleContext;

    PAGED_CODE();

    // DbgPrintEx
    *StreamHandleContext = NULL;
    if (ContextCreated != NULL) *ContextCreated = FALSE;
    
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Trying to get the Context!\n");
    status = FltGetStreamHandleContext(Cbd->Iopb->TargetInstance, Cbd->Iopb->TargetFileObject, &streamHandleContext);

    if (!NT_SUCCESS(status) && (status == STATUS_NOT_FOUND) && CreateIfNotFound) {

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Trying to create the Context!\n");
        status = CtxCreateStreamHandleContext(&streamHandleContext);

        if (!NT_SUCCESS(status)) {

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] Error creating Context!\n");
            return status;
        }

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Trying to set the Context!\n");
        status = FltSetStreamHandleContext(Cbd->Iopb->TargetInstance, 
            Cbd->Iopb->TargetFileObject, 
            FLT_SET_CONTEXT_KEEP_IF_EXISTS, 
            streamHandleContext,
            &oldStreamHandleContext);

        if (!NT_SUCCESS(status)) {
        
            FltReleaseContext(streamHandleContext);

            if (status != STATUS_FLT_CONTEXT_ALREADY_DEFINED) {

                DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] Error setting context!\n");
                return status;
            }

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Context already defined!\n");
            streamHandleContext = oldStreamHandleContext;
            status = STATUS_SUCCESS;
        } else {

            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou FS MiniFilter] Context set!\n");
            if (ContextCreated != NULL) *ContextCreated = TRUE;
        }
    }

    *StreamHandleContext = streamHandleContext;

    return status;
}

NTSTATUS
CtxCreateStreamHandleContext(
    _Outptr_ PCTX_STREAMHANDLE_CONTEXT *StreamHandleContext
)
{
    NTSTATUS status;
    PCTX_STREAMHANDLE_CONTEXT streamHandleContext;

    PAGED_CODE();

    // DbgPrintEx
    status = FltAllocateContext(MilouFilter.gFilterHandle, 
                                FLT_STREAMHANDLE_CONTEXT, 
                                CTX_STREAMHANDLE_CONTEXT_SIZE, 
                                PagedPool, 
                                &streamHandleContext);

    if (!NT_SUCCESS(status)) {

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou FS MiniFilter] Error allocating context!\n");

        return status;
    }

    RtlZeroMemory(streamHandleContext, CTX_STREAMHANDLE_CONTEXT_SIZE);

    streamHandleContext->Resource = CtxAllocateResource();
    if (streamHandleContext->Resource == NULL) {

        // DbgPrintEx
        FltReleaseContext(streamHandleContext);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ExInitializeResourceLite(streamHandleContext->Resource);

    *StreamHandleContext = streamHandleContext;

    // DbgPrintEx
    return STATUS_SUCCESS;
}

// ADD DBGPRINTs
NTSTATUS
GetFileNameInformation(
    _In_  PFLT_CALLBACK_DATA Data,
    _Inout_ PCTX_STREAMHANDLE_CONTEXT StreamHandleContext
)
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION newNameInfo;
    PFLT_FILE_NAME_INFORMATION oldNameInfo;


    // DbgPrintEx
    status = FltGetFileNameInformation(Data, (FLT_FILE_NAME_OPENED | FLT_FILE_NAME_QUERY_DEFAULT), &newNameInfo);

    if (!NT_SUCCESS(status)) {
        // DbgPrintEx
        return status;
    }

    status = FltParseFileNameInformation(newNameInfo);

    if (!NT_SUCCESS(status)) {
        // DbgPrintEx
        return status;
    }

    oldNameInfo = InterlockedExchangePointer(&StreamHandleContext->NameInfo, newNameInfo);

    if (oldNameInfo != NULL) {
        FltReleaseFileNameInformation(oldNameInfo);
    }

    // DbgPrintEx
    return status;

}

/*
NTSTATUS
GetFileId(
    _In_  PFLT_CALLBACK_DATA Data,
    _Inout_ PCTX_STREAM_CONTEXT StreamContext
)
{
    NTSTATUS status;
    FILE_INTERNAL_INFORMATION fileInternalInformation;

    PAGED_CODE();

    // DbgPrintEx
    if (!StreamContext->FileId.FileId128.Identifier) {

        status = FltQueryInformationFile(Data->Iopb->TargetInstance,
                                         Data->Iopb->TargetFileObject,
                                         &fileInternalInformation,
                                         sizeof(FILE_INTERNAL_INFORMATION),
                                         FileInternalInformation,
                                         NULL);

        if (NT_SUCCESS(status)) {

            if (fileInternalInformation.IndexNumber.QuadPart == FILE_INVALID_FILE_ID) {

                FILE_ID_INFORMATION fileIdInformation;

                status = FltQueryInformationFile(Data->Iopb->TargetInstance,
                    Data->Iopb->TargetFileObject,
                    &fileIdInformation,
                    sizeof(FILE_ID_INFORMATION),
                    FileIdInformation,
                    NULL);

                if (NT_SUCCESS(status)) {

                    RtlCopyMemory(&StreamContext->FileId,
                        &fileInternalInformation.IndexNumber,
                        sizeof(StreamContext->FileId));

                    KeMemoryBarrier();
                }
            }
        }
        else {

            StreamContext->FileId.FileId64.Value = fileInternalInformation.IndexNumber.QuadPart;
            StreamContext->FileId.FileId64.UpperZeroes = 0ll;

            KeMemoryBarrier();
        }

    }
    return status;
}
*/
