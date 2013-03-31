#include <ntifs.h>
#include <ntdddisk.h>
#include <wdmsec.h>
#include "BaseDef.h"
#include "BaseFun.h"
#include "MySFilter.h"
#include "MajorFunction.h"
#include "FastIO.h"
#include "User.h"

// ����������ȫ��ָ��
PDRIVER_OBJECT gSFilterDriverObject = NULL;

// CDO��ȫ��ָ��
PDEVICE_OBJECT gSFilterControlDeviceObject = NULL;

// �洢CDO�ķ���������
UNICODE_STRING gCDOLinkNameStr;
WCHAR gCDOLinkNameBuf[MY_DEV_MAX_NAME];

// ������
FAST_MUTEX gSfilterAttachLock;

// Ŀ¼�洢ָ��
extern PDIR_NODE gListHead;

// ��������
VOID
DriverUnload (
			  IN PDRIVER_OBJECT DriverObject
			  );


extern "C" NTSTATUS DriverEntry(
					 IN PDRIVER_OBJECT DriverObject,
					 IN PUNICODE_STRING RegistryPath
					 )
{

	_asm int 3

	NTSTATUS status = STATUS_SUCCESS;

	// ������������ָ��
	gSFilterDriverObject = DriverObject;

	// ж������
	gSFilterDriverObject->DriverUnload = DriverUnload;

	// ��ʼ��������
	ExInitializeFastMutex(&gSfilterAttachLock);

	// ��ʼ��Ŀ¼��
	gListHead = NULL;

	// ���ûص�����
	UNICODE_STRING CDONameStr;
	WCHAR CDONameBuf[MY_DEV_MAX_NAME];
	UNICODE_STRING CDOSybLnkStr;
	WCHAR CDOSybLnkBuf[MY_DEV_MAX_NAME];
	RtlInitEmptyUnicodeString(&CDONameStr,CDONameBuf,sizeof(CDONameBuf));
	RtlInitEmptyUnicodeString(&CDOSybLnkStr,CDOSybLnkBuf,sizeof(CDOSybLnkBuf));

	status = OnSfilterDriverEntry(gSFilterDriverObject,RegistryPath,&CDONameStr,&CDOSybLnkStr);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("OnSfilterDriverEntry Failed!\n"));
		return status;
	}

	// ��д���·��
	UNICODE_STRING CDOFullNameStr;
	WCHAR CDOFullNameBuf[MY_DEV_MAX_PATH];
	RtlInitEmptyUnicodeString(&CDOFullNameStr,CDOFullNameBuf,sizeof(CDOFullNameBuf));
	UNICODE_STRING path;
	RtlInitUnicodeString(&path,L"\\FileSystem\\Filters\\");
	RtlCopyUnicodeString(&CDOFullNameStr,&path);
	RtlAppendUnicodeStringToString(&CDOFullNameStr,&CDONameStr);
	
	// ����CDO
	status = IoCreateDevice(DriverObject,
		0,
		&CDOFullNameStr,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&gSFilterControlDeviceObject);

	if (!NT_SUCCESS(status))
	{
		// XP֮ǰ�İ汾���ܴ���status == STATUS_OBJECT_PATH_NOT_FOUND,���ﲻ������
		KdPrint(("IoCreateDevice Failed"));
		return status;
	}

	// ���ɷ�������
	UNICODE_STRING CDOFullSybLnkStr;
	WCHAR CDOSybLnkFullBuf[MY_DEV_MAX_PATH];
	RtlInitEmptyUnicodeString(&CDOFullSybLnkStr,CDOSybLnkFullBuf,sizeof(CDOSybLnkFullBuf));
	RtlCopyUnicodeString(&CDOFullSybLnkStr,&path);
	RtlAppendUnicodeStringToString(&CDOFullSybLnkStr,&CDOSybLnkStr);

	status = IoCreateSymbolicLink(&CDOFullSybLnkStr,&CDOFullNameStr);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("IoCreateSymbolicLink Failed"));
		// ɾ�����ɵ�CDO
		IoDeleteDevice(gSFilterControlDeviceObject);
		return status;
	}
	
	// ��дȫ���豸������
	RtlInitEmptyUnicodeString(&gCDOLinkNameStr,gCDOLinkNameBuf,sizeof(gCDOLinkNameBuf));
	RtlCopyUnicodeString(&gCDOLinkNameStr,&CDOFullSybLnkStr);

	// ��дMajorFunction
	ULONG i;
	for (i = 0;i < IRP_MJ_MAXIMUM_FUNCTION;i++)
	{
		gSFilterDriverObject->MajorFunction[i] = SfPassThrough;
	}

	gSFilterDriverObject->MajorFunction[IRP_MJ_CREATE] = SfCreate;
	// �Ҳ��봦������������ġ���
//	DriverObject->MajorFunction[IRP_MJ_CREATE_NAMED_PIPE] = SfCreate;
//	DriverObject->MajorFunction[IRP_MJ_CREATE_MAILSLOT] = SfCreate;

	DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = SfFsControl;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SfCleanupClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = SfRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = SfWrite;
	DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] = SfSetInfo;

	// ����FastIO
	PFAST_IO_DISPATCH fastIoDispatch;
	fastIoDispatch = (PFAST_IO_DISPATCH)ExAllocatePoolWithTag(NonPagedPool,sizeof(FAST_IO_DISPATCH),POOL_TAG);

	if (!fastIoDispatch)
	{
		KdPrint(("ExAllocatePoolWithTag For fastIoDispatch Failed"));
		IoDeleteDevice(gSFilterControlDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(fastIoDispatch,sizeof(FAST_IO_DISPATCH));

	fastIoDispatch->SizeOfFastIoDispatch = sizeof( FAST_IO_DISPATCH );
	fastIoDispatch->FastIoCheckIfPossible = SfFastIoCheckIfPossible;
	fastIoDispatch->FastIoRead = SfFastIoRead;
	fastIoDispatch->FastIoWrite = SfFastIoWrite;
	fastIoDispatch->FastIoQueryBasicInfo = SfFastIoQueryBasicInfo;
	fastIoDispatch->FastIoQueryStandardInfo = SfFastIoQueryStandardInfo;
	fastIoDispatch->FastIoLock = SfFastIoLock;
	fastIoDispatch->FastIoUnlockSingle = SfFastIoUnlockSingle;
	fastIoDispatch->FastIoUnlockAll = SfFastIoUnlockAll;
	fastIoDispatch->FastIoUnlockAllByKey = SfFastIoUnlockAllByKey;
	fastIoDispatch->FastIoDeviceControl = SfFastIoDeviceControl;
	fastIoDispatch->FastIoDetachDevice = SfFastIoDetachDevice;
	fastIoDispatch->FastIoQueryNetworkOpenInfo = SfFastIoQueryNetworkOpenInfo;
	fastIoDispatch->MdlRead = SfFastIoMdlRead;
	fastIoDispatch->MdlReadComplete = SfFastIoMdlReadComplete;
	fastIoDispatch->PrepareMdlWrite = SfFastIoPrepareMdlWrite;
	fastIoDispatch->MdlWriteComplete = SfFastIoMdlWriteComplete;
	fastIoDispatch->FastIoReadCompressed = SfFastIoReadCompressed;
	fastIoDispatch->FastIoWriteCompressed = SfFastIoWriteCompressed;
	fastIoDispatch->MdlReadCompleteCompressed = SfFastIoMdlReadCompleteCompressed;
	fastIoDispatch->MdlWriteCompleteCompressed = SfFastIoMdlWriteCompleteCompressed;
	fastIoDispatch->FastIoQueryOpen = SfFastIoQueryOpen;

	gSFilterDriverObject->FastIoDispatch = fastIoDispatch;


	// ��������Ļص�����
	FS_FILTER_CALLBACKS fsFilterCallbacks;
	fsFilterCallbacks.SizeOfFsFilterCallbacks = sizeof( FS_FILTER_CALLBACKS );
	fsFilterCallbacks.PreAcquireForSectionSynchronization = SfPreFsFilterPassThrough;
	fsFilterCallbacks.PostAcquireForSectionSynchronization = SfPostFsFilterPassThrough;
	fsFilterCallbacks.PreReleaseForSectionSynchronization = SfPreFsFilterPassThrough;
	fsFilterCallbacks.PostReleaseForSectionSynchronization = SfPostFsFilterPassThrough;
	fsFilterCallbacks.PreAcquireForCcFlush = SfPreFsFilterPassThrough;
	fsFilterCallbacks.PostAcquireForCcFlush = SfPostFsFilterPassThrough;
	fsFilterCallbacks.PreReleaseForCcFlush = SfPreFsFilterPassThrough;
	fsFilterCallbacks.PostReleaseForCcFlush = SfPostFsFilterPassThrough;
	fsFilterCallbacks.PreAcquireForModifiedPageWriter = SfPreFsFilterPassThrough;
	fsFilterCallbacks.PostAcquireForModifiedPageWriter = SfPostFsFilterPassThrough;
	fsFilterCallbacks.PreReleaseForModifiedPageWriter = SfPreFsFilterPassThrough;
	fsFilterCallbacks.PostReleaseForModifiedPageWriter = SfPostFsFilterPassThrough;

	status = FsRtlRegisterFileSystemFilterCallbacks( DriverObject, 
		&fsFilterCallbacks );

	if (!NT_SUCCESS( status )) 
	{
		DriverObject->FastIoDispatch = NULL;
		ExFreePool( fastIoDispatch );
		IoDeleteDevice( gSFilterControlDeviceObject );
		return status;
	}

	// ע���ļ�ϵͳ�����ֹͣ�Ļص�����
	status = IoRegisterFsRegistrationChange(gSFilterDriverObject,SfFsNotification);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("IoRegisterFsRegistrationChange Failed"));
		gSFilterDriverObject->FastIoDispatch = NULL;
		ExFreePoolWithTag(fastIoDispatch,POOL_TAG);
		IoDeleteDevice(gSFilterControlDeviceObject);
		return status;
	}

	// RAW file system device δ������֪����ʲô����

	// ���ó�ʼ��ȫ�����
	ClearFlag(gSFilterDriverObject->Flags,DO_DEVICE_INITIALIZING);

	return STATUS_SUCCESS;
}

/***********************************************************************
* ��������:DriverUnload 
* ��������:����ж�غ���������������ļ�������Ӧ�ñ�ж�أ�
* �����б�:
*		��
* ����ֵ:��
***********************************************************************/
VOID
DriverUnload (
			  IN PDRIVER_OBJECT DriverObject
			  )
{
	LOG_PRINT("DriverUnload!");
	
	NTSTATUS status = STATUS_SUCCESS;
	ASSERT(DriverObject == gSFilterDriverObject);

	// ֹͣ�����ļ�ϵͳ�仯
	IoUnregisterFsRegistrationChange(gSFilterDriverObject,SfFsNotification);

	PDEVICE_OBJECT devList[DEVOBJ_LIST_SIZE];
	ULONG numDevices;

	while(1)
	{
		// �����ѭ����ֱ�����е��豸���������

		// ��ñ����������������豸�б�
		status = IoEnumerateDeviceObjectList(gSFilterDriverObject,
			devList,
			sizeof(devList),
			&numDevices);

		if (numDevices <= 0)
		{
			// û���豸�ˣ������˳���
			break;
		}
		
		ULONG i;
		PSFILTER_DEVICE_EXTENSION devExt;
		for (i = 0;i < numDevices;i++)
		{
			devExt = (PSFILTER_DEVICE_EXTENSION)devList[i]->DeviceExtension;
			if (devExt != NULL)
			{
				// ����CDO��ȡ����󶨵��豸
				IoDetachDevice(devExt->AttachedToDeviceObject);
			}
		}

		// ȡ��֮ǰ���ܻ����Ѿ����ͳ�����IRP����ʱһ��ʱ�䣬ȷ������IRP��������
		LARGE_INTEGER interval;
		interval.QuadPart = (1 * DELAY_ONE_SECOND);
		KeDelayExecutionThread(KernelMode,FALSE,&interval);

		// ������������
		for (i = 0; i < numDevices;i++)
		{
			
			if (devList[i]->DeviceExtension != NULL)
			{
				// ����CDO������
			//	KdPrint(("IoDeleteDevice %d",i));
				SfCleanupMountedDevice( devList[i] );
			} 
			else
			{
				if(devList[i] == gSFilterControlDeviceObject)
				{
				//	KdPrint(("IoDeleteDevice CDO"));
					// ��CDO
					gSFilterControlDeviceObject = NULL;
					// ɾ����������
					IoDeleteSymbolicLink(&gCDOLinkNameStr);
				}
			}
			IoDeleteDevice( devList[i] );
			ObDereferenceObject( devList[i] );
		}
	}

	// ����FastIO����Ŀռ�
	ExFreePoolWithTag(gSFilterDriverObject->FastIoDispatch,POOL_TAG);
	gSFilterDriverObject->FastIoDispatch = NULL;

	// �����û��ص�����
	OnSfilterDriverUnload();
}