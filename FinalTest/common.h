#pragma once

#ifndef _COMMON_H
#define _COMMON_H


#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

extern ULONG_PTR OperationStatusCtx;
extern ULONG gTraceFlags;

//
// define the number of monitored process
//
#define MAXNUM 10
extern PCHAR MonitoredProcess[MAXNUM];
extern UNICODE_STRING Ext[MAXNUM];
extern UNICODE_STRING MonitoredDirectory;

extern ULONG g_curProcessNameOffset;


VOID InitMonitorVariable();

BOOLEAN IsMonitoredProcess(PCHAR procName);

BOOLEAN IsMonitoredFileExt(PUNICODE_STRING ext);

BOOLEAN IsMonitored(PCHAR procName, PUNICODE_STRING ext);

BOOLEAN IsMonitoredDirectory(PUNICODE_STRING path);

ULONG GetProcessNameOffset();

PCHAR GetProcessName();





#endif