#include "MilouUtils.h"

FAST_MUTEX  g_CallbackCtxListMutex;
LIST_ENTRY  g_CallbackCtxListHead;
SIZE_T      g_CallbackCtxListNumEntries;
BOOLEAN     g_IsWindows8OrGreater;


_Use_decl_annotations_
PVOID
CreateCallbackContext(
    _In_    PCWSTR  AltitudeName
)
{
    PMILOU_CALLBACK_CONTEXT pMilouCallbackCtx = NULL;

    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)ExAllocatePoolWithTag(PagedPool,
                                                                       sizeof(MILOU_CALLBACK_CONTEXT),
                                                                       MILOU_CTX_TAG);
    if (NULL == pMilouCallbackCtx) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "[Milou] Failed to allocate callback ctx\n");

        return NULL;
    }
    RtlSecureZeroMemory(pMilouCallbackCtx, sizeof(MILOU_CALLBACK_CONTEXT));

    RtlInitUnicodeString(&pMilouCallbackCtx->Altitude, AltitudeName);

    return pMilouCallbackCtx;
}

_Use_decl_annotations_
BOOLEAN
InsertCallbackContext(
    _In_    PMILOU_CALLBACK_CONTEXT CallbackCtx
)
{
    ExAcquireFastMutex(&g_CallbackCtxListMutex);

    InterlockedIncrement((volatile LONG *)&g_CallbackCtxListNumEntries);
    InsertHeadList(&g_CallbackCtxListHead, &CallbackCtx->CallbackCtxList);

    ExReleaseFastMutex(&g_CallbackCtxListMutex);

    return TRUE;
}

_Use_decl_annotations_
PMILOU_CALLBACK_CONTEXT
FindCallbackContext(
    _In_    LARGE_INTEGER   Cookie)
{
    PMILOU_CALLBACK_CONTEXT pMilouCallbackCtx = NULL;
    PLIST_ENTRY             pListEntry = NULL;

    ExAcquireFastMutex(&g_CallbackCtxListMutex);

    pListEntry = g_CallbackCtxListHead.Flink;
    while (pListEntry != &g_CallbackCtxListHead) {
        pMilouCallbackCtx = CONTAINING_RECORD(pListEntry,
                                              MILOU_CALLBACK_CONTEXT,
                                              CallbackCtxList);
        if (pMilouCallbackCtx->Cookie.QuadPart == Cookie.QuadPart) {
            break;
        }

        pListEntry = pListEntry->Flink;
    }

    ExReleaseFastMutex(&g_CallbackCtxListMutex);

    return pMilouCallbackCtx;
}

_Use_decl_annotations_
PMILOU_CALLBACK_CONTEXT
FindAndRemoveCallbackContext(
    _In_    LARGE_INTEGER   Cookie
)
{
    PMILOU_CALLBACK_CONTEXT pMilouCallbackCtx = NULL;
    PLIST_ENTRY             pListEntry = NULL;

    ExAcquireFastMutex(&g_CallbackCtxListMutex);

    pListEntry = g_CallbackCtxListHead.Flink;
    while (pListEntry != &g_CallbackCtxListHead) {
        pMilouCallbackCtx = CONTAINING_RECORD(pListEntry,
                                              MILOU_CALLBACK_CONTEXT,
                                              CallbackCtxList);
        if (pMilouCallbackCtx->Cookie.QuadPart == Cookie.QuadPart) {
            RemoveEntryList(&pMilouCallbackCtx->CallbackCtxList);
            InterlockedDecrement((volatile LONG *)&g_CallbackCtxListNumEntries);

            break;
        }

        pListEntry = pListEntry->Flink;
    }

    ExReleaseFastMutex(&g_CallbackCtxListMutex);

    return pMilouCallbackCtx;
}

VOID
DeleteCallbackContext(
    _In_    PMILOU_CALLBACK_CONTEXT CallbackCtx
)
{
    if (CallbackCtx != NULL) {
        ExFreePoolWithTag(CallbackCtx, MILOU_CTX_TAG);
        CallbackCtx = NULL;
    }
}

VOID
DetectOSVersion()
{
    NTSTATUS                Status;
    RTL_OSVERSIONINFOEXW    VersionInfo = { 0 };
    ULONGLONG               ConditionMask = 0;

    VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
    VersionInfo.dwMajorVersion = 6;
    VersionInfo.dwMinorVersion = 1;

    VER_SET_CONDITION(ConditionMask, VER_MAJORVERSION, VER_LESS_EQUAL);
    VER_SET_CONDITION(ConditionMask, VER_MINORVERSION, VER_LESS_EQUAL);

    Status = RtlVerifyVersionInfo(&VersionInfo,
                                  VER_MAJORVERSION | VER_MINORVERSION,
                                  ConditionMask);
    if (NT_SUCCESS(Status)) {
        g_IsWindows8OrGreater = FALSE;

        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   "[Milou] Detected Windows 7 or an older version.\n");
    } else if (Status == STATUS_REVISION_MISMATCH) {
        g_IsWindows8OrGreater = TRUE;
        
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   "[Milou] Detected Windows 8 or a newer version.\n");
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "[Milou] Failed to detect Windows version. Assuming it is Windows 8 or newer.\n");

        g_IsWindows8OrGreater = TRUE;
    }
}

// Adapted from https://github.com/Microsoft/Windows-driver-samples/blob/6c1981b8504329521343ad00f32daa847fa6083a/general/registry/regfltr/sys/capture.c#L405
_Use_decl_annotations_
NTSTATUS
CaptureBuffer(
    PVOID   *CapturedBuffer,
    PVOID   Buffer,
    SIZE_T  Length,
    ULONG   PoolTag
)
{
    NTSTATUS    ntStatus = STATUS_SUCCESS;
    PVOID       tempBuffer = NULL;

    NT_ASSERT(CapturedBuffer != NULL);

    if (Length == 0) {
        *CapturedBuffer = NULL;

        return ntStatus;
    }

    tempBuffer = (unsigned char *)ExAllocatePoolWithTag(PagedPool, Length, PoolTag);
    if (tempBuffer != NULL) {
        try {
            RtlCopyMemory(tempBuffer, Buffer, Length);
        } except(EXCEPTION_EXECUTE_HANDLER) {
            ExFreePoolWithTag(tempBuffer, PoolTag);
            tempBuffer = NULL;

            ntStatus = GetExceptionCode();
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "[Milou] Failed capturing buffer, exception %#X\n",
                       ntStatus);
        }
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "[Milou] Failed capturing buffer, insufficient resources!\n");

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    *CapturedBuffer = tempBuffer;

    return ntStatus;
}


VOID
FreeCapturedBuffer(
    _In_    PVOID   CapturedBuffer,
    _In_    ULONG   PoolTag
)
{
    if (CapturedBuffer != NULL) {
        ExFreePoolWithTag(CapturedBuffer, PoolTag);
        CapturedBuffer = NULL;
    }
}


NTSTATUS
CaptureUnicodeString(
    _Inout_ PUNICODE_STRING     DestString,
    _In_    PCUNICODE_STRING    SourceString,
    _In_    ULONG               PoolTag
)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (SourceString->Length == 0) {
        DestString->Buffer = NULL;
        DestString->Length = 0;
        DestString->MaximumLength = 0;

        return ntStatus;
    }

    DestString->Length = SourceString->Length;
    DestString->MaximumLength = SourceString->Length + sizeof(WCHAR);

    DestString->Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool,
                                                      DestString->MaximumLength,
                                                      PoolTag);
    if (DestString->Buffer != NULL) {

        RtlSecureZeroMemory(DestString->Buffer, DestString->MaximumLength);

        try {
            RtlCopyMemory(DestString->Buffer,
                          SourceString->Buffer,
                          SourceString->Length);
        } except(EXCEPTION_EXECUTE_HANDLER) {
            ExFreePoolWithTag(DestString->Buffer, PoolTag);
            DestString->Buffer = NULL;

            ntStatus = GetExceptionCode();
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "[Milou] Failed capturing unicode buffer, exception %#X\n",
                       ntStatus);
        }
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "[Milou] Failed capturing unicode buffer, insufficient resources!\n");
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    if (DestString->Buffer == NULL) {
        DestString->Length = 0;
        DestString->MaximumLength = 0;
    }

    return ntStatus;

}

VOID
FreeCapturedUnicodeString(
    _In_    PUNICODE_STRING String,
    _In_    ULONG           PoolTag
)
{
    if (String->Length != 0) {
        String->Length = 0;
        String->MaximumLength = 0;
        FreeCapturedBuffer(String->Buffer, PoolTag);
        String->Buffer = NULL;
    }
}
