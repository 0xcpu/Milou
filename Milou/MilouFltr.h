#pragma once

#include <ntddk.h>

#include "MilouEtw.h"
#include "MilouUtils.h"


#define MILOU_CTX_TAG           'xtCM'
#define MILOU_REG_CB_TAG        'bcRM'
#define MILOU_CAPTURE_BUFF_LEN  32 // in bytes

extern  FAST_MUTEX  g_CallbackCtxListMutex;
extern  LIST_ENTRY  g_CallbackCtxListHead;
extern  SIZE_T      g_CallbackCtxListNumEntries;
extern  BOOLEAN     g_IsWindows8OrGreater;
extern  BOOLEAN     g_IsRegistryCallbackActive;

typedef struct _MILOU_CALLBACK_CONTEXT {
    LIST_ENTRY      CallbackCtxList;
    UNICODE_STRING  Altitude;
    LARGE_INTEGER   Cookie;
} MILOU_CALLBACK_CONTEXT, *PMILOU_CALLBACK_CONTEXT;

_Success_(return == TRUE)
BOOLEAN
RegisterRegistryCallback(
    _In_    PDEVICE_OBJECT  DeviceObject,
    _In_    PCWSTR          AltitudeName
);

_Success_(return == TRUE)
BOOLEAN
UnregisterRegistryCallback(
    VOID
);

// Reference: https://github.com/Microsoft/Windows-driver-samples/blob/6c1981b8504329521343ad00f32daa847fa6083a/general/registry/regfltr/sys/regfltr.h
_Ret_maybenull_
PVOID
CreateCallbackContext(
    _In_    PCWSTR  AltitudeString
);

_Success_(return == TRUE)
BOOLEAN
InsertCallbackContext(
    _In_    PMILOU_CALLBACK_CONTEXT CallbackCtx
);

_Ret_maybenull_
PMILOU_CALLBACK_CONTEXT
FindCallbackContext(
    _In_    LARGE_INTEGER   Cookie
);

_Ret_maybenull_
PMILOU_CALLBACK_CONTEXT
FindAndRemoveCallbackContext(
    _In_    LARGE_INTEGER   Cookie
);

VOID
DeleteCallbackContext(
    _In_    PMILOU_CALLBACK_CONTEXT   CallbackCtx
);
