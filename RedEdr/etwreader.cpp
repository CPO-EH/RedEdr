#include <windows.h>
#include <evntrace.h>
#include <tdh.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

#include "logging.h"
#include "etwreader.h"
#include "processcache.h"
#include "config.h"
#include "etwhandler.h"
#include "etwconsumer.h"

#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "advapi32.lib")


// EtwReader: Setup ETW (EtwConsumer) so it calls the EtwHandlers

// Local variables
std::list<EtwConsumer*> EtwConsumers;


int InitializeEtwReader(std::vector<HANDLE>& threads) {
    int id = 0;
    EtwConsumer* etwConsumer = NULL;

    if (g_config.etw_standard) {
        etwConsumer = new EtwConsumer();
        if (!etwConsumer->SetupEtw(
            id++,
            L"{22fb2cd6-0e7b-422b-a0c7-2fad1fd0e716}",
            &EventRecordCallbackKernelProcess,
            L"Microsoft-Windows-Kernel-Process",
            g_config.sessionName.c_str()
        )) {
            LOG_W(LOG_ERROR, L"ETW: Problem with: Microsoft-Windows-Kernel-Process");
        }
        else {
            EtwConsumers.push_back(etwConsumer);
        }
    }
    if (g_config.etw_kernelaudit) {
        etwConsumer = new EtwConsumer();
        if (!etwConsumer->SetupEtw(
            id++,
            L"{e02a841c-75a3-4fa7-afc8-ae09cf9b7f23}",
            &EventRecordCallbackApiCalls,
            L"Microsoft-Windows-Kernel-Audit-API-Calls",
            g_config.sessionName.c_str()
        )) {
            LOG_W(LOG_ERROR, L"ETW: Problem with: Microsoft-Windows-Kernel-Audit-API-Calls");
        }
        else {
            EtwConsumers.push_back(etwConsumer);
        }

        // Nothing interesting in here
        // L"{8c416c79-d49b-4f01-a467-e56d3aa8234c}", &EventRecordCallbackWin32, L"Microsoft-Windows-Win32k");
    }

    // Security-Auditing, special case
    if (g_config.etw_secaudit) {
        // Check: Are we system?
        char user_name[128] = { 0 };
        DWORD user_name_length = 128;
        if (!GetUserNameA(user_name, &user_name_length) || strcmp(user_name, "SYSTEM") != 0)
        {
            LOG_A(LOG_ERROR, "ETW: Microsoft-Windows-Security-Auditing can only be traced by SYSTEM");
        }
        else {
            etwConsumer = new EtwConsumer();
            if (!etwConsumer->SetupEtwSecurityAuditing(
                id++,
                &EventRecordCallbackSecurityAuditing,
                g_config.sessionName.c_str()
            )) {
				LOG_W(LOG_ERROR, L"ETW: Problem with: Microsoft-Windows-Security-Auditing");
			}
            else {
				EtwConsumers.push_back(etwConsumer);
			}
        }
    }

    // Antimalware
    /*
    if (g_config.etw_defender) {
        reader = SetupTrace(id++, L"{0a002690-3839-4e3a-b3b6-96d8df868d99}", &EventRecordCallbackAntimalwareEngine, L"Microsoft-Antimalware-Engine");
        if (reader != NULL) {
            readers.push_back(reader);
        }
        reader = SetupTrace(id++, L"{8E92DEEF-5E17-413B-B927-59B2F06A3CFC}", &EventRecordCallbackAntimalwareRtp, L"Microsoft-Antimalware-RTP");
        if (reader != NULL) {
            readers.push_back(reader);
        }
        reader = SetupTrace(id++, L"{CFEB0608-330E-4410-B00D-56D8DA9986E6}", &EventRecordCallbackPrintAll, L"Microsoft-Antimalware-AMFilter");
        if (reader != NULL) {
            readers.push_back(reader);
        }
        reader = SetupTrace(id++, L"{2A576B87-09A7-520E-C21A-4942F0271D67}", &EventRecordCallbackPrintAll, L"Microsoft-Antimalware-Scan-Interface");
        if (reader != NULL) {
            readers.push_back(reader);
        }
        reader = SetupTrace(id++, L"{e4b70372-261f-4c54-8fa6-a5a7914d73da}", &EventRecordCallbackPrintAll, L"Microsoft-Antimalware-Protection");
        if (reader != NULL) {
            readers.push_back(reader);
        }
    }*/

    // ProcessTrace() can only handle 1 (one) real-time processing session
    // Create threads instead fuck...
    LOG_A(LOG_INFO, "ETW: Start the tracing threads");
    for (const auto& consumer: EtwConsumers) {
        HANDLE thread = CreateThread(NULL, 0, TraceProcessingThread, consumer, 0, NULL);
        if (thread == NULL) {
            LOG_A(LOG_ERROR, "ETW: Failed to create thread, continue");
            //return 1;
        }
        else {
            threads.push_back(thread);
        }
    }

    return 0;
}


DWORD WINAPI TraceProcessingThread(LPVOID param) {
    EtwConsumer* etwConsumer = (EtwConsumer*)param;
    LOG_A(LOG_INFO, "!ETW: Start Thread %i", etwConsumer->getId());

    if (!etwConsumer->StartEtw()) {
        LOG_A(LOG_ERROR, "ERror");
    }

    LOG_A(LOG_INFO, "!ETW: Exit Thread %i", etwConsumer->getId());
    return 0;
}


void EtwReaderStopAll() {
    LOG_A(LOG_INFO, "ETW: Stopping EtwTracing"); fflush(stdout);

    for (const auto& etwConsumer : EtwConsumers) {
        etwConsumer->StopEtw();
    }

    // Todo free memory

    LOG_A(LOG_INFO, "ETW: EtwTracing all stopped"); 
}


