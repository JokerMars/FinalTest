#include "CallbackRoutines.h"


FLT_PREOP_CALLBACK_STATUS
PreRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	BOOLEAN isDir = FALSE;
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION pfNameInfo = NULL;
	FLT_PREOP_CALLBACK_STATUS retVal = FLT_PREOP_SUCCESS_NO_CALLBACK;


	try
	{
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
		KdPrint(("\nIRP_MJ_READ:\n"));
		KdPrint(("    Process Name: %s\n", procName));
		KdPrint(("   *Read Len: %d\n", Data->Iopb->Parameters.Read.Length));
		KdPrint(("  **Read Offset: %d\n", Data->Iopb->Parameters.Read.ByteOffset.QuadPart));
		KdPrint((" ***Read Offset + Len: %d\n", Data->Iopb->Parameters.Read.Length +
			Data->Iopb->Parameters.Read.ByteOffset.QuadPart));

		KdPrint(("    The File Name: %wZ\n", &(pfNameInfo->Name)));
		KdPrint(("    The File Ext: %wZ\n", &(pfNameInfo->Extension)));

		//KdPrint(("    Buffer Address: %08x\n", Data->Iopb->Parameters.Write.WriteBuffer));

		if (FLT_IS_FASTIO_OPERATION(Data))
		{
			KdPrint(("This Read: FastIO\n"));
		}
#endif

		retVal = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}
	finally
	{
		if (pfNameInfo != NULL)
		{
			FltReleaseFileNameInformation(pfNameInfo);
		}
	}

	return retVal;
}


FLT_POSTOP_CALLBACK_STATUS
PostRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	KdPrint(("    Read In File Len: %d\n", Data->IoStatus.Information));
	return FLT_POSTOP_FINISHED_PROCESSING;
}

