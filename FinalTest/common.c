#include "common.h"


ULONG_PTR OperationStatusCtx = 1;
ULONG gTraceFlags = 0;

PCHAR MonitoredProcess[MAXNUM] = { NULL };
UNICODE_STRING Ext[MAXNUM] = { 0 };

ULONG g_curProcessNameOffset;

//
// initialize the above process and ext 
//
VOID InitMonitorVariable()
{
	//MonitoredProcess[0] = "notepad.exe";
	MonitoredProcess[0] = "acad.exe";
	MonitoredProcess[1] = "Connect.Service.ContentService.exe";
	MonitoredProcess[2] = "AdAppMgr.exe";
	MonitoredProcess[3] = "AdAppMgrSvc.exe";
	MonitoredProcess[4] = "WSCommCntr4.exe";

	RtlInitUnicodeString(Ext+0, L"dwg");
	RtlInitUnicodeString(Ext+1, L"bak");
	RtlInitUnicodeString(Ext+2, L"dwl");
	RtlInitUnicodeString(Ext+3, L"dwl2");

}


BOOLEAN IsMonitoredProcess(PCHAR procName)
{
	int i;
	for (i = 0; i < MAXNUM; i++)
	{
		if (MonitoredProcess[i] == NULL)continue;
		if (strncmp(MonitoredProcess[i], procName, strlen(procName)) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}


BOOLEAN IsMonitoredFileExt(PUNICODE_STRING ext)
{
	int i;
	for (i = 0; i < MAXNUM; i++)
	{
		if (Ext[i].Length == 0)continue;
		if (RtlCompareUnicodeString(&Ext[i], ext, TRUE) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}


BOOLEAN IsMonitored(PCHAR procName, PUNICODE_STRING ext)
{
	if (IsMonitoredProcess(procName) && IsMonitoredFileExt(ext))
	{
		return TRUE;
	}

	return FALSE;
}



ULONG GetProcessNameOffset()
{
	PEPROCESS curproc = NULL;
	int i = 0;

	curproc = PsGetCurrentProcess();

	for (i = 0; i<3 * PAGE_SIZE; i++)
	{
		if (!strncmp("System", (PCHAR)curproc + i, strlen("System")))
		{
			return i;
		}
	}

	return 0;
}

PCHAR GetProcessName()
{
	PEPROCESS curproc = NULL;
	curproc = PsGetCurrentProcess();

	if (curproc != NULL)
	{
		return (PCHAR)curproc + g_curProcessNameOffset;
	}

	return NULL;
}

