#pragma once

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

                                                                               
// CONSTANTS
#define MILOARD_ACTION_INSTALL		0
#define MILOARD_ACTION_UNINSTALL	1

_Success_(return == TRUE)
BOOLEAN
MiloardManageDriver(
    _In_	LPCTSTR		DriverPath,
    _In_	LPCTSTR		ServiceName,
    _In_	SIZE_T		Action
);
