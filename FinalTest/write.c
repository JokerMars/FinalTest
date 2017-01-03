#include "CallbackRoutines.h"



FLT_PREOP_CALLBACK_STATUS
PreWrite(
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
	PVOID newBuf = NULL;
	PVOID newMdl = NULL;
	PPRE_2_POST_CONTEXT p2pCtx = NULL;
	PVOID origBuf = NULL;
	ULONG writeLen = iopb->Parameters.Write.Length;

	try
	{
#if 1
		//
		// fitering work
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
		KdPrint(("\nIRP_MJ_WRITE:\n"));
		KdPrint(("    Process Name: %s\n", procName));
		KdPrint(("   *Write Len: %d\n", Data->Iopb->Parameters.Write.Length));
		KdPrint(("  **Write Offset: %d\n", Data->Iopb->Parameters.Write.ByteOffset.QuadPart));
		KdPrint((" ***Write Offset + Len: %d\n", Data->Iopb->Parameters.Write.Length +
			Data->Iopb->Parameters.Write.ByteOffset.QuadPart));

		KdPrint(("    The File Name: %wZ\n", &(pfNameInfo->Name)));
		KdPrint(("    The File Ext: %wZ\n", &(pfNameInfo->Extension)));
		KdPrint(("    The Volume: %wZ\n", &(pfNameInfo->Volume)));
		
		//KdPrint(("    Buffer Address: %08x\n", Data->Iopb->Parameters.Write.WriteBuffer));
#endif

#endif
		//
		//some thing about write work
		//

		if (writeLen == 0)
		{
			leave;
		}

	/*	if (FlagOn(IRP_NOCACHE, iopb->IrpFlags)) 
		{
			writeLen = (ULONG)ROUND_TO_SIZE(writeLen, 0x200);
		}*/

		newBuf = FltAllocatePoolAlignedWithTag(FltObjects->Instance,
			NonPagedPool,
			(SIZE_T)writeLen,
			BUFFER_SWAP_TAG);

		if (newBuf == NULL) 
		{	
			leave;
		}

		if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION)) 
		{

			//
			//  Allocate a MDL for the new allocated memory.  If we fail
			//  the MDL allocation then we won't swap buffer for this operation
			//

			newMdl = IoAllocateMdl(newBuf,
				writeLen,
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
		//  If the users original buffer had a MDL, get a system address.
		//

		if (iopb->Parameters.Write.MdlAddress != NULL) {

			//
			//  This should be a simple MDL. We don't expect chained MDLs
			//  this high up the stack
			//

			FLT_ASSERT(((PMDL)iopb->Parameters.Write.MdlAddress)->Next == NULL);

			origBuf = MmGetSystemAddressForMdlSafe(iopb->Parameters.Write.MdlAddress,
				NormalPagePriority | MdlMappingNoExecute);

			if (origBuf == NULL) {

				//
				//  If we could not get a system address for the users buffer,
				//  then we are going to fail this operation.
				//

				Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Data->IoStatus.Information = 0;
				retVal = FLT_PREOP_COMPLETE;
				leave;
			}

		}
		else {

			//
			//  There was no MDL defined, use the given buffer address.
			//

			origBuf = iopb->Parameters.Write.WriteBuffer;
		}

		//
		//  Copy the memory, we must do this inside the try/except because we
		//  may be using a users buffer address
		//

		try {

			//
			// before copy, we need encrypt the data
			//

			PCHAR buffer = origBuf;
			for (int i = 0; i < writeLen; i++)
			{
				buffer[i] = buffer[i] ^ 0x77;
			}

			RtlCopyMemory(newBuf,
				origBuf,
				writeLen);

		} except(EXCEPTION_EXECUTE_HANDLER) {

			//
			//  The copy failed, return an error, failing the operation.
			//

			Data->IoStatus.Status = GetExceptionCode();
			Data->IoStatus.Information = 0;
			retVal = FLT_PREOP_COMPLETE;
			leave;
		}

#if DBG
		KdPrint(("    The Old Mdl: %p\n", iopb->Parameters.Write.MdlAddress));
		KdPrint(("    The Old Buffer: %p\n", iopb->Parameters.Write.WriteBuffer));
		KdPrint(("    The New Mdl: %p\n", newMdl));
		KdPrint(("    The New Buffer: %p\n", newBuf));
#endif

		//
		//  We are ready to swap buffers, get a pre2Post context structure.
		//  We need it to pass the volume context and the allocate memory
		//  buffer to the post operation callback.
		//

		p2pCtx = ExAllocateFromNPagedLookasideList(&Pre2PostContextList);

		if (p2pCtx == NULL) {
			leave;
		}


		iopb->Parameters.Write.WriteBuffer = newBuf;
		iopb->Parameters.Write.MdlAddress = newMdl;
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
PostWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);

	//
	//  Free allocate POOL and volume context
	//

	FltFreePoolAlignedWithTag(FltObjects->Instance,
		p2pCtx->SwappedBuffer,
		BUFFER_SWAP_TAG);

	ExFreeToNPagedLookasideList(&Pre2PostContextList,
		p2pCtx);

	return FLT_POSTOP_FINISHED_PROCESSING;


	return FLT_POSTOP_FINISHED_PROCESSING;
}
