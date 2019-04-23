#include "Miloard.h"


_Success_(return == TRUE)
BOOLEAN
InstallDriver(
	_In_	SC_HANDLE	hSCManager,
	_In_	LPCTSTR		ServiceName,
	_In_	LPCTSTR		DriverPath
)
{
    SC_HANDLE   schService;
    DWORD       errCode;

    schService = CreateService(hSCManager,
                               ServiceName,
                               ServiceName,
                               SERVICE_ALL_ACCESS,
                               SERVICE_KERNEL_DRIVER,
                               SERVICE_DEMAND_START,
                               SERVICE_ERROR_NORMAL,
                               DriverPath,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    if (NULL == schService) {
        errCode = GetLastError();

        if (ERROR_SERVICE_EXISTS == errCode) {
            fprintf(stderr, "[Miloard] Service already exists\n");

            return TRUE;
        } else {
            fprintf(stderr, "[Miloard] Failed creating service: %#x\n", errCode);

            return FALSE;
        }
    } else {
        CloseServiceHandle(schService);

        fprintf(stdout, "[Miloard] Service %S was successfully created\n", ServiceName);

        return TRUE;
    }
}

_Success_(return == TRUE)
BOOLEAN
UninstallDriver(
	_In_	SC_HANDLE	hSCManager,
	_In_	LPCTSTR		ServiceName
)
{
    SC_HANDLE   schService;
    BOOLEAN     bRetStatus = FALSE;

    schService = OpenService(hSCManager,
                             ServiceName,
                             SERVICE_ALL_ACCESS);
    if (NULL == schService) {
        fprintf(stderr, "[Miloard] Failed opening the service: %#X\n", GetLastError());

        return bRetStatus;
    }

    if (DeleteService(schService)) {
        bRetStatus = TRUE;

        fprintf(stdout, "[Miloard] Service %S was successfully deleted\n", ServiceName);
    } else {
        fprintf(stderr, "[Miloard] Failed deleting the service: %#X\n", GetLastError());
    }

    CloseServiceHandle(schService);

    return bRetStatus;
}

_Success_(return == TRUE)
BOOLEAN
StartDriver(
	_In_	SC_HANDLE	hSCManager,
	_In_	LPCTSTR		ServiceName
)
{
    SC_HANDLE   schService;
    DWORD       errCode;
    BOOLEAN     bRetStatus = FALSE;

    schService = OpenService(hSCManager,
                             ServiceName,
                             SERVICE_ALL_ACCESS);
    if (NULL == schService) {
        fprintf(stderr, "[Miloard] Failed opening the service: %#X\n", GetLastError());

        return bRetStatus;
    }

    if (!StartService(schService,
                      0,
                      NULL)) {
        errCode = GetLastError();

        if (ERROR_SERVICE_ALREADY_RUNNING == errCode) {
            bRetStatus = TRUE;

            fprintf(stdout, "[Miloard] Service %S already running\n", ServiceName);
        } else {
            fprintf(stderr, "[Miloard] Failed starting the service: %#X\n", errCode);
        }
    } else {
        bRetStatus = TRUE;

        fprintf(stdout, "[Miloard] Service %S was successfully started\n", ServiceName);
    }

    CloseServiceHandle(schService);

    return bRetStatus;
}

_Success_(return == TRUE)
BOOLEAN
StopDriver(
	_In_	SC_HANDLE	hSCManager,
	_In_	LPCTSTR		ServiceName
)
{
    SC_HANDLE       schService;
    SERVICE_STATUS  serviceStatus;
    BOOLEAN         bRetStatus = FALSE;

    schService = OpenService(hSCManager,
                             ServiceName,
                             SERVICE_ALL_ACCESS);
    if (NULL == schService) {
        fprintf(stderr, "[Miloard] Failed opening the service: %#X\n", GetLastError());

        return bRetStatus;
    }

    if (ControlService(schService,
                       SERVICE_CONTROL_STOP,
                       &serviceStatus)) {
        bRetStatus = TRUE;

        fprintf(stdout, "[Miloard] Service %S was successfully stopped\n", ServiceName);
    } else {
        fprintf(stderr, "[Miloard] Failed stopping the service: %#X\n", GetLastError());
    }

    CloseServiceHandle(schService);

    return bRetStatus;
}

_Success_(return == TRUE)
BOOLEAN
MiloardManageDriver(
    _In_	LPCTSTR		DriverPath,
	_In_	LPCTSTR		ServiceName,
	_In_	SIZE_T		Action
)
{
	SC_HANDLE	schSCManager;
	BOOLEAN		bRetVal = TRUE;

	if (NULL == DriverPath || NULL == ServiceName) {
		fprintf(stderr, "[Miloard] Invalid driver name or service name\n");

		return FALSE;
	}

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == schSCManager) {
		fprintf(stderr, "[Miloard] Failed opening a connection to SCM: %#X\n", GetLastError());

		return FALSE;
	}

	switch (Action) {
	case MILOARD_ACTION_INSTALL:
		if (InstallDriver(schSCManager, ServiceName, DriverPath)) {
			bRetVal = StartDriver(schSCManager, ServiceName);
		} else {
			bRetVal = FALSE;
		}

		break;
	case MILOARD_ACTION_UNINSTALL:
		if (StopDriver(schSCManager, ServiceName)) {
			bRetVal = UninstallDriver(schSCManager, ServiceName);
		} else {
			bRetVal = FALSE;
		}

		break;
	default:
		fprintf(stderr, "[Miloard] Unknown action: %zu\n", Action);

		bRetVal = FALSE;

		break;
	}

	if (CloseServiceHandle(schSCManager) == 0) {
		fprintf(stderr, "[Miloard] Failed closing SCM: %#X\n", GetLastError());
	}

	return bRetVal;
}
