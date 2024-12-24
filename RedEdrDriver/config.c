
#include "config.h"
#include <ntddk.h>
#include <string.h>
#include <stdio.h>

Config g_config;


void init_config() {
	g_config.init_processnotify = 1;
	g_config.init_threadnotify = 1;
	g_config.init_imagenotify = 1;
	g_config.init_obnotify = 0; // too much data, no parser

	g_config.enable_kapc_injection = 0;
	g_config.enable_logging = 0;

	g_config.trace_pid = 0;
	g_config.trace_children = 0;

	wcscpy_s(g_config.target, TARGET_WSTR_LEN, L""); // disable
}


void print_config() {

}