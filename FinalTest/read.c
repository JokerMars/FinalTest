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

	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	ULONG readLen = iopb->Parameters.Read.Length;
	PVOID newBuf = NULL;
	PMDL newMdl = NULL;
	PPRE_2_POST_CONTEXT p2pCtx = NULL;

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

#endif
		//
		// something about reading file
		//

		if (readLen == 0)
		{
			leave;
		}

		if (FlagOn(IRP_NOCACHE, iopb->IrpFlags))
		{
			readLen = (ULONG)ROUND_TO_SIZE(readLen, 512);
		}

		newBuf = FltAllocatePoolAlignedWithTag(FltObjects->Instance,
			NonPagedPool,
			(SIZE_T)readLen,
			BUFFER_SWAP_TAG);
		if (newBuf == NULL) 
		{
			leave;
		}

		if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION)) {

			//
			//  Allocate a MDL for the new allocated memory.  If we fail
			//  the MDL allocation then we won't swap buffer for this operation
			//

			newMdl = IoAllocateMdl(newBuf,
				readLen,
				FALSE,
				FALSE,
				NULL);

			if (newMdl == NULL)
			{
				leave;
			}

			//
			//  setup the MDL for the non-paged pool we just allocated
			//

			MmBuildMdlForNonPagedPool(newMdl);
		}

		//
		//  We are ready to swap buffers, get a pre2Post context structure.
		//  We need it to pass the volume context and the allocate memory
		//  buffer to the post operation callback.
		//

		p2pCtx = ExAllocateFromNPagedLookasideList(&Pre2PostContextList);

		if (p2pCtx == NULL) 
		{
			leave;
		}

#if DBG
		KdPrint(("    The Old Mdl: %p\n", iopb->Parameters.Read.MdlAddress));
		KdPrint(("    The Old Buffer: %p\n", iopb->Parameters.Read.ReadBuffer));
		KdPrint(("    The New Mdl: %p\n", newMdl));
		KdPrint(("    The New Buffer: %p\n", newBuf));
#endif

		//
		//  Update the buffer pointers and MDL address, mark we have changed
		//  something.
		//

		iopb->Parameters.Read.ReadBuffer = newBuf;
		iopb->Parameters.Read.MdlAddress = newMdl;
		FltSetCallbackDataDirty(Data);

		//
		//  Pass state to our post-operation callback.
		//

		p2pCtx->SwappedBuffer = newBuf;

		*CompletionContext = p2pCtx;

		//
		//  Return we want a post-operation callback
		//

		retVal = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}
	finally
	{
		if (pfNameInfo != NULL)
		{
			FltReleaseFileNameInformation(pfNameInfo);
		}


		if (retVal != FLT_PREOP_SUCCESS_WITH_CALLBACK) {

			if (newBuf != NULL) {

				FltFreePoolAlignedWithTag(FltObjects->Instance,
					newBuf,
					BUFFER_SWAP_TAG);
			}

			if (newMdl != NULL) {

				IoFreeMdl(newMdl);
			}

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
	PVOID origBuf;
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;
	BOOLEAN cleanupAllocatedBuffer = TRUE;
	FLT_POSTOP_CALLBACK_STATUS retVal = FLT_POSTOP_FINISHED_PROCESSING;

	//
	//  This system won't draining an operation with swapped buffers, verify
	//  the draining flag is not set.
	//

	FLT_ASSERT(!FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING));

	try
	{

		//
		//  If the operation failed or the count is zero, there is no data to
		//  copy so just return now.
		//

		if (!NT_SUCCESS(Data->IoStatus.Status) ||
			(Data->IoStatus.Information == 0)) 
		{
			leave;
		}

		//
		//  We need to copy the read data back into the users buffer.  Note
		//  that the parameters passed in are for the users original buffers
		//  not our swapped buffers.
		//

		if (iopb->Parameters.Read.MdlAddress != NULL) {

		
			FLT_ASSERT(((PMDL)iopb->Parameters.Read.MdlAddress)->Next == NULL);

			origBuf = MmGetSystemAddressForMdlSafe(iopb->Parameters.Read.MdlAddress,
				NormalPagePriority | MdlMappingNoExecute);

			if (origBuf == NULL) {

				//
				//  If we failed to get a SYSTEM address, mark that the read
				//  failed and return.
				//

				Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Data->IoStatus.Information = 0;
				leave;
			}

		}
		else if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) ||
			FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION)) {

			origBuf = iopb->Parameters.Read.ReadBuffer;

		}
		else {

			if (FltDoCompletionProcessingWhenSafe(Data,
				FltObjects,
				CompletionContext,
				Flags,
				SwapPostReadBuffersWhenSafe,
				&retVal)) 
			{

				cleanupAllocatedBuffer = FALSE;

			}
			else 
			{
				Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Data->IoStatus.Information = 0;
			}

			leave;
		}


		//
		//  We either have a system buffer or this is a fastio operation
		//  so we are in the proper context.  Copy the data handling an
		//  exception.
		//

		try 
		{

			//
			// WE need decrypt
			//
			PCHAR buffer = p2pCtx->SwappedBuffer;

			for (int i = 0; i < Data->IoStatus.Information; i++)
			{
				buffer[i] = buffer[i] ^ 0x77;
			}

			RtlCopyMemory(origBuf,
				p2pCtx->SwappedBuffer,
				Data->IoStatus.Information);

		} except(EXCEPTION_EXECUTE_HANDLER) 
		{

			//
			//  The copy failed, return an error, failing the operation.
			//

			Data->IoStatus.Status = GetExceptionCode();
			Data->IoStatus.Information = 0;

		}

#if DBG
		KdPrint(("    The Read Len From File: %d\n", Data->IoStatus.Information));
#endif


	}
	finally
	{
		//
		//  If we are supposed to, cleanup the allocated memory and release
		//  the volume context.  The freeing of the MDL (if there is one) is
		//  handled by FltMgr.
		//

		if (cleanupAllocatedBuffer) {

			FltFreePoolAlignedWithTag(FltObjects->Instance,
				p2pCtx->SwappedBuffer,
				BUFFER_SWAP_TAG);
			ExFreeToNPagedLookasideList(&Pre2PostContextList,
				p2pCtx);
		}
	}

	return retVal;
}

FLT_POSTOP_CALLBACK_STATUS
SwapPostReadBuffersWhenSafe(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;
	PVOID origBuf;
	NTSTATUS status;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	FLT_ASSERT(Data->IoStatus.Information != 0);

	//
	//  This is some sort of user buffer without a MDL, lock the user buffer
	//  so we can access it.  This will create a MDL for it.
	//

	status = FltLockUserBuffer(Data);

	if (!NT_SUCCESS(status)) {

		//
		//  If we can't lock the buffer, fail the operation
		//

		Data->IoStatus.Status = status;
		Data->IoStatus.Information = 0;

	}
	else {

		//
		//  Get a system address for this buffer.
		//

		origBuf = MmGetSystemAddressForMdlSafe(iopb->Parameters.Read.MdlAddress,
			NormalPagePriority | MdlMappingNoExecute);

		if (origBuf == NULL) {
			//
			//  If we couldn't get a SYSTEM buffer address, fail the operation
			//

			Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Data->IoStatus.Information = 0;

		}
		else {

			//
			//  Copy the data back to the original buffer.  Note that we
			//  don't need a try/except because we will always have a system
			//  buffer address.
			//

			PCHAR buffer = p2pCtx->SwappedBuffer;
			for (int i = 0; i < Data->IoStatus.Information; i++)
			{
				buffer[i] = buffer[i] ^ 0x77;
			}


			RtlCopyMemory(origBuf,
				p2pCtx->SwappedBuffer,
				Data->IoStatus.Information);
		}
	}

	//
	//  Free allocated memory and release the volume context
	//

	FltFreePoolAlignedWithTag(FltObjects->Instance,
		p2pCtx->SwappedBuffer,
		BUFFER_SWAP_TAG);

	ExFreeToNPagedLookasideList(&Pre2PostContextList,
		p2pCtx);

	return FLT_POSTOP_FINISHED_PROCESSING;
}