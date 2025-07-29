/*--

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.


Module Name:

    kbfiltr.c

Abstract: This is an upper device filter driver sample for PS/2 keyboard. This
        driver layers in between the KbdClass driver and i8042prt driver and
        hooks the callback routine that moves keyboard inputs from the port
        driver to class driver. With this filter, you can remove or insert
        additional keys into the stream. This sample also creates a raw
        PDO and registers an interface so that application can talk to
        the filter driver directly without going thru the PS/2 devicestack.
        The reason for providing this additional interface is because the keyboard
        device is an exclusive secure device and it's not possible to open the
        device from usermode and send custom ioctls.

        If you want to filter keyboard inputs from all the keyboards (ps2, usb)
        plugged into the system then you can install this driver as a class filter
        and make it sit below the kbdclass filter driver by adding the service
        name of this filter driver before the kbdclass filter in the registry at
        " HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\
        {4D36E96B-E325-11CE-BFC1-08002BE10318}\UpperFilters"


Environment:

    Kernel mode only.

--*/

#include "kbfiltr.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, KbFilter_AddDevice)
#pragma alloc_text (PAGE, KbFilter_DispatchInternalDeviceControl)
#endif

ULONG InstanceNo = 0;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    UNREFERENCED_PARAMETER(RegistryPath);

    DebugPrint(("Keyboard Filter Driver Sample - WDM Edition.\n"));

    //
    // Set up the device driver entry points.
    //
    DriverObject->MajorFunction[IRP_MJ_CREATE] = KbFilter_DispatchGeneral;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = KbFilter_DispatchGeneral;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KbFilter_DispatchGeneral;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = KbFilter_DispatchInternalDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_POWER] = KbFilter_DispatchGeneral;
    DriverObject->MajorFunction[IRP_MJ_PNP] = KbFilter_DispatchGeneral;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = KbFilter_DispatchGeneral;
    DriverObject->DriverExtension->AddDevice = KbFilter_AddDevice;
    DriverObject->DriverUnload = KbFilter_Unload;

    return STATUS_SUCCESS;
}

NTSTATUS
KbFilter_AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    )
/*++
Routine Description:

    AddDevice is called by the PnP manager to create and initialize
    filter device objects.

Arguments:

    DriverObject - Handle to a driver object created in DriverEntry

    PhysicalDeviceObject - Pointer to the physical device object

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS                status;
    PDEVICE_OBJECT          deviceObject = NULL;
    PDEVICE_EXTENSION       filterExt;
    
    DebugPrint(("Enter KbFilter_AddDevice \n"));

    //
    // Create filter device object.
    //
    status = IoCreateDevice(DriverObject,
                           sizeof(DEVICE_EXTENSION),
                           NULL,
                           FILE_DEVICE_KEYBOARD,
                           FILE_DEVICE_SECURE_OPEN,
                           FALSE,
                           &deviceObject);
    
    if (!NT_SUCCESS(status)) {
        DebugPrint(("IoCreateDevice failed with status code 0x%x\n", status));
        return status;
    }

    filterExt = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;
    RtlZeroMemory(filterExt, sizeof(DEVICE_EXTENSION));

    //
    // Initialize the device extension
    //
    filterExt->DeviceObject = deviceObject;

    //
    // Attach to the device stack
    //
    filterExt->TargetDeviceObject = IoAttachDeviceToDeviceStack(deviceObject, PhysicalDeviceObject);

    if (!filterExt->TargetDeviceObject) {
        DebugPrint(("IoAttachDeviceToDeviceStack failed\n"));
        IoDeleteDevice(deviceObject);
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Initialize lag mitigation structures
    //
    KeInitializeSpinLock(&filterExt->RecentKeysLock);
    filterExt->RecentKeyIndex = 0;
    RtlZeroMemory(filterExt->RecentKeys, sizeof(filterExt->RecentKeys));

    //
    // Set the device object flags
    //
    deviceObject->Flags |= (filterExt->TargetDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE));
    deviceObject->DeviceType = filterExt->TargetDeviceObject->DeviceType;
    deviceObject->Characteristics = filterExt->TargetDeviceObject->Characteristics;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    //
    // Create a RAW pdo so we can provide a sideband communication with
    // the application. For now, this is disabled to minimize WDF dependencies.
    //
    // status = KbFiltr_CreateRawPdo(deviceObject, ++InstanceNo);

    return STATUS_SUCCESS;
}

VOID
KbFilter_Unload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++
Routine Description:

    Driver unload routine.

Arguments:

    DriverObject - Handle to a driver object

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);
    
    DebugPrint(("KbFilter_Unload\n"));
}

NTSTATUS
KbFilter_DispatchGeneral(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    General dispatch routine for IRPs we don't specifically handle.
    This routine simply passes the IRP down to the next driver.

Arguments:

    DeviceObject - Pointer to the device object.
    Irp - Pointer to the request packet.

Return Value:

    Status returned from the next driver.

--*/
{
    PDEVICE_EXTENSION deviceExtension;
    
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    
    //
    // Simply pass the request along
    //
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);
}

NTSTATUS
KbFilter_DispatchInternalDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    There are two specific control codes that are of interest:

    IOCTL_INTERNAL_KEYBOARD_CONNECT:
        Store the old context and function pointer and replace it with our own.
        This makes life much simpler than intercepting IRPs sent by the RIT and
        modifying them on the way back up.

    IOCTL_INTERNAL_I8042_HOOK_KEYBOARD:
        Add in the necessary function pointers and context values so that we can
        alter how the ps/2 keyboard is initialized.

    NOTE:  Handling IOCTL_INTERNAL_I8042_HOOK_KEYBOARD is *NOT* necessary if
           all you want to do is filter KEYBOARD_INPUT_DATAs.  You can remove
           the handling code and all related device extension fields and
           functions to conserve space.

Arguments:

    DeviceObject - Pointer to the device object.
    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION               devExt;
    PINTERNAL_I8042_HOOK_KEYBOARD   hookKeyboard = NULL;
    PCONNECT_DATA                   connectData = NULL;
    NTSTATUS                        status = STATUS_SUCCESS;
    PIO_STACK_LOCATION              irpStack;
    ULONG                           ioControlCode;
    ULONG                           inputBufferLength;
    ULONG                           outputBufferLength;
    BOOLEAN                         needCompletion = FALSE;

    PAGED_CODE();

    DebugPrint(("Entered KbFilter_DispatchInternalDeviceControl\n"));

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    
    ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
    inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    switch (ioControlCode) {

    //
    // Connect a keyboard class device driver to the port driver.
    //
    case IOCTL_INTERNAL_KEYBOARD_CONNECT:
        //
        // Only allow one connection.
        //
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }

        //
        // Get the input buffer from the request
        //
        if (inputBufferLength < sizeof(CONNECT_DATA)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        connectData = (PCONNECT_DATA) irpStack->Parameters.DeviceIoControl.Type3InputBuffer;
        
        devExt->UpperConnectData = *connectData;

        //
        // Hook into the report chain.  Everytime a keyboard packet is reported
        // to the system, KbFilter_ServiceCallback will be called
        //

        connectData->ClassDeviceObject = DeviceObject;

#pragma warning(disable:4152)  //nonstandard extension, function/data pointer conversion

        connectData->ClassService = KbFilter_ServiceCallback;

#pragma warning(default:4152)

        break;

    //
    // Disconnect a keyboard class device driver from the port driver.
    //
    case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:

        //
        // Clear the connection parameters in the device extension.
        //
        // devExt->UpperConnectData.ClassDeviceObject = NULL;
        // devExt->UpperConnectData.ClassService = NULL;

        status = STATUS_NOT_IMPLEMENTED;
        break;

    //
    // Attach this driver to the initialization and byte processing of the
    // i8042 (ie PS/2) keyboard.  This is only necessary if you want to do PS/2
    // specific functions, otherwise hooking the CONNECT_DATA is sufficient
    //
    case IOCTL_INTERNAL_I8042_HOOK_KEYBOARD:

        DebugPrint(("hook keyboard received!\n"));

        //
        // Get the input buffer from the request
        //
        if (inputBufferLength < sizeof(INTERNAL_I8042_HOOK_KEYBOARD)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        hookKeyboard = (PINTERNAL_I8042_HOOK_KEYBOARD) irpStack->Parameters.DeviceIoControl.Type3InputBuffer;

        //
        // Enter our own initialization routine and record any Init routine
        // that may be above us.  Repeat for the isr hook
        //
        devExt->UpperContext = hookKeyboard->Context;

        //
        // replace old Context with our own
        //
        hookKeyboard->Context = (PVOID) devExt;

        if (hookKeyboard->InitializationRoutine) {
            devExt->UpperInitializationRoutine =
                hookKeyboard->InitializationRoutine;
        }
        hookKeyboard->InitializationRoutine =
            (PI8042_KEYBOARD_INITIALIZATION_ROUTINE)
            KbFilter_InitializationRoutine;

        if (hookKeyboard->IsrRoutine) {
            devExt->UpperIsrHook = hookKeyboard->IsrRoutine;
        }
        hookKeyboard->IsrRoutine = (PI8042_KEYBOARD_ISR) KbFilter_IsrHook;

        //
        // Store all of the other important stuff
        //
        devExt->IsrWritePort = hookKeyboard->IsrWritePort;
        devExt->QueueKeyboardPacket = hookKeyboard->QueueKeyboardPacket;
        devExt->CallContext = hookKeyboard->CallContext;

        status = STATUS_SUCCESS;
        break;


    case IOCTL_KEYBOARD_QUERY_ATTRIBUTES:
        needCompletion = TRUE;
        break;
        
    //
    // Might want to capture these in the future.  For now, then pass them down
    // the stack.  These queries must be successful for the RIT to communicate
    // with the keyboard.
    //
    case IOCTL_KEYBOARD_QUERY_INDICATOR_TRANSLATION:
    case IOCTL_KEYBOARD_QUERY_INDICATORS:
    case IOCTL_KEYBOARD_SET_INDICATORS:
    case IOCTL_KEYBOARD_QUERY_TYPEMATIC:
    case IOCTL_KEYBOARD_SET_TYPEMATIC:
        break;
    }

    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    //
    // Forward the request down the stack
    //
    if (needCompletion) {
        //
        // Set up completion routine to capture output data
        //
        IoCopyCurrentIrpStackLocationToNext(Irp);
        IoSetCompletionRoutine(Irp,
                             KbFilterRequestCompletionRoutine,
                             devExt,
                             TRUE,
                             TRUE,
                             TRUE);
        return IoCallDriver(devExt->TargetDeviceObject, Irp);
    }
    else {
        //
        // Simply pass the request along
        //
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(devExt->TargetDeviceObject, Irp);
    }
}

NTSTATUS
KbFilter_InitializationRoutine(
    IN PVOID                           InitializationContext,
    IN PVOID                           SynchFuncContext,
    IN PI8042_SYNCH_READ_PORT          ReadPort,
    IN PI8042_SYNCH_WRITE_PORT         WritePort,
    OUT PBOOLEAN                       TurnTranslationOn
    )
/*++

Routine Description:

    This routine gets called after the following has been performed on the kb
    1)  a reset
    2)  set the typematic
    3)  set the LEDs

    i8042prt specific code, if you are writing a packet only filter driver, you
    can remove this function

Arguments:

    DeviceObject - Context passed during IOCTL_INTERNAL_I8042_HOOK_KEYBOARD

    SynchFuncContext - Context to pass when calling Read/WritePort

    Read/WritePort - Functions to synchronoulsy read and write to the kb

    TurnTranslationOn - If TRUE when this function returns, i8042prt will not
                        turn on translation on the keyboard

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt;
    NTSTATUS            status = STATUS_SUCCESS;

    devExt = (PDEVICE_EXTENSION)InitializationContext;

    //
    // Do any interesting processing here.  We just call any other drivers
    // in the chain if they exist.  Make sure Translation is turned on as well
    //
    if (devExt->UpperInitializationRoutine) {
        status = (*devExt->UpperInitializationRoutine) (
                        devExt->UpperContext,
                        SynchFuncContext,
                        ReadPort,
                        WritePort,
                        TurnTranslationOn
                        );

        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    *TurnTranslationOn = TRUE;
    return status;
}

BOOLEAN
KbFilter_IsrHook(
    PVOID                  IsrContext,
    PKEYBOARD_INPUT_DATA   CurrentInput,
    POUTPUT_PACKET         CurrentOutput,
    UCHAR                  StatusByte,
    PUCHAR                 DataByte,
    PBOOLEAN               ContinueProcessing,
    PKEYBOARD_SCAN_STATE   ScanState
    )
/*++

Routine Description:

    This routine gets called at the beginning of processing of the kb interrupt.

    i8042prt specific code, if you are writing a packet only filter driver, you
    can remove this function

Arguments:

    DeviceObject - Our context passed during IOCTL_INTERNAL_I8042_HOOK_KEYBOARD

    CurrentInput - Current input packet being formulated by processing all the
                    interrupts

    CurrentOutput - Current list of bytes being written to the keyboard or the
                    i8042 port.

    StatusByte    - Byte read from I/O port 60 when the interrupt occurred

    DataByte      - Byte read from I/O port 64 when the interrupt occurred.
                    This value can be modified and i8042prt will use this value
                    if ContinueProcessing is TRUE

    ContinueProcessing - If TRUE, i8042prt will proceed with normal processing of
                         the interrupt.  If FALSE, i8042prt will return from the
                         interrupt after this function returns.  Also, if FALSE,
                         it is this functions responsibilityt to report the input
                         packet via the function provided in the hook IOCTL or via
                         queueing a DPC within this driver and calling the
                         service callback function acquired from the connect IOCTL

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION devExt;
    BOOLEAN           retVal = TRUE;

    devExt = (PDEVICE_EXTENSION)IsrContext;

    if (devExt->UpperIsrHook) {
        retVal = (*devExt->UpperIsrHook) (
                        devExt->UpperContext,
                        CurrentInput,
                        CurrentOutput,
                        StatusByte,
                        DataByte,
                        ContinueProcessing,
                        ScanState
                        );

        if (!retVal || !(*ContinueProcessing)) {
            return retVal;
        }
    }

    *ContinueProcessing = TRUE;
    return retVal;
}

BOOLEAN
KbFilter_IsRecentDuplicateKey(
    IN PDEVICE_EXTENSION DevExt,
    IN PKEYBOARD_INPUT_DATA InputData
    )
/*++

Routine Description:

    Checks if the current key input is a recent duplicate that should be filtered
    out due to lag-induced multiple key presses.

Arguments:

    DevExt - Device extension containing recent key tracking data
    InputData - Current keyboard input data to check

Return Value:

    TRUE if this is a recent duplicate that should be filtered, FALSE otherwise.

--*/
{
    KIRQL oldIrql;
    LARGE_INTEGER currentTime;
    LARGE_INTEGER timeDiff;
    ULONG i;
    BOOLEAN isDuplicate = FALSE;

    // Only filter key-down events (make codes)
    if (InputData->Flags & KEY_BREAK) {
        return FALSE;
    }

    KeQuerySystemTime(&currentTime);

    KeAcquireSpinLock(&DevExt->RecentKeysLock, &oldIrql);

    // Check recent keys for duplicates
    for (i = 0; i < MAX_RECENT_KEYS; i++) {
        PRECENT_KEY_INPUT recentKey = &DevExt->RecentKeys[i];
        
        // Skip empty slots
        if (recentKey->MakeCode == 0) {
            continue;
        }

        // Check if this is the same key
        if (recentKey->MakeCode == InputData->MakeCode) {
            // Calculate time difference in 100ns units
            timeDiff.QuadPart = currentTime.QuadPart - recentKey->Timestamp.QuadPart;
            
            // Convert to milliseconds (100ns units to ms)
            LONG timeDiffMs = (LONG)(timeDiff.QuadPart / 10000);
            
            // If within threshold, it's a duplicate
            if (timeDiffMs < LAG_MITIGATION_THRESHOLD_MS) {
                isDuplicate = TRUE;
                DebugPrint(("Filtered duplicate key 0x%x (time diff: %dms)\n", 
                           InputData->MakeCode, timeDiffMs));
                break;
            }
        }
    }

    KeReleaseSpinLock(&DevExt->RecentKeysLock, oldIrql);

    return isDuplicate;
}

VOID
KbFilter_AddRecentKey(
    IN PDEVICE_EXTENSION DevExt,
    IN PKEYBOARD_INPUT_DATA InputData
    )
/*++

Routine Description:

    Adds a key input to the recent keys tracking for lag mitigation.

Arguments:

    DevExt - Device extension containing recent key tracking data
    InputData - Keyboard input data to add to recent keys

Return Value:

    None.

--*/
{
    KIRQL oldIrql;
    LARGE_INTEGER currentTime;

    // Only track key-down events (make codes)
    if (InputData->Flags & KEY_BREAK) {
        return;
    }

    KeQuerySystemTime(&currentTime);

    KeAcquireSpinLock(&DevExt->RecentKeysLock, &oldIrql);

    // Add to circular buffer
    DevExt->RecentKeys[DevExt->RecentKeyIndex].MakeCode = InputData->MakeCode;
    DevExt->RecentKeys[DevExt->RecentKeyIndex].Flags = InputData->Flags;
    DevExt->RecentKeys[DevExt->RecentKeyIndex].Timestamp = currentTime;

    // Move to next slot in circular buffer
    DevExt->RecentKeyIndex = (DevExt->RecentKeyIndex + 1) % MAX_RECENT_KEYS;

    KeReleaseSpinLock(&DevExt->RecentKeysLock, oldIrql);
}

VOID
KbFilter_ServiceCallback(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
/*++

Routine Description:

    Called when there are keyboard packets to report to the Win32 subsystem.
    You can do anything you like to the packets.  For instance:

    o Drop a packet altogether
    o Mutate the contents of a packet
    o Insert packets into the stream

Arguments:

    DeviceObject - Context passed during the connect IOCTL

    InputDataStart - First packet to be reported

    InputDataEnd - One past the last packet to be reported.  Total number of
                   packets is equal to InputDataEnd - InputDataStart

    InputDataConsumed - Set to the total number of packets consumed by the RIT
                        (via the function pointer we replaced in the connect
                        IOCTL)

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt;
    PKEYBOARD_INPUT_DATA currentInput, outputStart, outputCurrent;
    ULONG originalCount, filteredCount = 0;

    devExt = FilterGetData(DeviceObject);

    originalCount = (ULONG)(InputDataEnd - InputDataStart);
    
    // Allocate temporary buffer for filtered inputs
    outputStart = (PKEYBOARD_INPUT_DATA)ExAllocatePoolWithTag(
        NonPagedPoolNx,
        originalCount * sizeof(KEYBOARD_INPUT_DATA),
        KBFILTER_POOL_TAG
    );

    if (outputStart == NULL) {
        // If allocation fails, pass through all inputs unfiltered
        DebugPrint(("Memory allocation failed, passing through unfiltered\n"));
        (*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR) devExt->UpperConnectData.ClassService)(
            devExt->UpperConnectData.ClassDeviceObject,
            InputDataStart,
            InputDataEnd,
            InputDataConsumed);
        return;
    }

    outputCurrent = outputStart;

    // Process each input packet
    for (currentInput = InputDataStart; currentInput < InputDataEnd; currentInput++) {
        DebugPrint(("kbfilter v1: %x\n", currentInput->MakeCode));

        // Check if this is a lag-induced duplicate
        if (KbFilter_IsRecentDuplicateKey(devExt, currentInput)) {
            // Skip this input - it's a duplicate
            continue;
        }

        // Copy the input to output buffer
        *outputCurrent = *currentInput;
        outputCurrent++;
        filteredCount++;

        // Add to recent keys tracking (only for key-down events)
        KbFilter_AddRecentKey(devExt, currentInput);
    }

    // Call the upper service with filtered inputs
    if (filteredCount > 0) {
        (*(PSERVICE_CALLBACK_ROUTINE)(ULONG_PTR) devExt->UpperConnectData.ClassService)(
            devExt->UpperConnectData.ClassDeviceObject,
            outputStart,
            outputStart + filteredCount,
            InputDataConsumed);
    } else {
        *InputDataConsumed = 0;
    }

    // Free the temporary buffer
    ExFreePoolWithTag(outputStart, KBFILTER_POOL_TAG);

    if (originalCount != filteredCount) {
        DebugPrint(("Filtered %d duplicate keys out of %d total\n", 
                   originalCount - filteredCount, originalCount));
    }
}

NTSTATUS
KbFilterRequestCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
   )
/*++

Routine Description:

    Completion Routine

Arguments:

    DeviceObject - Device object
    Irp - IRP
    Context - Driver supplied context

Return Value:

    NTSTATUS

--*/
{
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) Context;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    UNREFERENCED_PARAMETER(DeviceObject);
 
    //
    // Save the keyboard attributes in our context area so that we can return
    // them to the app later.
    //
    if (NT_SUCCESS(Irp->IoStatus.Status) && 
        irpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_KEYBOARD_QUERY_ATTRIBUTES) {

        if (Irp->IoStatus.Information >= sizeof(KEYBOARD_ATTRIBUTES)) {
            
            RtlCopyMemory(&deviceExtension->KeyboardAttributes,
                         Irp->AssociatedIrp.SystemBuffer,
                         sizeof(KEYBOARD_ATTRIBUTES));
        }
    }

    return STATUS_SUCCESS;
}


