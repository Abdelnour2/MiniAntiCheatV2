#include <ntifs.h>
#include <ntddk.h>

#include "MiniAntiCheatCommon.h"

void DriverUnload(PDRIVER_OBJECT DriverObject);

NTSTATUS MiniAntiCheatCreate(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS MiniAntiCheatClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS MiniAntiCheatDeviceControl(PDEVICE_OBJECT, PIRP Irp);

bool IsProcessRunning(const char* targetName);

extern "C" NTKERNELAPI PEPROCESS PsInitialSystemProcess;
extern "C" NTKERNELAPI PCHAR PsGetProcessImageFileName(PEPROCESS);

void ProcessNotifyRoutine(PEPROCESS, HANDLE, PPS_CREATE_NOTIFY_INFO CreateInfo);

OB_PREOP_CALLBACK_STATUS PreCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation);

ULONG GameInstances = 0;
HANDLE GameProcessId = NULL;
PVOID RegistrationHandle = NULL;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	KdPrint(("Mini Anti Cheat - DriverEntry\n"));

	DriverObject->DriverUnload = DriverUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = MiniAntiCheatCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = MiniAntiCheatClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MiniAntiCheatDeviceControl;

	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\MiniAntiCheat");
	PDEVICE_OBJECT deviceObject;
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed at IoCreateDevice - Error 0x%X\n", status));

		return status;
	}

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\MiniAntiCheat");
	status = IoCreateSymbolicLink(&symLink, &deviceName);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed at IoCreateSymbolicLink - Error 0x%X\n", status));

		IoDeleteDevice(deviceObject);

		return status;
	}

	status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutine, FALSE);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed at PsSetCreateProcessNotifyRoutineEx - Error 0x%X\n", status));

		PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutine, TRUE);
		IoDeleteSymbolicLink(&symLink);
		IoDeleteDevice(DriverObject->DeviceObject);

		return status;
	}

	OB_OPERATION_REGISTRATION opReg = { 0 };
    opReg.ObjectType = PsProcessType;
    opReg.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    opReg.PreOperation = PreCallback;

    OB_CALLBACK_REGISTRATION cbReg = { 0 };
    cbReg.Version = OB_FLT_REGISTRATION_VERSION;
    cbReg.OperationRegistrationCount = 1;
    cbReg.OperationRegistration = &opReg;
    cbReg.Altitude = RTL_CONSTANT_STRING(L"321000");

	// 3. Register the Shield
	status = ObRegisterCallbacks(&cbReg, &RegistrationHandle);
	
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to register Memory Shield: 0x%X\n", status));

		ObUnRegisterCallbacks(RegistrationHandle);
		PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutine, TRUE);
		IoDeleteSymbolicLink(&symLink);
		IoDeleteDevice(DriverObject->DeviceObject);

		return status;
	}

	return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
	KdPrint(("Mini Anti Cheat - Unloaded\n"));

	if (RegistrationHandle) {
		ObUnRegisterCallbacks(RegistrationHandle);
	}

	PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutine, TRUE);

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\MiniAntiCheat");
	IoDeleteSymbolicLink(&symLink);

	IoDeleteDevice(DriverObject->DeviceObject);
}


NTSTATUS MiniAntiCheatCreate(PDEVICE_OBJECT, PIRP Irp) {
	GameInstances++;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);

	return STATUS_SUCCESS;
}

NTSTATUS MiniAntiCheatClose(PDEVICE_OBJECT, PIRP Irp) {
	if (GameInstances > 0) {
		GameInstances--;
	}

	GameProcessId = NULL;

	KdPrint(("Mini Anti Cheat: Handle closed. Shield Deactivated. Remaining Instances: %lu\n", GameInstances));

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);

	return STATUS_SUCCESS;
}

NTSTATUS MiniAntiCheatDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
		case CHECK_BLACKLIST_TO_START:
		{
			if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MiniAntiCheatOutput)) {
				status = STATUS_BUFFER_TOO_SMALL;

				break;
			}

			auto output = (MiniAntiCheatOutput*)Irp->AssociatedIrp.SystemBuffer;

			if (IsProcessRunning("Notepad.exe")) {
				output->isSafeToStart = FALSE;

				KdPrint(("Mini Anti Cheat: Notepad detected - The game will not start!\n"));
			}
			else {
				output->isSafeToStart = TRUE;

				KdPrint(("Mini Anti Cheat: Notepad wasn't detected - The game will start\n"));
			}

			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(MiniAntiCheatOutput);

			break;
		}
		case GET_GAME_PROCESS_ID:
		{
			if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(GameProcessIdInput)) {
				status = STATUS_BUFFER_TOO_SMALL;

				break;
			}

			auto input = (GameProcessIdInput*)Irp->AssociatedIrp.SystemBuffer;

			GameProcessId = (HANDLE)input->processID;

			KdPrint(("Mini Anti Cheat: Registered Game PID: %p (Numeric: %llu)\n", GameProcessId, (ULONG64)GameProcessId));

			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;

			break;
		}
		case DISABLE_SHIELD:
		{
			GameProcessId = NULL;
			KdPrint(("Mini Anti Cheat: Shield manually disabled by request.\n"));
			status = STATUS_SUCCESS;
			break;
		}
	}

	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, 0);

	return status;
}

bool IsProcessRunning(const char* targetName) {
	PEPROCESS sysProcess = PsInitialSystemProcess;

	PLIST_ENTRY listHead = (PLIST_ENTRY)((PUCHAR)sysProcess + 0x1d8);
	PLIST_ENTRY currentEntry = listHead->Flink;

	while (currentEntry != listHead) {
		PEPROCESS entryProcess = (PEPROCESS)((PUCHAR)currentEntry - 0x1d8);

		PCHAR imageName = PsGetProcessImageFileName(entryProcess);
		if (imageName != NULL) {
			if (strstr(imageName, targetName) != NULL) {
				return true;
			}
		}

		currentEntry = currentEntry->Flink;
	}

	return false;
}

void ProcessNotifyRoutine(PEPROCESS, HANDLE, PPS_CREATE_NOTIFY_INFO CreateInfo) {
	if (CreateInfo && GameInstances > 0) {
		if (CreateInfo->ImageFileName != NULL && wcsstr(CreateInfo->ImageFileName->Buffer, L"Notepad.exe") != NULL) {
			CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
			KdPrint(("Mini Anti Cheat: Active Blocking - Cannot Open Notepad.exe While the Game is Running\n"));
		}
	}
}

OB_PREOP_CALLBACK_STATUS PreCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation) {
	UNREFERENCED_PARAMETER(RegistrationContext);
	
	if (OperationInformation->ObjectType != *PsProcessType) {
		return OB_PREOP_SUCCESS;
	}

	PEPROCESS targetProcess = (PEPROCESS)OperationInformation->Object;
	HANDLE targetPid = PsGetProcessId(targetProcess);

	if (targetPid == GameProcessId && GameProcessId != NULL) {
		if (OperationInformation->KernelHandle == FALSE) {
			ACCESS_MASK denyRights = PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_TERMINATE | PROCESS_VM_OPERATION;
		
			if (OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE) {
				OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~denyRights;
			}
			else if (OperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE) {
				OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~denyRights;
			}

			PEPROCESS sourceProcess = PsGetCurrentProcess();
			PCHAR sourceName = PsGetProcessImageFileName(sourceProcess);

			KdPrint(("Mini Anti Cheat: [%s] (PID: %llu) tried to touch the Game - ACCESS DENIED\n", sourceName, (ULONG64)PsGetCurrentProcessId()));
		}
	}

	return OB_PREOP_SUCCESS;
}