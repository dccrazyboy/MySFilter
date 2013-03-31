#ifndef _BASEDEF_H
#define _BASEDEF_H

#define MY_DEV_MAX_NAME 128
#define MY_DEV_MAX_PATH 128
#define DEVOBJ_LIST_SIZE 64

// ��ʱ����
#define DELAY_ONE_MICROSECOND   (-10)
#define DELAY_ONE_MILLISECOND   (DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND        (DELAY_ONE_MILLISECOND*1000)

// �����ڴ�ı�־���ڴ�й¶���
#define POOL_TAG 'tlFS'

#define LOG_PRINT(msg, ...) DbgPrint(msg, ##__VA_ARGS__)

/*
// ����������ȫ��ָ��
PDRIVER_OBJECT gSFilterDriverObject = NULL;

// CDO��ȫ��ָ��
PDEVICE_OBJECT gSFilterControlDeviceObject = NULL;
*/

typedef enum{
	DEV_FILESYSTEM = 0,	// �ļ�ϵͳ����
	DEV_VOLUME = 1		// �����
} DEV_TYPE;

// �����豸���豸��չ
typedef struct _SFILTER_DEVICE_EXTENSION 
{
	ULONG TypeFlag;

	DEV_TYPE DevType;

	// �洢�ļ�ϵͳָ����߾�ָ��
	PDEVICE_OBJECT DevObj;

	//
	//  Pointer to the file system device object we are attached to
	//

	PDEVICE_OBJECT AttachedToDeviceObject;

	//
	//  Pointer to the real (disk) device object that is associated with
	//  the file system device object we are attached to
	//

	PDEVICE_OBJECT StorageStackDeviceObject;

	//
	//  Name for this device.  If attached to a Volume Device Object it is the
	//  name of the physical disk drive.  If attached to a Control Device
	//  Object it is the name of the Control Device Object.
	//

	UNICODE_STRING DeviceName;

	//
	//  Buffer used to hold the above unicode strings
	//

	WCHAR DeviceNameBuffer[MY_DEV_MAX_NAME];


	// ���豸�ķ���������
	UNICODE_STRING SybDeviceName;

	//
	//  Buffer used to hold the above unicode strings
	//

	WCHAR SybDeviceNameBuffer[MY_DEV_MAX_NAME];

} SFILTER_DEVICE_EXTENSION, *PSFILTER_DEVICE_EXTENSION;



// �������Ժ�

/***********************************************************************
* ������:IS_MY_DEVICE_OBJECT 
* ��������:�ж��Ƿ��Ǳ��������ɵ��豸���󣬲��Ҳ���CDO
* �����б�:
*		_devObj:�豸����
* �ж�����:
*		�豸����ָ�벻Ϊ��
*		��������ΪgSFilterDriverObject
*		�豸��չ��Ϊ��
*		�豸��չ�ı�־ΪPOOL_TAG
* ����ֵ:BOOL
***********************************************************************/
#define IS_MY_DEVICE_OBJECT(_devObj) \
	(((_devObj) != NULL) && \
	((_devObj)->DriverObject == gSFilterDriverObject) && \
	((_devObj)->DeviceExtension != NULL) && \
	((*(ULONG *)(_devObj)->DeviceExtension) == POOL_TAG))

/***********************************************************************
* ������:IS_MY_CONTROL_DEVICE_OBJECT 
* ��������:�ж��Ƿ��Ǳ��������ɵ�CDO
* �����б�:
*		_devObj:�豸����
* �ж�����:
*		�豸����ΪgSFilterControlDeviceObject
*		��������ΪgSFilterDriverObject
*		û���豸��չ
* ����ֵ:BOOL
***********************************************************************/
#define IS_MY_CONTROL_DEVICE_OBJECT(_devObj) \
	(((_devObj) == gSFilterControlDeviceObject) ? \
	(ASSERT(((_devObj)->DriverObject == gSFilterDriverObject) && \
	((_devObj)->DeviceExtension == NULL)), TRUE) : \
	FALSE)

/***********************************************************************
* ������:IS_DESIRED_DEVICE_TYPE 
* ��������:��Ҫ���˵��豸��������
* �����б�:
*		_type:�豸����
* �ж�����:
*		_type��ֵ
* ����ֵ:BOOL
***********************************************************************/
#define IS_DESIRED_DEVICE_TYPE(_type) \
	((_type) == FILE_DEVICE_DISK_FILE_SYSTEM)
/*
#define IS_DESIRED_DEVICE_TYPE(_type) \
	(((_type) == FILE_DEVICE_DISK_FILE_SYSTEM) || \
	((_type) == FILE_DEVICE_CD_ROM_FILE_SYSTEM) || \
	((_type) == FILE_DEVICE_NETWORK_FILE_SYSTEM))
*/

typedef enum{
	SF_IRP_GO_ON = 0,		// ������ȥ�����й���
	SF_IRP_PASS = 1			// ������ȥ�������й���
} SF_RET;

// �洢Ŀ¼��Ϣ
#define MAX_DIR_PATH 1024

typedef struct _DIR_NODE
{
	_DIR_NODE *nextNode;
	UNICODE_STRING DirPathStr;
	WCHAR DirPathBuf[MAX_DIR_PATH];
	ULONG referenceNum;
}DIR_NODE,*PDIR_NODE;

#endif