#pragma once

#ifndef COMMON_H
#define COMMON_H

#define PIPE_BUFFER_SIZE 8192 // thats the pipe buffer (default 4096)
#define DATA_BUFFER_SIZE 4096 // all buffers for strings

#define WCHAR_BUFFER_SIZE 2048 // half of DATA_BUFFER_SIZE as its 2 bytes per char

#define SMALL_PIPE 256
#define WCHAR_SMALL_PIPE 128

#define IOCTL_MY_IOCTL_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MY_DRIVER_DATA {
    WCHAR filename[256];
    int enable;
    int dll_inject;
} MY_DRIVER_DATA, * PMY_DRIVER_DATA;


#define DRIVER_KERNEL_PIPE_NAME L"\\??\\pipe\\RedEdrKrnCom"
#define KERNEL_PIPE_NAME L"\\\\.\\pipe\\RedEdrKrnCom"
#define DLL_PIPE_NAME L"\\\\.\\pipe\\RedEdrDllCom"

#define PPL_SERVICE_PIPE_NAME L"\\\\.\\pipe\\RedEdrPplService"
#define SERVICE_NAME  L"RedEdrPplService"

#define DRIVER_NAME L"c:\\RedEdr\\elam_driver.sys"
#define MAX_BUF_SIZE 2048

#define MAX_CALLSTACK_ENTRIES 5

#endif