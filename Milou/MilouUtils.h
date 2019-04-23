#pragma once

#include "MilouFltr.h"

_Success_(NT_SUCCESS(return) == TRUE)
NTSTATUS
CaptureBuffer(
    _Outptr_result_maybenull_   PVOID   *CapturedBuffer,
    _In_reads_bytes_(Length)    PVOID   Buffer,
    _In_                        SIZE_T  Length,
    _In_                        ULONG   PoolTag
);

VOID
FreeCapturedBuffer(
    _In_    PVOID   CapturedBuffer,
    _In_    ULONG   PoolTag
);

NTSTATUS
CaptureUnicodeString(
    _Inout_ PUNICODE_STRING     DestString,
    _In_    PCUNICODE_STRING    SourceString,
    _In_    ULONG               PoolTag
);

VOID
FreeCapturedUnicodeString(
    _In_    PUNICODE_STRING String,
    _In_    ULONG           PoolTag
);

VOID
DetectOSVersion();