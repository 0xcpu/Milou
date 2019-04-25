#pragma once

#include "MilouEtw.h"
#include "MilouUtils.h"

/*************************************************************************
	Prototypes
*************************************************************************/

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
FsFilterUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
FsFilterQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
FsFilterPreOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS
FsFilterPostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

//
//  operation registration
//
// Maybe it would be better to use MDL to be able to access User Buffers

CONST FLT_OPERATION_REGISTRATION Callbacks[] = { 
    { IRP_MJ_CREATE,
	  0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_CLOSE,
      0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_READ,
      0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_WRITE,
      0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_QUERY_INFORMATION,
      0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_SET_INFORMATION,
      0,
      FsFilterPreOperation,
      FsFilterPostOperation },

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),           //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    FsFilterUnload,                     //  MiniFilterUnload

    NULL,                               //  InstanceSetup
    FsFilterQueryTeardown,              //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};
