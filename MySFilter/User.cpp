#include <ntifs.h>
#include "MySFilter.h"
#include "BaseFun.h"
WCHAR MajorFunTable[][50] = {
L"IRP_MJ_CREATE",
L"IRP_MJ_CREATE_NAMED_PIPE",
L"IRP_MJ_CLOSE",
L"IRP_MJ_READ",
L"IRP_MJ_WRITE",
L"IRP_MJ_QUERY_INFORMATION",
L"IRP_MJ_SET_INFORMATION",
L"IRP_MJ_QUERY_EA",
L"IRP_MJ_SET_EA",
L"IRP_MJ_FLUSH_BUFFERS",
L"IRP_MJ_QUERY_VOLUME_INFORMATION",
L"IRP_MJ_SET_VOLUME_INFORMATION",
L"IRP_MJ_DIRECTORY_CONTROL",
L"IRP_MJ_FILE_SYSTEM_CONTROL",
L"IRP_MJ_DEVICE_CONTROL",
L"IRP_MJ_INTERNAL_DEVICE_CONTROL",
L"IRP_MJ_SHUTDOWN",
L"IRP_MJ_LOCK_CONTROL",
L"IRP_MJ_CLEANUP",
L"IRP_MJ_CREATE_MAILSLOT",
L"IRP_MJ_QUERY_SECURITY",
L"IRP_MJ_SET_SECURITY",
L"IRP_MJ_POWER",
L"IRP_MJ_SYSTEM_CONTROL",
L"IRP_MJ_DEVICE_CHANGE",
L"IRP_MJ_QUERY_QUOTA",
L"IRP_MJ_SET_QUOTA",
L"IRP_MJ_PNP"
};
/***********************************************************************
* ��������:OnSfilterDriverEntry 
* ��������:DriverEntry�Ļص���������д�豸���ͷ���������
* �����б�:
*		DriverObject:��������
*		RegistryPath:ע���·��
*		userNameString:�豸��
*		syblnkString:����������
* ����ֵ:״ֵ̬
***********************************************************************/
NTSTATUS OnSfilterDriverEntry(
							  IN PDRIVER_OBJECT DriverObject,
							  IN PUNICODE_STRING RegistryPath,
							  OUT PUNICODE_STRING userNameString,
							  OUT PUNICODE_STRING syblnkString
							  )
{
	NTSTATUS status = STATUS_SUCCESS;

	// ȷ�������豸�����ֺͷ������ӡ�
	UNICODE_STRING user_name,syb_name;
	RtlInitUnicodeString(&user_name,L"MySFilterCDO");
	RtlInitUnicodeString(&syb_name,L"MySFilterCDOSyb");
	RtlCopyUnicodeString(userNameString,&user_name);
	RtlCopyUnicodeString(syblnkString,&syb_name);

	return status;
}

/***********************************************************************
* ��������:OnSfilterDriverUnload 
* ��������:DriverUnload�Ļص�����
* �����б�:
*		��
* ����ֵ:��
***********************************************************************/
VOID OnSfilterDriverUnload()
{
	// ûʲôҪ����...
}

/***********************************************************************
* ��������:OnSfilterAttachPre 
* ��������:�ж��Ƿ�󶨴��豸
* �����б�:
*		ourDevice:�����豸
*		theDeviceToAttach:��Ҫ�����豸
*		DeviceName:�����豸���豸����
* ����ֵ:״ֵ̬
***********************************************************************/
BOOLEAN OnSfilterAttachPre(
						   IN PDEVICE_OBJECT ourDevice,
						   IN PDEVICE_OBJECT theDeviceToAttach,
						   IN PUNICODE_STRING DeviceName
						   )
{
	// ���е��豸����
	return TRUE;
}

/***********************************************************************
* ��������:OnSfilterAttachPost 
* ��������:�󶨳ɹ�֮��Ļص�����
* �����б�:
*		ourDevice:�����豸
*		theDeviceToAttach:��Ҫ�����豸
*		theDeviceToAttached:�����豸
*		status:��״̬
* ����ֵ:״ֵ̬
***********************************************************************/
VOID OnSfilterAttachPost(
						 IN PDEVICE_OBJECT ourDevice,
						 IN PDEVICE_OBJECT theDeviceToAttach,
						 IN PDEVICE_OBJECT theDeviceToAttached,
						 IN NTSTATUS status)
{
	// ���账��
}

/***********************************************************************
* ��������:OnSfilterIrpPre 
* ��������:�ڴ���IRP֮ǰ�ĵ��ú���
* �����б�:
*		DeviceObject:��ǰ�豸
*		Irp:IRP
* ����ֵ:�Ƿ����
***********************************************************************/
SF_RET OnSfilterIrpPre(
					   IN PDEVICE_OBJECT DeviceObject,
					   IN PIRP Irp)
{
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT file = irpSp->FileObject;

	if (file == NULL)
	{
		// �����ļ����󣬲�������
		return SF_IRP_PASS;
	}

	// ���ļ�����


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

	UNICODE_STRING filterPathStr;
	RtlInitUnicodeString(&filterPathStr,L"C:\\123\\");
	
	USHORT oldLen = name.Length;
	name.Length = filterPathStr.Length;

	if (RtlCompareUnicodeString(&name,&filterPathStr,TRUE) == 0)
	{
		// ��Ҫ���˵�Ŀ¼
		name.Length = oldLen;
	/*	KdPrint(("SF_IRP_GO_ON\n"));
		// �����������������ַ����к������ģ�UNICODE_STRING�ᱻ�ض�
		// ANSI_STRINGû��Ӱ�졣����
			ANSI_STRING aName;
		RtlUnicodeStringToAnsiString(&aName,&file->FileName,TRUE);

		LOG_PRINT("IRP: file name = %Z, major function = %S\n",
			&aName,
			MajorFunTable[irpSp->MajorFunction]);
		RtlFreeAnsiString(&aName);
	*/
	
		return SF_IRP_GO_ON;
	}
	name.Length = oldLen;
	return SF_IRP_PASS;
}

/***********************************************************************
* ��������:OnSfilterIrpPost 
* ��������:�ڴ���IRP֮��ĵ��ú���
* �����б�:
*		DeviceObject:��ǰ�豸
*		Irp:IRP
* ����ֵ:��
***********************************************************************/
VOID OnSfilterIrpPost(
					  IN PDEVICE_OBJECT DeviceObject,
					  IN PIRP Irp)
{
	return;
}