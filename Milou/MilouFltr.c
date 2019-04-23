#include "MilouFltr.h"

// Driver callbacks
EX_CALLBACK_FUNCTION    MilouRegistryCallback;

_Success_(return == TRUE)
BOOLEAN
MilouRegNtPreDeleteKey(
    _In_    PVOID   CallbackContext,
    _In_    PVOID   Argument2
)
{
    BOOLEAN                     retStatus = TRUE;
    PWSTR                       pwKeyName = NULL;
    ULONG_PTR                   ObjectId = 0;
    PCUNICODE_STRING            ObjectName = NULL;
    PREG_DELETE_KEY_INFORMATION pRegDeleteKeyInfo = NULL;
    PMILOU_CALLBACK_CONTEXT     pMilouCallbackCtx = NULL;

    PAGED_CODE();

    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)CallbackContext;
    pRegDeleteKeyInfo = (PREG_DELETE_KEY_INFORMATION)Argument2;

    if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&pMilouCallbackCtx->Cookie,
                                              pRegDeleteKeyInfo->Object,
                                              &ObjectId,
                                              &ObjectName,
                                              0))) {
        pwKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, ObjectName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (NULL == pwKeyName) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "[Milou][RegNtPreDeleteKey] Failed to allocate memory\n");

            retStatus = FALSE;
        } else {
            RtlSecureZeroMemory(pwKeyName, ObjectName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwKeyName, ObjectName->Buffer, ObjectName->Length);

            EventWriteMilouRegPreDeleteKeyEvent(NULL,
                                                pwKeyName,
                                                (SIZE_T)PsGetCurrentProcessId(),
                                                (SIZE_T)PsGetCurrentThreadId());

            ExFreePoolWithTag(pwKeyName, MILOU_REG_CB_TAG);
        }

        CmCallbackReleaseKeyObjectIDEx(ObjectName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "[Milou][RegNtPreDeleteKey] Failed obtaining key name\n");

        retStatus = FALSE;
    }

    return retStatus;
}

_Success_(return == TRUE)
BOOLEAN
MilouRegNtPreSetValueKey(
    _In_    PVOID   CallbackContext,
    _In_    PVOID   Argument2
)
{
    NTSTATUS                        ntStatus;
    BOOLEAN                         retStatus = TRUE;
    BOOLEAN                         isCapturedData = FALSE;
    PWSTR                           pwKeyName = NULL;
    PWSTR                           pwValueName = NULL;
    PVOID                           pRegData = NULL;
    ULONG_PTR                       ObjectId = 0;
    PCUNICODE_STRING                ObjectName = NULL;
    PREG_SET_VALUE_KEY_INFORMATION  pRegSetValueInfo = NULL;
    PMILOU_CALLBACK_CONTEXT         pMilouCallbackCtx = NULL;

    PAGED_CODE();

    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)CallbackContext;
    pRegSetValueInfo = (PREG_SET_VALUE_KEY_INFORMATION)Argument2;

    if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&pMilouCallbackCtx->Cookie,
                                              pRegSetValueInfo->Object,
                                              &ObjectId,
                                              &ObjectName,
                                              0))) {
        pwKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, ObjectName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (NULL == pwKeyName) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       "[Milou][RegNtPreSetValueKey] Failed to allocate memory\n");

            retStatus = FALSE;
        } else {
            pwValueName = (PWSTR)ExAllocatePoolWithTag(PagedPool,
                                                       pRegSetValueInfo->ValueName->Length + sizeof(WCHAR),
                                                       MILOU_REG_CB_TAG);
            if (NULL == pwValueName) {
                DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                           DPFLTR_ERROR_LEVEL,
                           "[Milou][RegNtPreSetValueKey] Failed to allocate memory(2)\n");
            } else {
                RtlSecureZeroMemory(pwKeyName, ObjectName->Length + sizeof(WCHAR));
                RtlCopyMemory(pwKeyName, ObjectName->Buffer, ObjectName->Length);
                RtlSecureZeroMemory(pwValueName, pRegSetValueInfo->ValueName->Length + sizeof(WCHAR));
                RtlCopyMemory(pwValueName, pRegSetValueInfo->ValueName->Buffer, pRegSetValueInfo->ValueName->Length);

                //
                // To understand why we need to capture the buffer, read remarks sections:
                // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/wdm/nc-wdm-ex_callback_function
                //
                if (!g_IsWindows8OrGreater && (ExGetPreviousMode() == UserMode)) {
                    ntStatus = CaptureBuffer(&pRegData,
                                             pRegSetValueInfo->Data,
                                             pRegSetValueInfo->DataSize,
                                             MILOU_REG_CB_TAG);
                    if (NT_SUCCESS(ntStatus) && (pRegData != NULL)) {
                        isCapturedData = TRUE;
                    }
                } else {
                        pRegData = pRegSetValueInfo->Data;
                }
                //
                // In case it was needed to capture the buffer and capturing failed or length was 0
                // use default value
                //
                if (NULL == pRegData) {
                    EventWriteMilouRegPreSetValueKeyUcStrEvent(NULL,
                                                               pwKeyName,
                                                               pwValueName,
                                                               L"Failed to capture data buffer",
                                                               pRegSetValueInfo->Type,
                                                               (SIZE_T)PsGetCurrentProcessId(),
                                                               (SIZE_T)PsGetCurrentThreadId());
                } else {
                    switch (pRegSetValueInfo->Type) {
                    case REG_NONE:
                    case REG_BINARY:
                        EventWriteMilouRegPreSetValueKeyBinEvent(NULL,
                                                                 pwKeyName,
                                                                 pwValueName,
                                                                 pRegData,
                                                                 pRegSetValueInfo->Type,
                                                                 (SIZE_T)PsGetCurrentProcessId(),
                                                                 (SIZE_T)PsGetCurrentThreadId());
                        break;
                    case REG_DWORD_BIG_ENDIAN:
                    case REG_DWORD_LITTLE_ENDIAN:
                        EventWriteMilouRegPreSetValueKeyDwordEvent(NULL,
                                                                   pwKeyName,
                                                                   pwValueName,
                                                                   *(PDWORD32)pRegData,
                                                                   pRegSetValueInfo->Type,
                                                                   (SIZE_T)PsGetCurrentProcessId(),
                                                                   (SIZE_T)PsGetCurrentThreadId());
                        break;
                    case REG_QWORD:
                        EventWriteMilouRegPreSetValueKeyQwordEvent(NULL,
                                                                   pwKeyName,
                                                                   pwValueName,
                                                                   *(PDWORD64)pRegData,
                                                                   pRegSetValueInfo->Type,
                                                                   (SIZE_T)PsGetCurrentProcessId(),
                                                                   (SIZE_T)PsGetCurrentThreadId());
                    case REG_SZ:
                    case REG_EXPAND_SZ:
                        EventWriteMilouRegPreSetValueKeyUcStrEvent(NULL,
                                                                   pwKeyName,
                                                                   pwValueName,
                                                                   pRegData,
                                                                   pRegSetValueInfo->Type,
                                                                   (SIZE_T)PsGetCurrentProcessId(),
                                                                   (SIZE_T)PsGetCurrentThreadId());
                        break;
                    default:
                        EventWriteMilouRegPreSetValueKeyUcStrEvent(NULL,
                                                                   pwKeyName,
                                                                   pwValueName,
                                                                   L"Value isn't logged, check Type parameter",
                                                                   pRegSetValueInfo->Type,
                                                                   (SIZE_T)PsGetCurrentProcessId(),
                                                                   (SIZE_T)PsGetCurrentThreadId());
                        break;
                    }
                }

                if (isCapturedData) {
                    FreeCapturedBuffer(pRegData, MILOU_REG_CB_TAG);
                    pRegData = NULL;
                }

                ExFreePoolWithTag(pwValueName, MILOU_REG_CB_TAG);
                pwValueName = NULL;
            }

            ExFreePoolWithTag(pwKeyName, MILOU_REG_CB_TAG);
            pwKeyName = NULL;
        }
        CmCallbackReleaseKeyObjectIDEx(ObjectName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   "[Milou][RegNtPreSetValueKey] Failed obtaining key name\n");

        retStatus = FALSE;
    }

    return retStatus;
}

_Use_decl_annotations_
NTSTATUS
MilouRegistryCallback(
    PVOID   CallbackContext,
    PVOID   Argument1,
    PVOID   Argument2
)
{
    NT_ASSERT(Argument2 != NULL);
    // Argument1 is of type REG_NOTIFY_CLASS, using SIZE_T to shut up VS warning
    // didn't want to use suppress
    switch ((SIZE_T)Argument1) {
    case RegNtPreDeleteKey:
        if (!MilouRegNtPreDeleteKey(CallbackContext, Argument2)) {
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreDeleteKey");
        }

        break;
    case RegNtPreSetValueKey:
        if (!MilouRegNtPreSetValueKey(CallbackContext, Argument2)) {
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreDeleteKey");
        }

        break;
    default:
        break;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
BOOLEAN
RegisterRegistryCallback(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _In_    PCWSTR          AltitudeName
)
{
    NTSTATUS                ntStatus;
    BOOLEAN                 retStatus = TRUE;
    PMILOU_CALLBACK_CONTEXT pMilouCallbackContext = NULL;

    pMilouCallbackContext = CreateCallbackContext(AltitudeName);
    if (NULL == pMilouCallbackContext) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to create callback context\n");

        return FALSE;
    }

    ntStatus = CmRegisterCallbackEx(MilouRegistryCallback,
                                    &pMilouCallbackContext->Altitude,
                                    DeviceObject->DriverObject,
                                    (PVOID)pMilouCallbackContext,
                                    &pMilouCallbackContext->Cookie,
                                    NULL);
    if (NT_SUCCESS(ntStatus)) {
        if (!InsertCallbackContext(pMilouCallbackContext)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to insert callback context\n");

            ntStatus = CmUnRegisterCallback(pMilouCallbackContext->Cookie);
            if (!NT_SUCCESS(ntStatus)) {
                DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to unregister callback: %#X\n", ntStatus);
            }
             
            retStatus = FALSE;

            goto cleanup;
        }
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to register callback: %#X\n", ntStatus);

    cleanup:
        DeleteCallbackContext(pMilouCallbackContext);
        pMilouCallbackContext = NULL;

        retStatus = FALSE;
    }

    return retStatus;
}

//
// Currently all registered callbacks will be unregistered by this function
//
_Use_decl_annotations_
BOOLEAN
UnregisterRegistryCallback(
    VOID
)
{
    PMILOU_CALLBACK_CONTEXT pMilouCallbackCtx = NULL;
    PLIST_ENTRY             pListEntry = NULL;
    NTSTATUS                ntStatus;
    BOOLEAN                 retStatus = TRUE;

    pListEntry = g_CallbackCtxListHead.Flink;
    while (pListEntry != &g_CallbackCtxListHead) {
        pMilouCallbackCtx = CONTAINING_RECORD(pListEntry,
                                              MILOU_CALLBACK_CONTEXT,
                                              CallbackCtxList);
        ntStatus = CmUnRegisterCallback(pMilouCallbackCtx->Cookie);
        if (!NT_SUCCESS(ntStatus)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to unregister callback: %#X\n", ntStatus);

            retStatus = FALSE;

            break;
        } else {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou] Register callback unregistered\n");

            pMilouCallbackCtx = FindAndRemoveCallbackContext(pMilouCallbackCtx->Cookie);
            if (NULL != pMilouCallbackCtx) {
                DeleteCallbackContext(pMilouCallbackCtx);
                pMilouCallbackCtx = NULL;

                DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[Milou] Callback context deleted\n");
            } else {
                DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[Milou] Failed to find and delete context\n");
            }
        }         

        pListEntry = pListEntry->Flink;
    }

    return retStatus;
}
