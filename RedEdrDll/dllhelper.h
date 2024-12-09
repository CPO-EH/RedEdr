#pragma once
#include <windows.h>
#include <stdio.h>
#include <winternl.h>  // needs to be on bottom?

// Pipe
void InitDllPipe();
void SendDllPipe(wchar_t* buffer);

// Proc
size_t LogMyStackTrace(wchar_t* buf, size_t buf_size);

// Utils
void UnicodeStringToWChar(const UNICODE_STRING* ustr, wchar_t* dest, size_t destSize);

