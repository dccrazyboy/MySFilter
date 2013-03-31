#include <ntifs.h>
#include "BaseDef.h"
#include "BaseFun.h"
#include "MajorFunction.h"
#include "MySFilter.h"
// ����������ȫ��ָ��
extern PDRIVER_OBJECT gSFilterDriverObject;

// CDO��ȫ��ָ��
extern PDEVICE_OBJECT gSFilterControlDeviceObject;

/***********************************************************************
* ��������:SfPassThrough 
* ��������:���账���IRP��ֱ���·�
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS 
SfPassThrough(
			  IN PDEVICE_OBJECT DeviceObject,
			  IN PIRP Irp
			  )
{
	// CDO��Ӧ�ý����κ�IRP
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject));

	// �豸����Ӧ���Ǳ��������󴴽���
	ASSERT(IS_MY_DEVICE_OBJECT(DeviceObject));

	// �򵥵�������ǰ��,���·�IRP
	IoSkipCurrentIrpStackLocation(Irp);

	return IoCallDriver(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject,Irp);
}

/***********************************************************************
* ��������:SfFsNotification 
* ��������:���ļ�ϵͳ�����仯ʱ������
* �����б�:
*		DeviceObject:�豸����
*		FsActive:������߹ر�
* ����ֵ:״ֵ̬
***********************************************************************/
VOID
SfFsNotification (
				  IN PDEVICE_OBJECT DeviceObject,
				  IN BOOLEAN FsActive
				  )
{

	PAGED_CODE();

	// name���������豸����
	UNICODE_STRING name;
	WCHAR nameBuf[MY_DEV_MAX_NAME];

	PAGED_CODE();

	RtlInitEmptyUnicodeString(&name,nameBuf,sizeof(nameBuf));

	SfGetObjectName(DeviceObject,&name);

	LOG_PRINT("SfFsNotification: %s %wZ \n",
		FsActive ? "Activating file system" : "Deactivating file system",
		&name
		);

	// ����Ǽ�����ļ�ϵͳ
	// ����ǹرգ������
	if (FsActive)
	{
		SfAttachToFileSystemDevice(DeviceObject,&name);
	}
	else
	{
		SfDetachFromFileSystemDevice(DeviceObject);
	}
}

/***********************************************************************
* ��������:SfFsControl 
* ��������:���յ�IRP_MJ_FILE_SYSTEM_CONTROLʱ���ã������Ĺ�����Ϣ
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfFsControl(
			IN PDEVICE_OBJECT DeviceObject,
			IN PIRP Irp
			)
{
	// ��õ�ǰIRPջָ��
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject));
	ASSERT(IS_MY_DEVICE_OBJECT(DeviceObject));

	switch (irpSp->MinorFunction)
	{
	case IRP_MN_MOUNT_VOLUME:
		// ������
		return SfFsControlMountVolume(DeviceObject,Irp);
	case IRP_MN_LOAD_FILE_SYSTEM:
		// ������ʵ�ļ�ϵͳ
		return SfFsControlLoadFileSystem( DeviceObject, Irp );
	case IRP_MN_USER_FS_REQUEST:
		{
			switch (irpSp->Parameters.FileSystemControl.FsControlCode)
			{
			case FSCTL_DISMOUNT_VOLUME:
				// ж�ؾ���ɾ�������豸�������κδ���û��Ӱ��
				PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
				LOG_PRINT("DisAttached From Volume %wZ \n",&devExt->DeviceName);
			}
		}
	}
	
	// ��Ŀ���IRP����������
	return SfPassThrough(DeviceObject,Irp);
}

/***********************************************************************
* ��������:SfCreate 
* ��������:���յ�IRP_MJ_CREATEʱ���ã�������ļ�IRP
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfCreate(
		 IN PDEVICE_OBJECT DeviceObject,
		 IN PIRP Irp
		 )
{
	NTSTATUS status;
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject));
	ASSERT(IS_MY_DEVICE_OBJECT(DeviceObject));
	SF_RET ret;
	ret = OnSfilterIrpPre(DeviceObject,Irp);
	if (ret == SF_IRP_PASS)
	{
		// ������ȥ�������й���
		return SfPassThrough(DeviceObject,Irp);
	}
	else
	{
		// ������ȥ�����й���
		// ������ǰIRP
		IoCopyCurrentIrpStackLocationToNext(Irp);

		// ������ɺ�Ļص�����
		IoSetCompletionRoutine(Irp,
			SfCreateCompletion,
			NULL,
			TRUE,
			TRUE,
			TRUE);

		status = IoCallDriver(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject,
			Irp);
		OnSfilterIrpPost(DeviceObject,Irp);
		return status;
	}
}

/***********************************************************************
* ��������:SfCreateCompletion 
* ��������:IRP_MJ_CREATE���ʱ�Ļص�����
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfCreateCompletion(
				   IN PDEVICE_OBJECT DeviceObject,
				   IN PIRP Irp,
				   IN PVOID Context
				   )
{
//	_asm int 3
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT file = irpSp->FileObject;

	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		ASSERT(file != NULL);
		UNICODE_STRING name;
		WCHAR nameBuf[1024];
		RtlInitEmptyUnicodeString(&name,nameBuf,sizeof(nameBuf));

		// ���������·����ʹ���豸��������
	/*	SfGetFileName( irpSp->FileObject, 
			DeviceObject,
			&name);
	*/
		// ���������·����ʹ�÷�����������
		SfGetFileLinkName( irpSp->FileObject, 
			DeviceObject,
			&name);
		// �����������������ַ����к������ģ�UNICODE_STRING�ᱻ�ض�
		// ANSI_STRINGû��Ӱ�졣����
		ANSI_STRING aName;
		RtlUnicodeStringToAnsiString(&aName,&name,TRUE);


		/*
		// ����ļ�\Ŀ¼�жϲ�̫׼ȷ���Ժ����о�
		if ((irpSp->Parameters.Create.Options & FILE_DIRECTORY_FILE) != 0)
		{
			// �򿪵���Ŀ¼
			LOG_PRINT("Open Succeed Dir name = %Z\n",&aName);
			//	SfAddDirToTable(file);
		}
		else
		{
			// �򿪵����ļ�
			LOG_PRINT("Open Succeed File name = %Z\n",&aName);
		}
		*/

		if (Irp->IoStatus.Status == FILE_CREATED)
		{
			LOG_PRINT("Create Succeed! name = %Z\n",&aName);
		}
		else
		{
			LOG_PRINT("Opened Succeed! name = %Z\n",&aName);
		}
		
		// �ͷſռ�
		RtlFreeAnsiString(&aName);
	}
	return Irp->IoStatus.Status;
}

/***********************************************************************
* ��������:SfCleanupClose 
* ��������:IRP_MJ_CLOSE�ĵ��ú���
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfCleanupClose(
			   IN PDEVICE_OBJECT DeviceObject,
			   IN PIRP Irp
			   )
{
	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject));
	ASSERT(IS_MY_DEVICE_OBJECT( DeviceObject ));

	NTSTATUS status;
	SF_RET ret;
	ret = OnSfilterIrpPre(DeviceObject,Irp);
	if (ret == SF_IRP_PASS)
	{
		// ������ȥ�������й���
		return SfPassThrough(DeviceObject,Irp);
	}
	else
	{
		// ������ȥ�����й���
		
		PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
		PFILE_OBJECT file = irpSp->FileObject;
		UNICODE_STRING name;
		WCHAR nameBuf[1024];
		RtlInitEmptyUnicodeString(&name,nameBuf,sizeof(nameBuf));

		// ���������·����ʹ���豸��������
	/*	SfGetFileName( irpSp->FileObject, 
			DeviceObject,
			&name);
	*/
		// ���������·����ʹ�÷�����������
		SfGetFileLinkName( irpSp->FileObject, 
			DeviceObject,
			&name);
		// �����������������ַ����к������ģ�UNICODE_STRING�ᱻ�ض�
		// ANSI_STRINGû��Ӱ�졣����
		ANSI_STRING aName;
		RtlUnicodeStringToAnsiString(&aName,&name,TRUE);
		LOG_PRINT("Close Succeed! name = %Z\n",&aName);
		// �ͷſռ�
		RtlFreeAnsiString(&aName);

		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject,
			Irp);
		OnSfilterIrpPost(DeviceObject,Irp);
		return status;
	}
}

/***********************************************************************
* ��������:SfRead 
* ��������:IRP_MJ_READ�ĵ��ú���
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfRead(
	   IN PDEVICE_OBJECT DeviceObject,
	   IN PIRP Irp
	   )
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT file = irpSp->FileObject;
	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject));

	// �����ļ�ϵͳ��������
	if (devExt->DevType == DEV_FILESYSTEM)
	{
		return SfPassThrough(DeviceObject,Irp);
	}

	SF_RET ret;
	ret = OnSfilterIrpPre(DeviceObject,Irp);
	if (ret == SF_IRP_GO_ON)
	{
		// ���й���
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp,
			SfReadCompletion,
			NULL,
			TRUE,
			TRUE,
			TRUE);
		status = IoCallDriver(devExt->AttachedToDeviceObject,
			Irp);
		OnSfilterIrpPost(DeviceObject,Irp);
		return status;
	}
	else
	{
		// �����й���
		return SfPassThrough(DeviceObject,Irp);
	}
}

/***********************************************************************
* ��������:SfReadCompletion 
* ��������:IRP_MJ_READ��ɵĻص�����
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
*		Context:����ָ��
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfReadCompletion(
				 IN PDEVICE_OBJECT DeviceObject,
				 IN PIRP Irp,
				 IN PVOID Context
				 )
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		// ��ȡ�ɹ�
		VOID *Buf;
		ULONG len;
		if (Irp->MdlAddress != NULL)
		{
			Buf = MmGetSystemAddressForMdl(Irp->MdlAddress);
		}
		else
		{
			Buf = Irp->UserBuffer;
		}
		ASSERT(Buf != NULL);
		len = Irp->IoStatus.Information;

		// �����Ϣ
		UNICODE_STRING name;
		WCHAR nameBuf[1024];
		RtlInitEmptyUnicodeString(&name,nameBuf,sizeof(nameBuf));

		// ���������·����ʹ���豸��������
	/*	SfGetFileName( irpSp->FileObject, 
			DeviceObject,
			&name);
	*/
		// ���������·����ʹ�÷�����������
		SfGetFileLinkName( irpSp->FileObject, 
			DeviceObject,
			&name);
		// �����������������ַ����к������ģ�UNICODE_STRING�ᱻ�ض�
		// ANSI_STRINGû��Ӱ�졣����
		ANSI_STRING aName;
		RtlUnicodeStringToAnsiString(&aName,&name,TRUE);

		// ���ƫ����
		LARGE_INTEGER offset;
		offset.QuadPart = irpSp->Parameters.Read.ByteOffset.QuadPart;
		LOG_PRINT("Read Succeed! Name = %Z,Offset = %x%x, Len = %d, Buf = 0x%p\n",
			&aName,
			offset.HighPart,
			offset.LowPart,
			len,
			Buf);
	}
	return Irp->IoStatus.Status;
}

/***********************************************************************
* ��������:SfWrite 
* ��������:IRP_MJ_WRITE�ĵ��ú���
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS
SfWrite(
		IN PDEVICE_OBJECT DeviceObject,
		IN PIRP Irp
		)
{
	NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT file = irpSp->FileObject;
	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject));

	// �����ļ�ϵͳ��������
	if (devExt->DevType == DEV_FILESYSTEM)
	{
		return SfPassThrough(DeviceObject,Irp);
	}

	SF_RET ret;
	ret = OnSfilterIrpPre(DeviceObject,Irp);
	if (ret == SF_IRP_GO_ON)
	{
		// ���й���
		VOID *Buf;
		ULONG len;
		if (Irp->MdlAddress != NULL)
		{
			Buf = MmGetSystemAddressForMdl(Irp->MdlAddress);
		}
		else
		{
			Buf = Irp->UserBuffer;
		}
		ASSERT(Buf != NULL);
		len = irpSp->Parameters.Write.Length;

		// �����Ϣ
		UNICODE_STRING name;
		WCHAR nameBuf[1024];
		RtlInitEmptyUnicodeString(&name,nameBuf,sizeof(nameBuf));

		// ���������·����ʹ���豸��������
	/*	SfGetFileName( irpSp->FileObject, 
			DeviceObject,
			&name);
	*/
		// ���������·����ʹ�÷�����������
		SfGetFileLinkName( irpSp->FileObject, 
			DeviceObject,
			&name);
		// �����������������ַ����к������ģ�UNICODE_STRING�ᱻ�ض�
		// ANSI_STRINGû��Ӱ�졣����
		ANSI_STRING aName;
		RtlUnicodeStringToAnsiString(&aName,&name,TRUE);

		// ���ƫ����
		LARGE_INTEGER offset;
		offset.QuadPart = irpSp->Parameters.Write.ByteOffset.QuadPart;
		LOG_PRINT("Write Succeed! Name = %Z,Offset = %x%x, Len = %d, Buf = 0x%p\n",
			&aName,
			offset.HighPart,
			offset.LowPart,
			len,
			Buf);
		// ���´���IRP
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(devExt->AttachedToDeviceObject,
			Irp);
		OnSfilterIrpPost(DeviceObject,Irp);
		return status;
	}
	else
	{
		// �����й���
		return SfPassThrough(DeviceObject,Irp);
	}
}

/***********************************************************************
* ��������:SfSetInfo 
* ��������:IRP_MJ_SET_INFORMATION�ĵ��ú���
* �����б�:
*		DeviceObject:�豸����
*		Irp:IRP
* ����ֵ:״ֵ̬
* ע:������Ҫ���ɾ���ļ�
***********************************************************************/
NTSTATUS
SfSetInfo(
		  IN PDEVICE_OBJECT DeviceObject,
		  IN PIRP Irp
		  )
{

	NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT file = irpSp->FileObject;
	PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	ASSERT(!IS_MY_CONTROL_DEVICE_OBJECT(DeviceObject));

	// �����ļ�ϵͳ��������
	if (devExt->DevType == DEV_FILESYSTEM)
	{
		return SfPassThrough(DeviceObject,Irp);
	}

	SF_RET ret;
	ret = OnSfilterIrpPre(DeviceObject,Irp);
	if (ret == SF_IRP_GO_ON)
	{
		if (irpSp->Parameters.SetFile.FileInformationClass == FileDispositionInformation && 
			*((BOOLEAN *)Irp->AssociatedIrp.SystemBuffer) == TRUE)
		{
			// ������ɾ��״̬
			// �����Ϣ
			UNICODE_STRING name;
			WCHAR nameBuf[1024];
			RtlInitEmptyUnicodeString(&name,nameBuf,sizeof(nameBuf));

			// ���������·����ʹ���豸��������
		/*	SfGetFileName( irpSp->FileObject, 
				DeviceObject,
				&name);
		*/
			// ���������·����ʹ�÷�����������
			SfGetFileLinkName( irpSp->FileObject, 
				DeviceObject,
				&name);
			// �����������������ַ����к������ģ�UNICODE_STRING�ᱻ�ض�
			// ANSI_STRINGû��Ӱ�졣����
			ANSI_STRING aName;
			RtlUnicodeStringToAnsiString(&aName,&name,TRUE);

			LOG_PRINT("Delete Succeed! Name = %Z\n",
				&aName);
		}
		// ���´���IRP
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(devExt->AttachedToDeviceObject,
			Irp);
		OnSfilterIrpPost(DeviceObject,Irp);
		return status;
	}
	else
	{
		// �����й���
		return SfPassThrough(DeviceObject,Irp);
	}
}
