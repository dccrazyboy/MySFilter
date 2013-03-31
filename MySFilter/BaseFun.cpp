#include <ntifs.h>
#include "BaseDef.h"
#include "BaseFun.h"
#include "MySFilter.h"

// ����������ȫ��ָ��
extern PDRIVER_OBJECT gSFilterDriverObject;

// CDO��ȫ��ָ��
extern PDEVICE_OBJECT gSFilterControlDeviceObject;

// ������
extern FAST_MUTEX gSfilterAttachLock;

// Ŀ¼�洢ָ��
PDIR_NODE gListHead;

/***********************************************************************
* ��������:SfCleanupMountedDevice 
* ��������:�豸����ж��ʱ����
* �����б�:
*		DeviceObject:��Ҫж�ص��豸����
* ����ֵ:��
***********************************************************************/
VOID
SfCleanupMountedDevice(
					   IN PDEVICE_OBJECT DeviceObject
					   )
{
	// �����κ�����
	UNREFERENCED_PARAMETER(DeviceObject);
	return;
}

/***********************************************************************
* ��������:SfAttachDeviceToDeviceStack 
* ��������:���豸���豸ջ
* �����б�:
*		SourceDevice:���豸
*		TargetDevice:��Ҫ���󶨵��豸
*		AttachedToDeviceObject:���󶨵��豸
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfAttachDeviceToDeviceStack (
							 IN PDEVICE_OBJECT SourceDevice,
							 IN PDEVICE_OBJECT TargetDevice,
							 IN OUT PDEVICE_OBJECT *AttachedToDeviceObject
							 )
{
	// ��Ϊ��XP�汾��ֱ�ӵ���IoAttachDeviceToDeviceStackSafe
	return IoAttachDeviceToDeviceStackSafe(SourceDevice,TargetDevice,AttachedToDeviceObject);
}

/***********************************************************************
* ��������:SfGetObjectName 
* ��������:����豸����
* �����б�:
*		Object:�豸����
*		Name:����
* ����ֵ:��
***********************************************************************/
VOID
SfGetObjectName(
				IN PVOID Object,
				IN OUT PUNICODE_STRING Name
				)
{
	/*
	NTSTATUS status;
	*
	*	����	���Բ�������ΪnameInfo����ռ䣡����
	*			ObQueryNameString�е�nameInfo��Ҫһ�������Ŀռ�����Ŷ�ȡ�����ƣ������������buffer����������
	*
	UNICODE_STRING nameStr;
	WCHAR nameBuf[MY_DEV_MAX_NAME];
	RtlInitEmptyUnicodeString(&nameStr,nameBuf,sizeof(nameBuf));

	POBJECT_NAME_INFORMATION nameInfo;
	nameInfo = (POBJECT_NAME_INFORMATION)&nameStr;
	ULONG len;

	// �������
	status = ObQueryNameString(Object,nameInfo,sizeof(nameBuf),&len);

	Name->Length = 0;
	if (NT_SUCCESS( status ))
	{
	RtlCopyUnicodeString( Name, &nameInfo->Name );
	}
	*/


	NTSTATUS status;
	CHAR nibuf[512];        //����һ���������ڴ漴��
	POBJECT_NAME_INFORMATION nameInfo = (POBJECT_NAME_INFORMATION)nibuf;
	ULONG retLength;

	status = ObQueryNameString( Object, nameInfo, sizeof(nibuf), &retLength);

	Name->Length = 0;
	if (NT_SUCCESS( status )) 
	{
		RtlCopyUnicodeString( Name, &nameInfo->Name );
	}
}


/***********************************************************************
* ��������:SfAttachToFileSystemDevice 
* ��������:���豸���ļ�ϵͳ
* �����б�:
*		DeviceObject:�ļ�ϵͳ�豸����
*		DeviceName:�ļ�ϵͳ�豸����
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfAttachToFileSystemDevice(
						   IN PDEVICE_OBJECT DeviceObject,
						   IN PUNICODE_STRING DeviceName
						   )
{
	NTSTATUS status = STATUS_SUCCESS;

	if (!IS_DESIRED_DEVICE_TYPE(DeviceObject->DeviceType))
	{
		// ����Ҫ���˵��ļ�ϵͳ
		return STATUS_SUCCESS;
	}

	// �ļ�ʶ���������а�
	UNICODE_STRING fsNameStr;
	WCHAR fsNameBuf[MY_DEV_MAX_NAME];
	RtlInitEmptyUnicodeString(&fsNameStr,fsNameBuf,sizeof(fsNameBuf));
	SfGetObjectName(DeviceObject->DriverObject,&fsNameStr);

	UNICODE_STRING fsrecNameStr;
	RtlInitUnicodeString(&fsrecNameStr,L"\\FileSystem\\Fs_Rec");
	
	if (RtlCompareUnicodeString(&fsNameStr,&fsrecNameStr,TRUE) == 0)
	{
		// ��ʶ�����������а�
		return STATUS_SUCCESS;
	}

	// �������豸
	PDEVICE_OBJECT newDeviceObject;
	status = IoCreateDevice(gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&newDeviceObject
		);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("SfAttachToFileSystemDevice:IoCreateDevice Failed\n"));
		return status;
	}

	// ���ñ�־λ
	if (FlagOn(DeviceObject->Flags,DO_BUFFERED_IO))
	{
		SetFlag(newDeviceObject->Flags,DO_BUFFERED_IO);
	}
	if (FlagOn(DeviceObject->Flags,DO_DIRECT_IO))
	{
		SetFlag(newDeviceObject->Flags,DO_DIRECT_IO);
	}
	if (FlagOn(DeviceObject->Flags,FILE_DEVICE_SECURE_OPEN))
	{
		SetFlag(newDeviceObject->Flags,FILE_DEVICE_SECURE_OPEN);
	}

	PSFILTER_DEVICE_EXTENSION devExt;
	devExt = (PSFILTER_DEVICE_EXTENSION)newDeviceObject->DeviceExtension;

	// ���豸
	status = SfAttachDeviceToDeviceStack(newDeviceObject,
		DeviceObject,
		&devExt->AttachedToDeviceObject);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("SfAttachToFileSystemDevice:SfAttachDeviceToDeviceStack Failed\n"));
		SfCleanupMountedDevice(newDeviceObject);
		IoDeleteDevice(newDeviceObject);
		return status;
	}

	// ������չ��Ϣ
	devExt->TypeFlag = POOL_TAG;
	devExt->DevType = DEV_FILESYSTEM;
	devExt->DevObj = DeviceObject;
	RtlInitEmptyUnicodeString(&devExt->DeviceName,
		devExt->DeviceNameBuffer,
		sizeof(devExt->DeviceNameBuffer));
	RtlCopyUnicodeString(&devExt->DeviceName,DeviceName);
	
	// ���ó�ʼ�����
	ClearFlag(newDeviceObject->Flags,DO_DEVICE_INITIALIZING);

	LOG_PRINT("Attached to File System %wZ ,File System Dev = %p ,MyDev = %p\n",
		&devExt->DeviceName,
		DeviceObject,
		newDeviceObject);
	
	// ö������ļ�ϵͳ�ϵ����о�
	status = SfEnumerateFileSystemVolumes(DeviceObject,DeviceName);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("SfAttachToFileSystemDevice:SfEnumerateFileSystemVolumes Failed\n"));
		IoDetachDevice(devExt->AttachedToDeviceObject);
		SfCleanupMountedDevice(newDeviceObject);
		IoDeleteDevice(newDeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}

/***********************************************************************
* ��������:SfEnumerateFileSystemVolumes 
* ��������:ö������ļ�ϵͳ�ϵ����о�
* �����б�:
*		FSDeviceObject:�ļ�ϵͳ�豸����
*		FSName:�ļ�ϵͳ�豸����
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfEnumerateFileSystemVolumes(
							 IN PDEVICE_OBJECT FSDeviceObject,
							 IN PUNICODE_STRING FSName
							 )
{
	NTSTATUS status = STATUS_SUCCESS;
	// ���������豸������Ŀ
	ULONG numDevices;
	PDEVICE_OBJECT *devList;
	status = IoEnumerateDeviceObjectList(FSDeviceObject->DriverObject,
		NULL,
		0,
		&numDevices);

	if (!NT_SUCCESS(status))
	{
		// ʧ�ܵ�ԭ��Ӧ������buf
		ASSERT(status == STATUS_BUFFER_TOO_SMALL);

		// �����ڴ�
		devList = (PDEVICE_OBJECT *)ExAllocatePoolWithTag(NonPagedPool,
			numDevices * sizeof(DEVICE_OBJECT),
			POOL_TAG);

		if (devList == NULL)
		{
			// �ڴ治��
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		// ����豸����
		status = IoEnumerateDeviceObjectList(FSDeviceObject->DriverObject,
			devList,
			numDevices * sizeof(DEVICE_OBJECT),
			&numDevices);

		// ����������ʵ�洢����
		PDEVICE_OBJECT storageStackDeviceObject;

		ULONG i;
		for (i = 0;i < numDevices;i++)
		{
			// �����ʵ�洢����
			storageStackDeviceObject = NULL;
			
			if ((devList[i] == FSDeviceObject) ||	// �豸�������Լ���ʱ������豸�����Ǹ������豸����
				(devList[i]->DeviceType != FSDeviceObject->DeviceType) ||	// ���ͺ��ļ�ϵͳ�����Ͳ���
				SfIsAttachedToDevice( devList[i], NULL )	// �Ѿ��󶨹����豸����
				)
			{
				// �����а�
				continue;
			}
			
			// �����ײ��豸����
			UNICODE_STRING name;
			WCHAR nameBuf[MY_DEV_MAX_NAME];
			RtlInitEmptyUnicodeString(&name,nameBuf,sizeof(nameBuf));
			SfGetBaseDeviceObjectName(devList[i],&name);
			
			//��������֣����Ǹ������豸�������а�
			if (name.Length > 0)
			{
				continue;
			}

			// ��ô����豸����
			status = IoGetDiskDeviceObject(devList[i],&storageStackDeviceObject);

			if (!NT_SUCCESS(status))
			{
				continue;
			}

			// ��Ӱ�豸�������ǡ�����

			// �������豸����
			PDEVICE_OBJECT newDeviceObject;
			status = IoCreateDevice(gSFilterDriverObject,
				sizeof(SFILTER_DEVICE_EXTENSION),
				NULL,
				devList[i]->DeviceType,
				0,
				FALSE,
				&newDeviceObject);
			if (!NT_SUCCESS(status))
			{
				continue;
			}

			// �����豸��չ��Ϣ
			PSFILTER_DEVICE_EXTENSION newDevExt;
			newDevExt = (PSFILTER_DEVICE_EXTENSION)newDeviceObject->DeviceExtension;
			newDevExt->TypeFlag = POOL_TAG;
			newDevExt->DevType = DEV_VOLUME;
			newDevExt->StorageStackDeviceObject = storageStackDeviceObject;
			newDevExt->DevObj = devList[i];
			// ��÷���������
			RtlInitEmptyUnicodeString(&newDevExt->SybDeviceName,
				newDevExt->SybDeviceNameBuffer,
				sizeof(newDevExt->SybDeviceNameBuffer));
			status = RtlVolumeDeviceToDosName(storageStackDeviceObject,&newDevExt->SybDeviceName);

			// ����豸��
			RtlInitEmptyUnicodeString(&newDevExt->DeviceName,
				newDevExt->DeviceNameBuffer,
				sizeof(newDevExt->DeviceNameBuffer));
			SfGetObjectName(storageStackDeviceObject,
				&newDevExt->DeviceName);

			// ��ʱ��Ҫ������ȥ���Ʒ���
			// ��û�����
			ExAcquireFastMutex(&gSfilterAttachLock);

			// ��������豸��Ӧ��û�б����ع�
			ASSERT(!SfIsAttachedToDevice(devList[i],NULL));

			// �󶨵�����
			SfAttachToMountedDevice(devList[i],newDeviceObject);

			if (!NT_SUCCESS(status))
			{
				// ʧ�ܵ�ԭ��ֻ�п���������Ǳ�����ոձ����أ���δ��ɳ��»�
				// ��ʱ����FlagsΪDO_DEVICE_INITIALIZING
				// ���Եȴ�һ��ʱ��֮���ٽ��а�
				SfCleanupMountedDevice(newDeviceObject);
				IoDeleteDevice(newDeviceObject);
			}

			// �ͷŻ�����
			ExReleaseFastMutex(&gSfilterAttachLock);

			// ��������1
			if (storageStackDeviceObject != NULL)
			{
				ObDereferenceObject(storageStackDeviceObject);
			}
			ObDereferenceObject( devList[i] );
		}

		// �Դ���������ȫ������STATUS_SUCCESS
		status = STATUS_SUCCESS;

		ExFreePoolWithTag(devList,POOL_TAG);
	}
	return status;
}

/***********************************************************************
* ��������:SfIsAttachedToDevice 
* ��������:�鿴����豸����Ĺ��ض���
* �����б�:
*		FSDeviceObject:�ļ�ϵͳ�豸����
*		FSName:�ļ�ϵͳ�豸����
* ����ֵ:�Ƿ�ɹ�
***********************************************************************/
BOOLEAN
SfIsAttachedToDevice(
					 PDEVICE_OBJECT DeviceObject,
					 PDEVICE_OBJECT *AttachedDeviceObject OPTIONAL
					 )
{
	NTSTATUS status;
	PDEVICE_OBJECT curDeivceObject;
	// ��ô��豸������豸
	curDeivceObject = IoGetAttachedDeviceReference(DeviceObject);

	do 
	{
		if (IS_MY_DEVICE_OBJECT(curDeivceObject))
		{
			// �Ѿ�������
			if (ARGUMENT_PRESENT(AttachedDeviceObject))
			{
				// �������Ĳ���NULL
				*AttachedDeviceObject = curDeivceObject;
			}
			else
			{
				// ����������NULL
				ObDereferenceObject( curDeivceObject );
			}
			return TRUE;
		}

		// �õ��²���豸
		PDEVICE_OBJECT nextDeviceObject;
		nextDeviceObject = IoGetLowerDeviceObject(curDeivceObject);
		ObDereferenceObject(curDeivceObject);

		curDeivceObject = nextDeviceObject;
	} while (curDeivceObject != NULL);

	// ������˵��û�б�����
	if (ARGUMENT_PRESENT(AttachedDeviceObject))
	{
		// ����δ�ҵ��豸����δ������
		*AttachedDeviceObject = NULL;
	}

	return FALSE;
}

/***********************************************************************
* ��������:SfGetBaseDeviceObjectName 
* ��������:�����ײ��豸����
* �����б�:
*		DeviceObject:�豸����
*		Name:����
* ����ֵ:��
***********************************************************************/
VOID
SfGetBaseDeviceObjectName (
						   IN PDEVICE_OBJECT DeviceObject,
						   IN OUT PUNICODE_STRING Name
						   )
{
	// �����ײ��豸����
	PDEVICE_OBJECT lowestDeviceObject;
	lowestDeviceObject = IoGetDeviceAttachmentBaseRef(DeviceObject);

	// �����������
	SfGetObjectName(lowestDeviceObject,Name);

	// ������1
	ObDereferenceObject(lowestDeviceObject);
}

/***********************************************************************
* ��������:SfDetachFromFileSystemDevice 
* ��������:ж���ļ�ϵͳ�İ�
* �����б�:
*		DeviceObject:�豸����
* ����ֵ:��
***********************************************************************/
VOID
SfDetachFromFileSystemDevice (
							  IN PDEVICE_OBJECT DeviceObject
							  )
{
	PDEVICE_OBJECT attachedDevice;
	PSFILTER_DEVICE_EXTENSION devExt;

	attachedDevice = DeviceObject->AttachedDevice;

	while(attachedDevice != NULL)
	{
		if (IS_MY_DEVICE_OBJECT(attachedDevice))
		{
			// ���������ɵ��豸����
			devExt = (PSFILTER_DEVICE_EXTENSION)attachedDevice->DeviceExtension;
			LOG_PRINT("Detach From File System %wZ\n",&devExt->DeviceName);

			SfCleanupMountedDevice(attachedDevice);
			IoDetachDevice(DeviceObject);
			IoDeleteDevice(attachedDevice);
			return;
		}
		// DeviceObjectһֱ����Ϊ�����ص��豸
		DeviceObject = attachedDevice;
		attachedDevice = attachedDevice->AttachedDevice;
	}
}

/***********************************************************************
* ��������:SfAttachToMountedDevice 
* ��������:���豸����
* �����б�:
*		DeviceObject:���豸����
*		SFilterDeviceObject:�����豸����
* ����ֵ:��
***********************************************************************/
NTSTATUS
SfAttachToMountedDevice (
						 IN PDEVICE_OBJECT DeviceObject,
						 IN PDEVICE_OBJECT SFilterDeviceObject
						 )
{
	NTSTATUS status;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	newDevExt = (PSFILTER_DEVICE_EXTENSION)SFilterDeviceObject->DeviceExtension;

	// Ӧ����δ���󶨵��豸
	ASSERT(!SfIsAttachedToDevice ( DeviceObject, NULL ));

	// �鿴�Ƿ���Ҫ��
	if (!OnSfilterAttachPre(SFilterDeviceObject,DeviceObject,NULL))
	{
		// ����Ҫ��
		return STATUS_UNSUCCESSFUL;
	}

	if (FlagOn( DeviceObject->Flags, DO_BUFFERED_IO )) 
	{
		SetFlag( SFilterDeviceObject->Flags, DO_BUFFERED_IO );
	}

	if (FlagOn( DeviceObject->Flags, DO_DIRECT_IO )) 
	{
		SetFlag( SFilterDeviceObject->Flags, DO_DIRECT_IO );
	}

	// Ϊ�˷�ֹ�󶨲��ɹ�����ձ����أ���8�Σ����һ��ʱ��
	ULONG i;
	for (i = 0;i < 8;i++)
	{
		LARGE_INTEGER interval;
		status = SfAttachDeviceToDeviceStack(SFilterDeviceObject,
			DeviceObject,
			&newDevExt->AttachedToDeviceObject);

		if (NT_SUCCESS(status))
		{
			// �󶨳ɹ������ûص�����
			OnSfilterAttachPost(SFilterDeviceObject,
				DeviceObject,
				newDevExt->AttachedToDeviceObject,
				status);

			// ����Ϊ��ʼ�����
			ClearFlag(SFilterDeviceObject->Flags,DO_DEVICE_INITIALIZING);

			LOG_PRINT("Attached To Volume %wZ(%wZ) ,MountDev = %p ,MyDev = %p\n",
				&newDevExt->DeviceName,
				&newDevExt->SybDeviceName,
				DeviceObject,
				SFilterDeviceObject);

			return STATUS_SUCCESS;
		}

		// ��ʱ0.5������
		interval.QuadPart = (500 * DELAY_ONE_MILLISECOND);
		KeDelayExecutionThread( KernelMode, FALSE, &interval );
	}
	
	// ʧ�ܵ��ûص�����
	OnSfilterAttachPost(SFilterDeviceObject,
		DeviceObject,
		newDevExt->AttachedToDeviceObject,
		status);

	return status;
}

/***********************************************************************
* ��������:SfFsControlMountVolume 
* ��������:�������ؾ�IRP�ĵ��ú���
* �����б�:
*		DeviceObject:���豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfFsControlMountVolume (
						IN PDEVICE_OBJECT DeviceObject,
						IN PIRP Irp
						)
{
	NTSTATUS status;

	// ��õ�ǰջ��ַ
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));
	ASSERT(IS_DESIRED_DEVICE_TYPE(DeviceObject->DeviceType));

	// ������ʵ�豸��ַ���Ժ�����
	PDEVICE_OBJECT storageStackDeviceObject;
	storageStackDeviceObject = irpSp->Parameters.MountVolume.Vpb->RealDevice;

	// �����µ��豸
	PDEVICE_OBJECT newDeviceObject;
	status = IoCreateDevice(gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&newDeviceObject);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("SfFsControlMountVolume IoCreateDevice Failed"));
		// ����ʧ����Ϣ�����IRP
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp,IO_NO_INCREMENT);
		return status;
	}

	// �����豸��չ��Ϣ
	PSFILTER_DEVICE_EXTENSION newDevExt = (PSFILTER_DEVICE_EXTENSION)newDeviceObject->DeviceExtension;
	newDevExt->StorageStackDeviceObject = storageStackDeviceObject;
	newDevExt->TypeFlag = POOL_TAG;
	RtlInitEmptyUnicodeString(&newDevExt->DeviceName,
		newDevExt->DeviceNameBuffer,
		sizeof(newDevExt->DeviceNameBuffer));
	SfGetObjectName(storageStackDeviceObject,
		&newDevExt->DeviceName);

	// ����IRP������̣����ҵȴ������
	KEVENT waitEvent;
	KeInitializeEvent(&waitEvent,
		NotificationEvent,
		FALSE);

	IoCopyCurrentIrpStackLocationToNext(Irp);

	IoSetCompletionRoutine(Irp,
		SfFsControlCompletion,	// �������
		&waitEvent,		// ����ȥ�Ĳ���
		TRUE,
		TRUE,
		TRUE);

	status = IoCallDriver(devExt->AttachedToDeviceObject,
		Irp);

	// �ȴ����
	if (status = STATUS_PENDING)
	{
		status = KeWaitForSingleObject(&waitEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);
	}

	// �������ɣ����ûص�����
	status = SfFsControlMountVolumeComplete(DeviceObject,
		Irp,
		newDeviceObject);

	return status;
}

/***********************************************************************
* ��������:SfFsControlCompletion 
* ��������:IRP��ɵĻص�����
* �����б�:
*		DeviceObject:���豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfFsControlCompletion(
					  IN PDEVICE_OBJECT DeviceObject,
					  IN PIRP Irp,
					  IN PVOID Context
					  )
{
	// ����ʱ����ɣ��ص�����IRP�д���
	KeSetEvent((PKEVENT)Context,
		IO_NO_INCREMENT,
		FALSE);
	// ����Ϊ��Ҫ����Ĵ���
	return STATUS_MORE_PROCESSING_REQUIRED;
}

/***********************************************************************
* ��������:SfFsControlMountVolumeComplete 
* ��������:���ؾ���ɺ����
* �����б�:
*		DeviceObject:ԭ�豸����
*		Irp:IRP
*		NewDeviceObject:�����豸����
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfFsControlMountVolumeComplete (
								IN PDEVICE_OBJECT DeviceObject,
								IN PIRP Irp,
								IN PDEVICE_OBJECT NewDeviceObject
								)
{
	NTSTATUS status;
	PSFILTER_DEVICE_EXTENSION newDevExt = (PSFILTER_DEVICE_EXTENSION)NewDeviceObject->DeviceExtension;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PVPB vpb = newDevExt->StorageStackDeviceObject->Vpb;

	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		// IRP�ǳɹ���
		// Ϊ�˷�ֹ�����Σ�ʹ�û�����
		ExAcquireFastMutex(&gSfilterAttachLock);

		PDEVICE_OBJECT attachedDeviceObject;
		if (!SfIsAttachedToDevice(vpb->DeviceObject,&attachedDeviceObject))
		{
			// δ�󶨹����󶨵���
			status = SfAttachToMountedDevice(vpb->DeviceObject,NewDeviceObject);

			if (!NT_SUCCESS(status))
			{
				// ��ʧ��
				SfCleanupMountedDevice(NewDeviceObject);
				IoDeleteDevice(NewDeviceObject);
			}
		}
		else
		{
			// �Ѿ��󶨹���
			SfCleanupMountedDevice(NewDeviceObject);
			IoDeleteDevice(NewDeviceObject);
			ObDereferenceObject(attachedDeviceObject);
		}
		ExReleaseFastMutex(&gSfilterAttachLock);
	}
	else
	{
		// IRPʧ��
		SfCleanupMountedDevice(NewDeviceObject);
		IoDeleteDevice(NewDeviceObject);
	}

	status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return status;
}

/***********************************************************************
* ��������:SfFsControlLoadFileSystem 
* ��������:����������ʵ�ļ�ϵͳIRPʱ����
* �����б�:
*		DeviceObject:���豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfFsControlLoadFileSystem (
						   IN PDEVICE_OBJECT DeviceObject,
						   IN PIRP Irp
						   )
{
	NTSTATUS status;
	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	LOG_PRINT("Loading File System, Detaching from %wZ \n",&devExt->DeviceName);

	// �����¼����ȴ������
	KEVENT waitEvent;

	KeInitializeEvent( &waitEvent, 
		NotificationEvent, 
		FALSE );

	IoCopyCurrentIrpStackLocationToNext(Irp);

	IoSetCompletionRoutine(Irp,
		SfFsControlCompletion,
		&waitEvent,
		TRUE,
		TRUE,
		TRUE );

	status = IoCallDriver( devExt->AttachedToDeviceObject, Irp );

	if (STATUS_PENDING == status) 
	{
		status = KeWaitForSingleObject( &waitEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL );
	}

	status = SfFsControlLoadFileSystemComplete( DeviceObject,
		Irp );
	return status;
}

/***********************************************************************
* ��������:SfFsControlLoadFileSystemComplete 
* ��������:����������ʵ�ļ�ϵͳIRP��ɺ�
* �����б�:
*		DeviceObject:���豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfFsControlLoadFileSystemComplete (
								   IN PDEVICE_OBJECT DeviceObject,
								   IN PIRP Irp
								   )
{
	NTSTATUS status;
	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	LOG_PRINT("Detaching from recognizer %wZ \n",&devExt->DeviceName);

	if (!NT_SUCCESS( Irp->IoStatus.Status ) && (Irp->IoStatus.Status != STATUS_IMAGE_ALREADY_LOADED))
	{
		// ����ʧ�ܣ������ٴι��ص��ļ�ʶ������
		SfAttachDeviceToDeviceStack( DeviceObject, 
			devExt->AttachedToDeviceObject,
			&devExt->AttachedToDeviceObject );
	}
	else
	{
		// ���سɹ�
		SfCleanupMountedDevice(DeviceObject);
		IoDeleteDevice(DeviceObject);
	}

	status = Irp->IoStatus.Status;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return status;
}

/***********************************************************************
* ��������:SfAddDirToTable 
* ��������:��¼�򿪵�Ŀ¼
* �����б�:
*		file:�ļ�����
* ����ֵ:״ֵ̬
***********************************************************************/
VOID
SfAddDirToTable(PFILE_OBJECT file)
{
	
	if (gListHead == NULL)
	{
		// ����Ϊ��
		gListHead = (PDIR_NODE)ExAllocatePoolWithTag(PagedPool,
			sizeof(DIR_NODE),
			POOL_TAG);
		RtlInitEmptyUnicodeString(&gListHead->DirPathStr,
			gListHead->DirPathBuf,
			sizeof(gListHead->DirPathBuf));

		RtlCopyUnicodeString(&gListHead->DirPathStr,&file->FileName);

		gListHead->referenceNum = 1;

		gListHead->nextNode = NULL;
		return;
	}

	PDIR_NODE nextDir = gListHead;
	while(nextDir->nextNode != NULL)
	{
		if (RtlCompareUnicodeString(&nextDir->DirPathStr,&file->FileName,TRUE) == 0)
		{
			// �����д��ڴ�Ŀ¼
			nextDir->referenceNum++;
			return;
		}
		nextDir = nextDir->nextNode;
	}

	if (RtlCompareUnicodeString(&nextDir->DirPathStr,&file->FileName,TRUE) == 0)
	{
		// �����д��ڴ�Ŀ¼
		nextDir->referenceNum++;
		return;
	}

	// �����ڴ�Ŀ¼������½ڵ�
	nextDir->nextNode = (PDIR_NODE)ExAllocatePoolWithTag(PagedPool,
		sizeof(DIR_NODE),
		POOL_TAG);
	nextDir = nextDir->nextNode;
	RtlInitEmptyUnicodeString(&nextDir->DirPathStr,
		nextDir->DirPathBuf,
		sizeof(nextDir->DirPathBuf));

	RtlCopyUnicodeString(&nextDir->DirPathStr,&file->FileName);

	nextDir->referenceNum = 1;

	nextDir->nextNode = NULL;

	return;
}

/***********************************************************************
* ��������:SfDelDirFromTable 
* ��������:ɾ���򿪵�Ŀ¼��¼
* �����б�:
*		file:�ļ�����
* ����ֵ:״ֵ̬
***********************************************************************/
VOID 
SfDelDirFromTable(PFILE_OBJECT file)
{
	if (gListHead == NULL)
	{
		// ����Ϊ��
		return;
	}

	PDIR_NODE nextDir = gListHead;
	PDIR_NODE preDir = NULL;
	while(nextDir != NULL)
	{
		if (RtlCompareUnicodeString(&nextDir->DirPathStr,&file->FileName,TRUE) == 0)
		{
			// �ҵ�Ŀ¼
			if (preDir == NULL)
			{
				// ����ĵ�һ��Ԫ��
				nextDir->referenceNum--;
				if (nextDir->referenceNum == 0)
				{
					// �������ˣ�����ɾ���ڵ�
					gListHead = gListHead->nextNode;
					ExFreePoolWithTag(nextDir,POOL_TAG);
				}
			}
			else
			{
				nextDir->referenceNum--;
				if (nextDir->referenceNum == 0)
				{
					// �������ˣ�����ɾ���ڵ�
					preDir->nextNode = nextDir->nextNode;
					ExFreePoolWithTag(nextDir,POOL_TAG);
				}
			}
			return;
		}
		// �������Ŀ¼
		preDir = nextDir;
		nextDir = nextDir->nextNode;
	}
	return;
}

/***********************************************************************
* ��������:SfGetFileName 
* ��������:����������ļ���
* �����б�:
*		FileObject:�򿪵��ļ�����
*		DeviceObject:ʹ�õ��豸����
*		FileName:���ص��ļ���
* ����ֵ:״ֵ̬
* FileName�ڴ��ݽ���֮ǰӦ���Ѿ�������ڴ�
***********************************************************************/
VOID
SfGetFileName(
			  IN PFILE_OBJECT FileObject,
			  IN PDEVICE_OBJECT DeviceObject,
			  OUT PUNICODE_STRING FileName
			  )
{
	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	RtlCopyUnicodeString(FileName,&devExt->DeviceName);
	RtlAppendUnicodeStringToString(FileName,&FileObject->FileName);
}

/***********************************************************************
* ��������:SfGetFileLinkName 
* ��������:��������ķ��������ļ���
* �����б�:
*		FileObject:�򿪵��ļ�����
*		DeviceObject:ʹ�õ��豸����
*		FileName:���ص��ļ���
* ����ֵ:״ֵ̬
* FileName�ڴ��ݽ���֮ǰӦ���Ѿ�������ڴ�
***********************************************************************/
VOID
SfGetFileLinkName(
				  IN PFILE_OBJECT FileObject,
				  IN PDEVICE_OBJECT DeviceObject,
				  OUT PUNICODE_STRING FileName
				  )
{

	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	RtlCopyUnicodeString(FileName,&devExt->SybDeviceName);
	RtlAppendUnicodeStringToString(FileName,&FileObject->FileName);
}