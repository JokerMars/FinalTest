/*++

Module Name:

    FinalTest.c

Abstract:

    This is the main module of the FinalTest miniFilter driver.

Environment:

    Kernel mode

--*/

#include "FinalTest.h"


PFLT_FILTER gFilterHandle;

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

	{
		IRP_MJ_WRITE,
		0,
		PreWrite,
		PostWrite
	},

	{
		IRP_MJ_READ,
		0,
		PreRead,
		PostRead
	},

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    FinalTestUnload,                           //  MiniFilterUnload

    FinalTestInstanceSetup,                    //  InstanceSetup
    FinalTestInstanceQueryTeardown,            //  InstanceQueryTeardown
    FinalTestInstanceTeardownStart,            //  InstanceTeardownStart
    FinalTestInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FinalTest!DriverEntry: Entered\n") );

	//
	// to do my initial work
	//

	g_curProcessNameOffset = GetProcessNameOffset();
#if DBG
	KdPrint(("FinalTest Entry:\n    Process Offset: %d\n", g_curProcessNameOffset));
#endif

	InitMonitorVariable();

	ExInitializeNPagedLookasideList(&Pre2PostContextList,
		NULL,
		NULL,
		0,
		sizeof(PRE_2_POST_CONTEXT),
		PRE_2_POST_TAG,
		0);





    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );

			ExDeleteNPagedLookasideList(&Pre2PostContextList);
        }
    }

    return status;
}

NTSTATUS
FinalTestUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("FinalTest!FinalTestUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

	ExDeleteNPagedLookasideList(&Pre2PostContextList);

    return STATUS_SUCCESS;
}

