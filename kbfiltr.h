/*++
Copyright (c) 1997  Microsoft Corporation

Module Name:

    kbfilter.h

Abstract:

    This module contains the common private declarations for the keyboard
    packet filter

Environment:

    kernel mode only

--*/

#ifndef KBFILTER_H
#define KBFILTER_H

#pragma warning(disable:4201)

#include "ntddk.h"
#include "kbdmou.h"
#include <ntddkbd.h>
#include <ntdd8042.h>

#pragma warning(default:4201)

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include <initguid.h>
#include <devguid.h>

#include "public.h"

#define KBFILTER_POOL_TAG (ULONG) 'tlfK'

#define EnableDebugOutput

#ifdef EnableDebugOutput
  #define DebugPrint(_x_) DbgPrint _x_
#else
  #define DebugPrint(_x_)
#endif

#if DBG
  #define TRAP() DbgBreakPoint()
#else
  #define TRAP()
#endif

#define MIN(_A_,_B_) (((_A_) < (_B_)) ? (_A_) : (_B_))

//
// Lag mitigation constants
//
#define MAX_RECENT_KEYS 16
#define LAG_MITIGATION_THRESHOLD_MS 300  // 300ms threshold for duplicate detection

//
// Structure to track recent key inputs for lag mitigation
//
typedef struct _RECENT_KEY_INPUT {
    USHORT MakeCode;
    USHORT Flags;
    LARGE_INTEGER Timestamp;
} RECENT_KEY_INPUT, *PRECENT_KEY_INPUT;

typedef struct _DEVICE_EXTENSION
{
    //
    // Back pointer to device object
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // Target device for requests
    //
    PDEVICE_OBJECT TargetDeviceObject;

    //
    // Number of creates sent down
    //
    LONG EnableCount;

    //
    // The real connect data that this driver reports to
    //
    CONNECT_DATA UpperConnectData;

    //
    // Previous initialization and hook routines (and context)
    //
    PVOID UpperContext;
    PI8042_KEYBOARD_INITIALIZATION_ROUTINE UpperInitializationRoutine;
    PI8042_KEYBOARD_ISR UpperIsrHook;

    //
    // Write function from within KbFilter_IsrHook
    //
    IN PI8042_ISR_WRITE_PORT IsrWritePort;

    //
    // Queue the current packet (ie the one passed into KbFilter_IsrHook)
    //
    IN PI8042_QUEUE_PACKET QueueKeyboardPacket;

    //
    // Context for IsrWritePort, QueueKeyboardPacket
    //
    IN PVOID CallContext;

    //
    // Cached Keyboard Attributes
    //
    KEYBOARD_ATTRIBUTES KeyboardAttributes;

    //
    // Lag mitigation - track recent key inputs
    //
    RECENT_KEY_INPUT RecentKeys[MAX_RECENT_KEYS];
    ULONG RecentKeyIndex;
    KSPIN_LOCK RecentKeysLock;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Function to get device extension from device object
//
#define FilterGetData(DeviceObject) \
    ((PDEVICE_EXTENSION) DeviceObject->DeviceExtension)

typedef struct _WORKER_ITEM_CONTEXT {

    PIRP Request;
    PDEVICE_OBJECT DeviceObject;

} WORKER_ITEM_CONTEXT, *PWORKER_ITEM_CONTEXT;

//
// Prototypes
//
DRIVER_INITIALIZE DriverEntry;
DRIVER_ADD_DEVICE KbFilter_AddDevice;

DRIVER_DISPATCH KbFilter_DispatchGeneral;
DRIVER_DISPATCH KbFilter_DispatchInternalDeviceControl;
DRIVER_UNLOAD KbFilter_Unload;

IO_COMPLETION_ROUTINE KbFilterRequestCompletionRoutine;

NTSTATUS
KbFilter_InitializationRoutine(
    IN PVOID                           InitializationContext,
    IN PVOID                           SynchFuncContext,
    IN PI8042_SYNCH_READ_PORT          ReadPort,
    IN PI8042_SYNCH_WRITE_PORT         WritePort,
    OUT PBOOLEAN                       TurnTranslationOn
    );

BOOLEAN
KbFilter_IsrHook(
    PVOID                  IsrContext,
    PKEYBOARD_INPUT_DATA   CurrentInput,
    POUTPUT_PACKET         CurrentOutput,
    UCHAR                  StatusByte,
    PUCHAR                 DataByte,
    PBOOLEAN               ContinueProcessing,
    PKEYBOARD_SCAN_STATE   ScanState
    );

VOID
KbFilter_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

IO_COMPLETION_ROUTINE KbFilterRequestCompletionRoutine;


//
// IOCTL Related defintions
//

//
// Used to identify kbfilter bus. This guid is used as the enumeration string
// for the device id.
DEFINE_GUID(GUID_BUS_KBFILTER,
0xa65c87f9, 0xbe02, 0x4ed9, 0x92, 0xec, 0x1, 0x2d, 0x41, 0x61, 0x69, 0xfa);
// {A65C87F9-BE02-4ed9-92EC-012D416169FA}

DEFINE_GUID(GUID_DEVINTERFACE_KBFILTER,
0x3fb7299d, 0x6847, 0x4490, 0xb0, 0xc9, 0x99, 0xe0, 0x98, 0x6a, 0xb8, 0x86);
// {3FB7299D-6847-4490-B0C9-99E0986AB886}


#define  KBFILTR_DEVICE_ID L"{A65C87F9-BE02-4ed9-92EC-012D416169FA}\\KeyboardFilter\0"


typedef struct _RPDO_DEVICE_DATA
{
    ULONG InstanceNo;

    //
    // Device object for the rawPdo
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // Parent device object 
    //
    PDEVICE_OBJECT ParentDeviceObject;

} RPDO_DEVICE_DATA, *PRPDO_DEVICE_DATA;

//
// Function to get rawPdo device data from device object
//
#define PdoGetData(DeviceObject) \
    ((PRPDO_DEVICE_DATA) DeviceObject->DeviceExtension)

// Raw PDO functionality is disabled to minimize WDF dependencies
/*
NTSTATUS
KbFiltr_CreateRawPdo(
    PDEVICE_OBJECT       Device,
    ULONG                InstanceNo
);
*/



#endif  // KBFILTER_H

