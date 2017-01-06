#include "CallbackRoutines.h"

FLT_PREOP_CALLBACK_STATUS
PreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}


FLT_POSTOP_CALLBACK_STATUS
PostCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	NTSTATUS status;
	BOOLEAN isDir = FALSE;
	PFLT_FILE_NAME_INFORMATION pfNameInfo = NULL;

	try
	{
#if 1
		//
		// filtering work
		//

		status = FltIsDirectory(Data->Iopb->TargetFileObject, Data->Iopb->TargetInstance, &isDir);
		if (!NT_SUCCESS(status) || isDir)
		{
			leave;
		}

		PCHAR procName = GetProcessName();
		if (procName == NULL)
		{
			leave;
		}
		if (!IsMonitoredProcess(procName))
		{
			leave;
		}

		status = FltGetFileNameInformation(Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&pfNameInfo);
		if (!NT_SUCCESS(status) || pfNameInfo == NULL)
		{
			leave;
		}
		FltParseFileNameInformation(pfNameInfo);

		if (!IsMonitoredFileExt(&pfNameInfo->Extension))
		{
			leave;
		}

		if (!IsMonitoredDirectory(&pfNameInfo->Name))
		{
			leave;
		}

		

#if DBG
		KdPrint(("\nIRP_MJ_CREATE\n"));
		KdPrint(("    The File Name: %wZ\n", &(pfNameInfo->Name)));
		KdPrint(("    The File Ext: %wZ\n", &(pfNameInfo->Extension)));
#endif


#endif


	}
	finally
	{
		if (pfNameInfo != NULL)
		{
			FltReleaseFileNameInformation(pfNameInfo);
		}
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}
