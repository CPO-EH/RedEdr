#include <stdio.h>
#include <Windows.h>

#include "control.h"
#include "emitter.h"
#include "../Shared/common.h"
#include "logging.h"
#include "objcache.h"
#include "etwtireader.h"
#include "piping.h"


DWORD start_child_process(wchar_t* childCMD);


HANDLE control_thread = NULL;
BOOL keep_running = TRUE;

PipeServer pipeServer(L"EtwTi");


DWORD WINAPI ServiceControlPipeThread(LPVOID param) {
    wchar_t buffer[WCHAR_BUFFER_SIZE];

    while (keep_running) {
        LOG_W(LOG_INFO, L"Control: Waiting for client (RedEdr.exe) to connect...");
        if (!pipeServer.StartAndWaitForClient(PPL_SERVICE_PIPE_NAME, false)) {
            LOG_A(LOG_ERROR, "Error waiting for RedEdr.exe");
            continue;
        }
        while (keep_running) {
            memset(buffer, 0, sizeof(buffer));
            if (!pipeServer.Receive(buffer, WCHAR_BUFFER_SIZE)) {
                //LOG_A(LOG_ERROR, "Error waiting for RedEdr.exe config");
                break;
            }

            //if (wcscmp(buffer, L"start") == 0) {
            if (wcsstr(buffer, L"start:") != NULL) {
                wchar_t* token = NULL, * context = NULL;
                LOG_W(LOG_INFO, L"Control: Received command: start");

                // should give "start:"
                token = wcstok_s(buffer, L":", &context);
                if (token != NULL) {
                    // should give the thing after "start:"
                    token = wcstok_s(NULL, L":", &context);
                    if (token != NULL) {
                        LOG_W(LOG_INFO, L"Control: Target: %s", token);
                        set_target_name(_wcsdup(token));
                        ConnectEmitterPipe(); // Connect to the RedEdr pipe
                        enable_consumer(TRUE);
                    }
                }
            }
            else if (wcscmp(buffer, L"stop") == 0) {
                LOG_W(LOG_INFO, L"Control: Received command: stop");
                enable_consumer(FALSE);
                DisconnectEmitterPipe(); // Disconnect the RedEdr pipe
            }
            else if (wcscmp(buffer, L"shutdown") == 0) {
                LOG_W(LOG_INFO, L"Control: Received command: shutdown");
                //rededr_remove_service();  // attempt to remove service
                StopControl(); // stop this thread
                ShutdownEtwtiReader(); // also makes main return
                break;
            }
            else {
                LOG_W(LOG_INFO, L"Control: Unknown command: %s", buffer);
            }
        }
        pipeServer.Shutdown();
    }
    LOG_W(LOG_INFO, L"Control: Finished");
    return 0;
}


void StartControl() {
    LOG_W(LOG_INFO, L"Control: Start Thread");
    control_thread = CreateThread(NULL, 0, ServiceControlPipeThread, NULL, 0, NULL);
    if (control_thread == NULL) {
        LOG_W(LOG_INFO, L"Control: Failed to create thread");
    }
}


void StopControl() {
    LOG_W(LOG_INFO, L"Control: Stop Thread");

    // Disable the loops
    keep_running = FALSE;

    // Send some stuff so the ReadFile() in the pipe reader thread returns
    pipeServer.Send((wchar_t *) L"");
}


//////


// broken atm
void rededr_remove_service() {
    //wchar_t* cmd = L"C:\\RedEdr\\RedEdr.exe --pplstop";
    WCHAR childCMD[MAX_BUF_SIZE] = { 0 };
    //wcscpy_s(childCMD, MAX_BUF_SIZE, L"C:\\windows\\system32\\cmd.exe /c \"echo AAA > c:\\rededr\\aa\"");
    //wcscpy_s(childCMD, MAX_BUF_SIZE, L"C:\\RedEdr\\RedEdrPplRemover.exe");
    wcscpy_s(childCMD, MAX_BUF_SIZE, L"C:\\RedEdr\\RedEdr.exe --pplstop");
    start_child_process(childCMD);
}


DWORD start_child_process(wchar_t* childCMD)
{
    DWORD retval = 0;
    DWORD dataSize = MAX_BUF_SIZE;
    LOG_W(LOG_INFO, L"start_child_process: Starting");

    // Create Attribute List
    STARTUPINFOEXW StartupInfoEx = { 0 };
    SIZE_T AttributeListSize = 0;
    StartupInfoEx.StartupInfo.cb = sizeof(StartupInfoEx);
    InitializeProcThreadAttributeList(NULL, 1, 0, &AttributeListSize);
    if (AttributeListSize == 0) {
        retval = GetLastError();
        LOG_W(LOG_INFO, L"start_child_process: InitializeProcThreadAttributeList1 Error: %d\n", retval);
        return retval;
    }
    StartupInfoEx.lpAttributeList =
        (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, AttributeListSize);
    if (InitializeProcThreadAttributeList(StartupInfoEx.lpAttributeList, 1, 0, &AttributeListSize) == FALSE) {
        retval = GetLastError();
        LOG_W(LOG_INFO, L"start_child_process: InitializeProcThreadAttributeList2 Error: %d\n", retval);
        return retval;
    }

    // Set ProtectionLevel to be the same, i.e. PPL
    DWORD ProtectionLevel = PROTECTION_LEVEL_SAME;
    if (UpdateProcThreadAttribute(StartupInfoEx.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_PROTECTION_LEVEL,
        &ProtectionLevel,
        sizeof(ProtectionLevel),
        NULL,
        NULL) == FALSE)
    {
        retval = GetLastError();
        LOG_W(LOG_INFO, L"start_child_process: UpdateProcThreadAttribute Error: %d\n", retval);
        return retval;
    }

    // Start Process (hopefully)
    PROCESS_INFORMATION ProcessInformation = { 0 };
    LOG_W(LOG_INFO, L"start_child_process: Creating Process: '%s'\n", childCMD);
    if (CreateProcess(NULL,
        childCMD,
        NULL,
        NULL,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_PROTECTED_PROCESS,
        NULL,
        NULL,
        (LPSTARTUPINFOW)&StartupInfoEx,
        &ProcessInformation) == FALSE)
    {
        retval = GetLastError();
        if (retval == ERROR_INVALID_IMAGE_HASH) {
            LOG_W(LOG_INFO, L"start_child_process: CreateProcess Error: Invalid Certificate\n");
        }
        else {
            LOG_W(LOG_INFO, L"start_child_process: CreateProcess Error: %d\n", retval);
        }
        return retval;
    }

    // Don't wait on process handle, we're setting our child free into the wild
    // This is to prevent any possible deadlocks

    LOG_W(LOG_INFO, L"start_child_process: finished");
    return retval;
}
