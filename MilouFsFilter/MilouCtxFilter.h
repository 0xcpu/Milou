#pragma once

//
//  Memory Pool Tags
//

#define CTX_STRING_TAG                        'tSiM'
#define CTX_RESOURCE_TAG                      'cRiM'
#define CTX_STREAMHANDLE_CONTEXT_TAG          'cHiM'


typedef union _FILE_REFERENCE {

    struct {
        ULONGLONG   Value;
        ULONGLONG   UpperZeroes;
    } FileId64;

    FILE_ID_128     FileId128;

} FILE_REFERENCE, *PFILE_REFERENCE;

//
//  Stream handle context data structure
//

typedef struct _CTX_STREAMHANDLE_CONTEXT {

    PFLT_FILE_NAME_INFORMATION  NameInfo;
    volatile LONG               NumOps;
    BOOLEAN                     ExtensionMatch;
    FILE_REFERENCE              FileId;
    BOOLEAN                     SetDisp;
    BOOLEAN                     DeleteOnClose;
    PERESOURCE                  Resource;

} CTX_STREAMHANDLE_CONTEXT, *PCTX_STREAMHANDLE_CONTEXT;

#define CTX_STREAMHANDLE_CONTEXT_SIZE         sizeof( CTX_STREAMHANDLE_CONTEXT )

VOID
CtxContextCleanup(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType
);

NTSTATUS
CtxCreateStreamHandleContext(
    _Outptr_ PCTX_STREAMHANDLE_CONTEXT *StreamContext
);

NTSTATUS
CtxFindOrCreateStreamContext(
    _In_ PFLT_CALLBACK_DATA Cbd,
    _In_ BOOLEAN CreateIfNotFound,
    _Outptr_ PCTX_STREAMHANDLE_CONTEXT *StreamContext,
    _Out_opt_ PBOOLEAN ContextCreated
);

NTSTATUS
GetFileNameInformation(
    _In_  PFLT_CALLBACK_DATA Data,
    _Inout_ PCTX_STREAMHANDLE_CONTEXT StreamContext
);

NTSTATUS
GetFileId(
    _In_  PFLT_CALLBACK_DATA Data,
    _Inout_ PCTX_STREAMHANDLE_CONTEXT StreamContext
);

// Resource stuff just-in-case
FORCEINLINE
PERESOURCE
CtxAllocateResource(
    VOID
)
{
    return ExAllocatePoolWithTag(NonPagedPool, sizeof(ERESOURCE), CTX_RESOURCE_TAG);
}

FORCEINLINE
VOID
CtxFreeResource(
    _In_ PERESOURCE Resource
)
{
    ExFreePoolWithTag(Resource, CTX_RESOURCE_TAG);
}

// https://github.com/Microsoft/Windows-driver-samples/blob/master/filesys/miniFilter/ctx/CtxProc.h
FORCEINLINE
VOID
_Acquires_lock_(_Global_critical_region_)
_IRQL_requires_max_(APC_LEVEL)
CtxAcquireResourceExclusive(
    _Inout_ _Requires_lock_not_held_(*_Curr_) _Acquires_exclusive_lock_(*_Curr_)
    PERESOURCE Resource
)
{
    FLT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
    FLT_ASSERT(ExIsResourceAcquiredExclusiveLite(Resource) ||
        !ExIsResourceAcquiredSharedLite(Resource));

    KeEnterCriticalRegion();
    (VOID)ExAcquireResourceExclusiveLite(Resource, TRUE);
}

FORCEINLINE
VOID
_Releases_lock_(_Global_critical_region_)
_Requires_lock_held_(_Global_critical_region_)
_IRQL_requires_max_(APC_LEVEL)
CtxReleaseResource(
    _Inout_ _Requires_lock_held_(*_Curr_) _Releases_lock_(*_Curr_)
    PERESOURCE Resource
)
{
    FLT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
    FLT_ASSERT(ExIsResourceAcquiredExclusiveLite(Resource) ||
        ExIsResourceAcquiredSharedLite(Resource));

    ExReleaseResourceLite(Resource);
    KeLeaveCriticalRegion();
}