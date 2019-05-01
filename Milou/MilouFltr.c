#include "MilouFltr.h"


// Currently there is no synchronization object used for this flags
// as the probability that the callback will be registered and unregistered
// at the same time is minimal?
BOOLEAN g_IsRegistryCallbackActive;
BOOLEAN g_IsProcessCallbackActive;
BOOLEAN g_IsThreadCallbackActive;

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
                       MILOU_LOG_P __FUNCTION__ "Failed to allocate memory\n");

            retStatus = FALSE;
        } else {
            RtlSecureZeroMemory(pwKeyName, ObjectName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwKeyName, ObjectName->Buffer, ObjectName->Length);

            EventWriteMilouRegPreDeleteKeyEvent(NULL,
                                                pwKeyName,
                                                (SIZE_T)PsGetCurrentProcessId(),
                                                (SIZE_T)PsGetCurrentThreadId());

            ExFreePoolWithTag(pwKeyName, MILOU_REG_CB_TAG);
            pwKeyName = NULL;
        }

        CmCallbackReleaseKeyObjectIDEx(ObjectName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed obtaining key name\n");

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
                       MILOU_LOG_P __FUNCTION__ "Failed to allocate memory\n");

            retStatus = FALSE;
        } else {
            pwValueName = (PWSTR)ExAllocatePoolWithTag(PagedPool,
                                                       pRegSetValueInfo->ValueName->Length + sizeof(WCHAR),
                                                       MILOU_REG_CB_TAG);
            if (NULL == pwValueName) {
                DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                           DPFLTR_ERROR_LEVEL,
                           MILOU_LOG_P __FUNCTION__ "Failed to allocate memory(2)\n");
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

                        break;
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
                                                                   L"Value wasn't logged, check Type parameter",
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
                   MILOU_LOG_P __FUNCTION__ "Failed obtaining key name\n");

        retStatus = FALSE;
    }

    return retStatus;
}

_Success_(return == TRUE)
BOOLEAN
MilouRegNtPreDeleteValueKey(
    _In_    PVOID   CallbackContext,
    _In_    PVOID   Argument2
)
{
    BOOLEAN                             retStatus = TRUE;
    NTSTATUS                            ntStatus;
    PWSTR                               pwKeyName = NULL;
    ULONG_PTR                           ObjectId = 0;
    UNICODE_STRING                      ValueName;
    PCUNICODE_STRING                    ObjectName = NULL;
    PREG_DELETE_VALUE_KEY_INFORMATION   pRegDeleteValueKeyInfo = NULL;
    PMILOU_CALLBACK_CONTEXT             pMilouCallbackCtx = NULL;

    PAGED_CODE();

    RtlInitEmptyUnicodeString(&ValueName, NULL, 0);
    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)CallbackContext;
    pRegDeleteValueKeyInfo = (PREG_DELETE_VALUE_KEY_INFORMATION)Argument2;
    if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&pMilouCallbackCtx->Cookie,
                                              pRegDeleteValueKeyInfo->Object,
                                              &ObjectId,
                                              &ObjectName,
                                              0))) {
        pwKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, ObjectName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (NULL == pwKeyName) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to allocate memory\n");

            retStatus = FALSE;
        } else {
            RtlSecureZeroMemory(pwKeyName, ObjectName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwKeyName, ObjectName->Buffer, ObjectName->Length);

            if (!g_IsWindows8OrGreater && (ExGetPreviousMode() == UserMode)) {
                ntStatus = CaptureUnicodeString(&ValueName, pRegDeleteValueKeyInfo->ValueName, MILOU_REG_CB_TAG);
                if (NT_SUCCESS(ntStatus) && (ValueName.Buffer != NULL)) {
                    EventWriteMilouRegPreDeleteValueKeyEvent(NULL,
                                                             pwKeyName,
                                                             ValueName.Buffer,
                                                             (SIZE_T)PsGetCurrentProcessId(),
                                                             (SIZE_T)PsGetCurrentThreadId());

                    FreeCapturedUnicodeString(&ValueName, MILOU_REG_CB_TAG);
                    ValueName.Buffer = NULL;
                } else {
                    EventWriteMilouRegPreDeleteValueKeyEvent(NULL,
                                                             pwKeyName,
                                                             L"Failed to capture data buffer",
                                                             (SIZE_T)PsGetCurrentProcessId(),
                                                             (SIZE_T)PsGetCurrentThreadId());
                }
            } else {
                EventWriteMilouRegPreDeleteValueKeyEvent(NULL,
                                                         pwKeyName,
                                                         pRegDeleteValueKeyInfo->ValueName->Buffer,
                                                         (SIZE_T)PsGetCurrentProcessId(),
                                                         (SIZE_T)PsGetCurrentThreadId());
            }

            ExFreePoolWithTag(pwKeyName, MILOU_REG_CB_TAG);
            pwKeyName = NULL;
        }

        CmCallbackReleaseKeyObjectIDEx(ObjectName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed obtaining key name\n");

        retStatus = FALSE;
    }

    return retStatus;
}

_Success_(return == TRUE)
BOOLEAN
MilouRegNtPreSetInformationKey(
    _In_    PVOID   CallbackContext,
    _In_    PVOID   Argument2
)
{
    NTSTATUS                                ntStatus;
    BOOLEAN                                 retStatus = TRUE;
    PWSTR                                   pwKeyName = NULL;
    PVOID                                   pRegData = NULL;
    ULONG_PTR                               ObjectId = 0;
    PCUNICODE_STRING                        ObjectName = NULL;
    PREG_SET_INFORMATION_KEY_INFORMATION    pRegSetInfo = NULL;
    PMILOU_CALLBACK_CONTEXT                 pMilouCallbackCtx = NULL;

    PAGED_CODE();

    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)CallbackContext;
    pRegSetInfo = (PREG_SET_INFORMATION_KEY_INFORMATION)Argument2;

    if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&pMilouCallbackCtx->Cookie,
                                              pRegSetInfo->Object,
                                              &ObjectId,
                                              &ObjectName,
                                              0))) {
        pwKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, ObjectName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (NULL == pwKeyName) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to allocate memory\n");

            retStatus = FALSE;
        } else {
            RtlSecureZeroMemory(pwKeyName, ObjectName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwKeyName, ObjectName->Buffer, ObjectName->Length);

            switch (pRegSetInfo->KeySetInformationClass) {
            case KeyWriteTimeInformation:
                if (!g_IsWindows8OrGreater && (ExGetPreviousMode() == UserMode)) {
                    ntStatus = CaptureBuffer(&pRegData,
                                             pRegSetInfo->KeySetInformation,
                                             pRegSetInfo->KeySetInformationLength,
                                             MILOU_REG_CB_TAG);
                    if (NT_SUCCESS(ntStatus) && (pRegData != NULL)) {
                        EventWriteMilouRegPreSetInformationKeyQwordEvent(NULL,
                                                                         pwKeyName,
                                                                         ((PKEY_WRITE_TIME_INFORMATION)pRegData)->LastWriteTime.QuadPart,
                                                                         pRegSetInfo->KeySetInformationClass,
                                                                         (SIZE_T)PsGetCurrentProcessId(),
                                                                         (SIZE_T)PsGetCurrentThreadId());

                        FreeCapturedBuffer(pRegData, MILOU_REG_CB_TAG);
                        pRegData = NULL;
                    } else {
                        EventWriteMilouRegPreSetInformationKeyUcStrEvent(NULL,
                                                                         pwKeyName,
                                                                         L"Failed to capture data buffer",
                                                                         pRegSetInfo->KeySetInformationClass,
                                                                         (SIZE_T)PsGetCurrentProcessId(),
                                                                         (SIZE_T)PsGetCurrentThreadId());
                    }
                } else {
                    EventWriteMilouRegPreSetInformationKeyQwordEvent(NULL,
                                                                     pwKeyName,
                                                                     ((PKEY_WRITE_TIME_INFORMATION)pRegSetInfo->KeySetInformation)->LastWriteTime.QuadPart,
                                                                     pRegSetInfo->KeySetInformationClass,
                                                                     (SIZE_T)PsGetCurrentProcessId(),
                                                                     (SIZE_T)PsGetCurrentThreadId());
                }

                break;
            default:
                //
                // Don't log anything, other classes are reserved, we don't care... at least by 25/04/2019
                //

                break;
            }

            ExFreePoolWithTag(pwKeyName, MILOU_REG_CB_TAG);
            pwKeyName = NULL;
        }

        CmCallbackReleaseKeyObjectIDEx(ObjectName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed obtaining key name\n");

        retStatus = FALSE;
    }

    return retStatus;
}

_Success_(return == TRUE)
BOOLEAN
MilouRegNtPreRenameKey(
    _In_    PVOID   CallbackContext,
    _In_    PVOID   Argument2
)
{
    BOOLEAN                     retStatus = TRUE;
    NTSTATUS                    ntStatus;
    PWSTR                       pwKeyName = NULL;
    PWSTR                       pwNewKeyName = NULL;
    ULONG_PTR                   ObjectId = 0;
    UNICODE_STRING              ValueName;
    PCUNICODE_STRING            ObjectName = NULL;
    PREG_RENAME_KEY_INFORMATION pRegRenameKeyInfo = NULL;
    PMILOU_CALLBACK_CONTEXT     pMilouCallbackCtx = NULL;

    PAGED_CODE();

    RtlInitEmptyUnicodeString(&ValueName, NULL, 0);
    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)CallbackContext;
    pRegRenameKeyInfo = (PREG_RENAME_KEY_INFORMATION)Argument2;
    if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&pMilouCallbackCtx->Cookie,
                                              pRegRenameKeyInfo->Object,
                                              &ObjectId,
                                              &ObjectName,
                                              0))) {
        pwKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, ObjectName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (NULL == pwKeyName) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to allocate memory\n");

            retStatus = FALSE;
        } else {
            RtlSecureZeroMemory(pwKeyName, ObjectName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwKeyName, ObjectName->Buffer, ObjectName->Length);

            if (!g_IsWindows8OrGreater && (ExGetPreviousMode() == UserMode)) {
                ntStatus = CaptureUnicodeString(&ValueName, pRegRenameKeyInfo->NewName, MILOU_REG_CB_TAG);
                if (NT_SUCCESS(ntStatus) && (ValueName.Buffer != NULL)) {
                    EventWriteMilouRegPreRenameKeyEvent(NULL,
                                                        pwKeyName,
                                                        ValueName.Buffer,
                                                        (SIZE_T)PsGetCurrentProcessId(),
                                                        (SIZE_T)PsGetCurrentThreadId());

                    FreeCapturedUnicodeString(&ValueName, MILOU_REG_CB_TAG);
                    ValueName.Buffer = NULL;
                } else {
                    EventWriteMilouRegPreRenameKeyEvent(NULL,
                                                        pwKeyName,
                                                        L"Failed to capture data buffer",
                                                        (SIZE_T)PsGetCurrentProcessId(),
                                                        (SIZE_T)PsGetCurrentThreadId());
                }
            } else {
                //
                // we need to add end character to unicode string otherwise formatting in log we'll be messed up
                // if we failed to add end char, log an error message, but the previous key will still be in the log.
                //
                pwNewKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, pRegRenameKeyInfo->NewName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
                if (pwNewKeyName != NULL) {
                    RtlSecureZeroMemory(pwNewKeyName, pRegRenameKeyInfo->NewName->Length + sizeof(WCHAR));
                    RtlCopyMemory(pwNewKeyName, pRegRenameKeyInfo->NewName->Buffer, pRegRenameKeyInfo->NewName->Length);

                    EventWriteMilouRegPreRenameKeyEvent(NULL,
                                                        pwKeyName,
                                                        pwNewKeyName,
                                                        (SIZE_T)PsGetCurrentProcessId(),
                                                        (SIZE_T)PsGetCurrentThreadId());

                    ExFreePoolWithTag(pwNewKeyName, MILOU_REG_CB_TAG);
                    pwNewKeyName = NULL;
                } else {
                    EventWriteMilouRegPreRenameKeyEvent(NULL,
                                                        pwKeyName,
                                                        L"Failed to allocate memory",
                                                        (SIZE_T)PsGetCurrentProcessId(),
                                                        (SIZE_T)PsGetCurrentThreadId());
                }
            }

            ExFreePoolWithTag(pwKeyName, MILOU_REG_CB_TAG);
            pwKeyName = NULL;
        }

        CmCallbackReleaseKeyObjectIDEx(ObjectName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed obtaining key name\n");

        retStatus = FALSE;
    }

    return retStatus;
}

_Success_(return == TRUE)
BOOLEAN
MilouRegNtPreCreateKey(
    _In_    PVOID   CallbackContext,
    _In_    PVOID   Argument2
)
{
    BOOLEAN                         retStatus = TRUE;
    NTSTATUS                        ntStatus;
    PWSTR                           pwNewKeyName = NULL;
    UNICODE_STRING                  completeName;
    PREG_PRE_CREATE_KEY_INFORMATION pRegCreateKeyInfo = NULL;
    PMILOU_CALLBACK_CONTEXT         pMilouCallbackCtx = NULL;

    PAGED_CODE();

    RtlInitEmptyUnicodeString(&completeName, NULL, 0);
    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)CallbackContext;
    pRegCreateKeyInfo = (PREG_PRE_CREATE_KEY_INFORMATION)Argument2;

    if (!g_IsWindows8OrGreater && (ExGetPreviousMode() == UserMode)) {
        ntStatus = CaptureUnicodeString(&completeName, pRegCreateKeyInfo->CompleteName, MILOU_REG_CB_TAG);
        if (NT_SUCCESS(ntStatus) && (completeName.Buffer != NULL)) {
            EventWriteMilouRegPreCreateKeyEvent(NULL,
                                                completeName.Buffer,
                                                (SIZE_T)PsGetCurrentProcessId(),
                                                (SIZE_T)PsGetCurrentThreadId());

            FreeCapturedUnicodeString(&completeName, MILOU_REG_CB_TAG);
            completeName.Buffer = NULL;
        } else {
            EventWriteMilouRegPreCreateKeyEvent(NULL,
                                                L"Failed to capture data buffer",
                                                (SIZE_T)PsGetCurrentProcessId(),
                                                (SIZE_T)PsGetCurrentThreadId());
        }
    } else {
        //
        // we need to add end character to unicode string otherwise formatting in log we'll be messed up
        // if we failed to add end char, log an error message...
        //
        pwNewKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, pRegCreateKeyInfo->CompleteName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (pwNewKeyName != NULL) {
            RtlSecureZeroMemory(pwNewKeyName, pRegCreateKeyInfo->CompleteName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwNewKeyName, pRegCreateKeyInfo->CompleteName->Buffer, pRegCreateKeyInfo->CompleteName->Length);

            EventWriteMilouRegPreCreateKeyEvent(NULL,
                                                pwNewKeyName,
                                                (SIZE_T)PsGetCurrentProcessId(),
                                                (SIZE_T)PsGetCurrentThreadId());

            ExFreePoolWithTag(pwNewKeyName, MILOU_REG_CB_TAG);
            pwNewKeyName = NULL;
        } else {
            EventWriteMilouRegPreCreateKeyEvent(NULL,
                                                L"Failed to allocate memory",
                                                (SIZE_T)PsGetCurrentProcessId(),
                                                (SIZE_T)PsGetCurrentThreadId());
        }
    }

    return retStatus;
}

_Success_(return == TRUE)
BOOLEAN
MilouRegNtPreCreateKeyEx(
    _In_    PVOID   CallbackContext,
    _In_    PVOID   Argument2
)
{
    BOOLEAN                         retStatus = TRUE;
    NTSTATUS                        ntStatus;
    PWSTR                           pwNewKeyName = NULL;
    UNICODE_STRING                  completeName;
    PWSTR                           pwRootKeyName = NULL;
    ULONG_PTR                       ObjectId = 0;
    PCUNICODE_STRING                ObjectName = NULL;
    //
    // This structure is available only from Windows 7, I really hope there's no living creature running this on an earlier version
    //
    PREG_CREATE_KEY_INFORMATION_V1  pRegCreateKeyExInfo = NULL;
    PMILOU_CALLBACK_CONTEXT         pMilouCallbackCtx = NULL;

    PAGED_CODE();

    RtlInitEmptyUnicodeString(&completeName, NULL, 0);
    pMilouCallbackCtx = (PMILOU_CALLBACK_CONTEXT)CallbackContext;
    pRegCreateKeyExInfo = (PREG_CREATE_KEY_INFORMATION_V1)Argument2;

    if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&pMilouCallbackCtx->Cookie,
                                              pRegCreateKeyExInfo->RootObject,
                                              &ObjectId,
                                              &ObjectName,
                                              0))) {
        pwRootKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, ObjectName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (pwRootKeyName != NULL) {
            RtlSecureZeroMemory(pwRootKeyName, ObjectName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwRootKeyName, ObjectName->Buffer, ObjectName->Length);
        }

        CmCallbackReleaseKeyObjectIDEx(ObjectName);
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed obtaining key name\n");
    }

    if (!g_IsWindows8OrGreater && (ExGetPreviousMode() == UserMode)) {
        ntStatus = CaptureUnicodeString(&completeName, pRegCreateKeyExInfo->CompleteName, MILOU_REG_CB_TAG);
        if (NT_SUCCESS(ntStatus) && (completeName.Buffer != NULL)) {
            EventWriteMilouRegPreCreateKeyExEvent(NULL,
                                                  completeName.Buffer,
                                                  pwRootKeyName != NULL ? pwRootKeyName : L"",
                                                  (SIZE_T)PsGetCurrentProcessId(),
                                                  (SIZE_T)PsGetCurrentThreadId());

            FreeCapturedUnicodeString(&completeName, MILOU_REG_CB_TAG);
            completeName.Buffer = NULL;
        } else {
            EventWriteMilouRegPreCreateKeyExEvent(NULL,
                                                  L"Failed to capture data buffer",
                                                  pwRootKeyName != NULL ? pwRootKeyName : L"",
                                                  (SIZE_T)PsGetCurrentProcessId(),
                                                  (SIZE_T)PsGetCurrentThreadId());
        }
    } else {
        //
        // we need to add end character to unicode string otherwise formatting in log we'll be messed up
        // if we failed to add end char, log an error message...
        //
        pwNewKeyName = (PWSTR)ExAllocatePoolWithTag(PagedPool, pRegCreateKeyExInfo->CompleteName->Length + sizeof(WCHAR), MILOU_REG_CB_TAG);
        if (pwNewKeyName != NULL) {
            RtlSecureZeroMemory(pwNewKeyName, pRegCreateKeyExInfo->CompleteName->Length + sizeof(WCHAR));
            RtlCopyMemory(pwNewKeyName, pRegCreateKeyExInfo->CompleteName->Buffer, pRegCreateKeyExInfo->CompleteName->Length);

            EventWriteMilouRegPreCreateKeyExEvent(NULL,
                                                  pwNewKeyName,
                                                  pwRootKeyName != NULL ? pwRootKeyName : L"",
                                                  (SIZE_T)PsGetCurrentProcessId(),
                                                  (SIZE_T)PsGetCurrentThreadId());

            ExFreePoolWithTag(pwNewKeyName, MILOU_REG_CB_TAG);
            pwNewKeyName = NULL;
        } else {
            EventWriteMilouRegPreCreateKeyExEvent(NULL,
                                                  L"Failed to allocate memory",
                                                  pwRootKeyName != NULL ? pwRootKeyName : L"",
                                                  (SIZE_T)PsGetCurrentProcessId(),
                                                  (SIZE_T)PsGetCurrentThreadId());
        }
    }

    if (pwNewKeyName != NULL) {
        ExFreePoolWithTag(pwNewKeyName, MILOU_REG_CB_TAG);
        pwNewKeyName = NULL;
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
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreSetValueKey");
        }

        break;
    case RegNtPreDeleteValueKey:
        if (!MilouRegNtPreDeleteValueKey(CallbackContext, Argument2)) {
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreDeleteValueKey");
        }

        break;
    case RegNtPreSetInformationKey:
        if (!MilouRegNtPreSetInformationKey(CallbackContext, Argument2)) {
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreSetInformationKey");
        }

        break;
    case RegNtPreRenameKey:
        if (!MilouRegNtPreRenameKey(CallbackContext, Argument2)) {
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreRenameKey");
        }

        break;
    case RegNtPreCreateKey:
        if (!MilouRegNtPreCreateKey(CallbackContext, Argument2)) {
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreCreateKey");
        }

        break;
    case RegNtPreCreateKeyEx:
        if (!MilouRegNtPreCreateKeyEx(CallbackContext, Argument2)) {
            EventWriteMilouEvent(NULL, L"[Milou-error] RegNtPreCreateKeyEx");
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
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed to create callback context\n");

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
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to insert callback context\n");

            ntStatus = CmUnRegisterCallback(pMilouCallbackContext->Cookie);
            if (!NT_SUCCESS(ntStatus)) {
                DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                           DPFLTR_ERROR_LEVEL,
                           MILOU_LOG_P __FUNCTION__ "Failed to unregister callback: %#X\n",
                           ntStatus);
            }
             
            retStatus = FALSE;

            goto cleanup;
        } else {
            g_IsRegistryCallbackActive = TRUE;
        }
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed to register callback: %#X\n",
                   ntStatus);

    cleanup:
        DeleteCallbackContext(pMilouCallbackContext);
        pMilouCallbackContext = NULL;

        retStatus = FALSE;
    }

    return retStatus;
}

//
// Currently all registered registry callbacks will be unregistered by this function
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

    if (!g_IsRegistryCallbackActive) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Registry callback is not active. Nothing to unregister\n");

        return retStatus;
    }

    pListEntry = g_CallbackCtxListHead.Flink;
    while (pListEntry != &g_CallbackCtxListHead) {
        pMilouCallbackCtx = CONTAINING_RECORD(pListEntry,
                                              MILOU_CALLBACK_CONTEXT,
                                              CallbackCtxList);
        ntStatus = CmUnRegisterCallback(pMilouCallbackCtx->Cookie);
        if (!NT_SUCCESS(ntStatus)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to unregister callback: %#X\n",
                       ntStatus);

            retStatus = FALSE;

            break;
        } else {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_INFO_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Register callback unregistered\n");

            g_IsRegistryCallbackActive = FALSE;

            pMilouCallbackCtx = FindAndRemoveCallbackContext(pMilouCallbackCtx->Cookie);
            if (NULL != pMilouCallbackCtx) {
                DeleteCallbackContext(pMilouCallbackCtx);
                pMilouCallbackCtx = NULL;

                DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                           DPFLTR_INFO_LEVEL,
                           MILOU_LOG_P __FUNCTION__ "Callback context deleted\n");
            } else {
                DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                           DPFLTR_ERROR_LEVEL,
                           MILOU_LOG_P __FUNCTION__ "Failed to find and delete context\n");
            }
        }         

        if (IsListEmpty(&g_CallbackCtxListHead)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_INFO_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Empty list.\n");

            break;
        }

        pListEntry = pListEntry->Flink;
    }

    return retStatus;
}

VOID
MilouProcessCallback(
    PEPROCESS               Process,
    HANDLE                  ProcessId,
    PPS_CREATE_NOTIFY_INFO  CreateInfo
)
{
    UNREFERENCED_PARAMETER(Process);

    PWSTR   pImageFileName = NULL;
    PWSTR   pCommandLine = NULL;

    PAGED_CODE();

    //
    // Process is exiting, see https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/content/ntddk/nc-ntddk-pcreate_process_notify_routine_ex
    //
    if (NULL == CreateInfo) {
        EventWriteMilouProcExitEvent(NULL, (SIZE_T)ProcessId);

        return;
    }
    //
    // In case allocations fail, and CreateInfo->ImageFileName and/or CreateInfo->CommandLine are
    // not NULL, the message logged to ETW will not represent the truth, but logging UNICODE_STRING
    // that probably isn't NULL terminated isn't going to do much better.
    //
    if (CreateInfo->ImageFileName != NULL) {
        pImageFileName = (PWSTR)ExAllocatePoolWithTag(PagedPool,
                                                      CreateInfo->ImageFileName->Length + sizeof(WCHAR),
                                                      MILOU_PROC_CB_TAG);
        if (pImageFileName != NULL) {
            RtlSecureZeroMemory(pImageFileName, CreateInfo->ImageFileName->Length + sizeof(WCHAR));
            RtlCopyMemory(pImageFileName, CreateInfo->ImageFileName->Buffer, CreateInfo->ImageFileName->Length);
        } else {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to allocate memory\n");
        }
    }
    if (CreateInfo->CommandLine != NULL) {
        pCommandLine = (PWSTR)ExAllocatePoolWithTag(PagedPool,
                                                    CreateInfo->CommandLine->Length + sizeof(WCHAR),
                                                    MILOU_PROC_CB_TAG);
        if (pCommandLine != NULL) {
            RtlSecureZeroMemory(pCommandLine, CreateInfo->CommandLine->Length + sizeof(WCHAR));
            RtlCopyMemory(pCommandLine, CreateInfo->CommandLine->Buffer, CreateInfo->CommandLine->Length);
        } else {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to allocate memory\n");
        }
    }

    EventWriteMilouProcCreateEvent(NULL,
                                   (SIZE_T)ProcessId,
                                   (SIZE_T)CreateInfo->ParentProcessId,
                                   (SIZE_T)CreateInfo->CreatingThreadId.UniqueProcess,
                                   (SIZE_T)CreateInfo->CreatingThreadId.UniqueThread,
                                   CreateInfo->IsSubsystemProcess ? L"Not Win32" : L"Win32",
                                   pImageFileName != NULL ? pImageFileName : L"Not available",
                                   pCommandLine != NULL ? pCommandLine : L"Not available");

    if (pImageFileName != NULL) {
        ExFreePoolWithTag(pImageFileName, MILOU_PROC_CB_TAG);
        pImageFileName = NULL;
    }
    if (pCommandLine != NULL) {
        ExFreePoolWithTag(pCommandLine, MILOU_PROC_CB_TAG);
        pCommandLine = NULL;
    }
}

_Use_decl_annotations_
BOOLEAN
RegisterProcessCallback(
    _In_    BOOLEAN Remove
)
{
    NTSTATUS ntStatus;

    ntStatus = PsSetCreateProcessNotifyRoutineEx((PCREATE_PROCESS_NOTIFY_ROUTINE_EX)MilouProcessCallback, Remove);
    if (!NT_SUCCESS(ntStatus)) {
        if (!Remove) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to register process callback: %#X\n",
                       ntStatus);
        } else {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                       DPFLTR_ERROR_LEVEL,
                       MILOU_LOG_P __FUNCTION__ "Failed to unregister process callback: %#X\n",
                       ntStatus);
        }

        return FALSE;
    } else {
        if (!Remove) {
            g_IsProcessCallbackActive = TRUE;
        } else {
            g_IsProcessCallbackActive = FALSE;
        }

        return TRUE;
    }
}

_Use_decl_annotations_
BOOLEAN
UnregisterProcessCallback(
    VOID
)
{
    if (!g_IsProcessCallbackActive) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Process callback is not active. Nothing to unregister\n");

        return TRUE;
    }
    return RegisterProcessCallback(TRUE);
}

VOID
MilouThreadCallback(
    HANDLE  ProcessId,
    HANDLE  ThreadId,
    BOOLEAN Create
)
{
    PAGED_CODE();

    EventWriteMilouThreadEvent(NULL,
                               (SIZE_T)ProcessId,
                               (SIZE_T)ThreadId,
                               Create);
}

_Use_decl_annotations_
BOOLEAN
RegisterThreadCallback(
    VOID
)
{
    NTSTATUS ntStatus = PsSetCreateThreadNotifyRoutine((PCREATE_THREAD_NOTIFY_ROUTINE)MilouThreadCallback);
    if (NT_SUCCESS(ntStatus)) {
        g_IsThreadCallbackActive = TRUE;

        return TRUE;
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed to register thread callback: %#X\n",
                   ntStatus);

        return FALSE;
    }
}

_Use_decl_annotations_
BOOLEAN
UnregisterThreadCallback(
    VOID
)
{
    if (!g_IsThreadCallbackActive) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_INFO_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Thread callback is not active. Nothing to unregister\n");

        return TRUE;
    }

    NTSTATUS ntStatus = PsRemoveCreateThreadNotifyRoutine((PCREATE_THREAD_NOTIFY_ROUTINE)MilouThreadCallback);
    if (NT_SUCCESS(ntStatus)) {
        g_IsThreadCallbackActive = FALSE;

        return TRUE;
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID,
                   DPFLTR_ERROR_LEVEL,
                   MILOU_LOG_P __FUNCTION__ "Failed to unregister thread callback: %#X\n",
                   ntStatus);

        return FALSE;
    }
}
