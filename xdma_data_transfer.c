
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <strsafe.h>
#include <Windows.h>
#include <SetupAPI.h>
#include <INITGUID.H>
#include <WinIoCtl.h>

#include <time.h>
#include "xdma_public.h"
#include "xdma_data_transfer.h"
#pragma comment(lib, "setupapi.lib")
/*
 *FPGA_COUNT设置扫描板块数量
 */
#define DMASIZE 4*1024
#define FPGA_COUNT 10

#ifdef  UNICODE
    #define _tcscpy     wcscpy
#else
    #define _tcscpy     strcpy
#endif
#ifdef UNICODE
#define SetupDiGetClassDevs SetupDiGetClassDevsW
#else
#define SetupDiGetClassDevs SetupDiGetClassDevsA
#endif
typedef struct {
    TCHAR base_path[MAX_PATH + 1]; /* path to first found Xdma device */
    TCHAR c2h0_path[MAX_PATH + 1];	/* card to host DMA 0 */
    TCHAR h2c0_path[MAX_PATH + 1];	/* host to card DMA 0 */
    PBYTE buffer; //指向分配缓冲区的指针
    DWORD buffer_size; /* size of the buffer in bytes *///缓冲区的大小（以字节为单位）
	LARGE_INTEGER address;
    HANDLE c2h0;
    HANDLE h2c0;
} xdma_device;
typedef struct {
    BOOL verbose;
    char* device;
    char* file;
    BYTE* data;
    LARGE_INTEGER address;
    DWORD size;
    enum Direction direction;
    size_t alignment;
    BOOL binary;
} Options;
struct machine {
    xdma_device xdma;
    HANDLE controlDevice;
    Options controlOption;
}fpga[100];//可识别板卡数量最大值手动设置为100
char vendor_id[4] = {""};
char device_id[4] = {""};
long long int test = 0;
int find_devices(GUID guid, struct machine *device) {

	HDEVINFO dev_info = SetupDiGetClassDevs((LPGUID)&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (dev_info == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "GetDevices INVALID_HANDLE_VALUE\n");
		exit(-1);
	}
	SP_DEVICE_INTERFACE_DATA dev_interface;
	dev_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	// enumerate through devices
	DWORD index;
	for (index = 0; SetupDiEnumDeviceInterfaces(dev_info, NULL, &guid, index, &dev_interface); ++index)
	{
		// get required buffer size
		ULONG detail_size = 0;
		if (!SetupDiGetDeviceInterfaceDetail(dev_info, &dev_interface, NULL, 0, &detail_size, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			fprintf(stderr, "SetupDiGetDeviceInterfaceDetail - get length failed\n");
			break;

		}
		// allocate space for device interface detail
		PSP_DEVICE_INTERFACE_DETAIL_DATA dev_detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, detail_size);
		if (!dev_detail) {
			fprintf(stderr, "HeapAlloc failed\n");
			break;
		}

		dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		// get device interface detail
		if (!SetupDiGetDeviceInterfaceDetail(dev_info, &dev_interface, dev_detail, detail_size, NULL, NULL)) {
			fprintf(stderr, "SetupDiGetDeviceInterfaceDetail - get detail failed\n");
			HeapFree(GetProcessHeap(), 0, dev_detail);
			break;
		}
		HeapFree(GetProcessHeap(), 0, dev_detail);
	}
	SetupDiDestroyDeviceInfoList(dev_info);
	return index;
}

BYTE* allocate_buffer(size_t size, size_t alignment) {

	if (size == 0) {
		size = 4;
	}
	if (alignment == 0) {
		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);
		alignment = sys_info.dwPageSize;
	}
	return (BYTE*)_aligned_malloc(size, alignment);
}

void prama_init(struct machine *param)
{
	for (int i = 0; i < FPGA_COUNT; i++) {
		param[i].controlOption.size = 4;
		param[i].controlOption.address.QuadPart = 0;
		param[i].controlOption.device = _strdup("user");//???? user : ????????  h2c: host->board(DMA) c2h: board->host (DMA)
		param[i].controlOption.alignment = 0;

	}
}
static int get_devices(GUID guid, char* devpath, size_t len_devpath, int openDeviceId) {

	HDEVINFO device_info = SetupDiGetClassDevs((LPGUID)&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (device_info == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "GetDevices INVALID_HANDLE_VALUE\n");
		exit(-1);
	}

	SP_DEVICE_INTERFACE_DATA device_interface;
	device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	// enumerate through devices
	DWORD index;
	index = SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, openDeviceId, &device_interface);

	//for (index = 0; SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, index, &device_interface); ++index)
	//{

	// get required buffer size
	ULONG detailLength = 0;
	if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &detailLength, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		fprintf(stderr, "SetupDiGetDeviceInterfaceDetail - get length failed\n");
		//break;
		return -1;
	}

	// allocate space for device interface detail
	PSP_DEVICE_INTERFACE_DETAIL_DATA dev_detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, detailLength);
	if (!dev_detail) {
		fprintf(stderr, "HeapAlloc failed\n");
		//break;
		return -1;
	}
	dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	// get device interface detail
	if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, dev_detail, detailLength, NULL, NULL)) {
		fprintf(stderr, "SetupDiGetDeviceInterfaceDetail - get detail failed\n");
		HeapFree(GetProcessHeap(), 0, dev_detail);
		//break;
		return -1;
	}
	StringCchCopy(devpath, len_devpath, dev_detail->DevicePath);
	HeapFree(GetProcessHeap(), 0, dev_detail);
	//}

	SetupDiDestroyDeviceInfoList(device_info);

	return index;
}
int openControlDevice(int openDeviceId)
{
	DWORD num_devices;
	char device_path[MAX_PATH + 1] = "";
	num_devices = get_devices(GUID_DEVINTERFACE_XDMA, fpga[openDeviceId].xdma.base_path, sizeof(fpga[openDeviceId].xdma.base_path), openDeviceId);
	strcpy_s(device_path, sizeof device_path, fpga[openDeviceId].xdma.base_path);
	strcat_s(device_path, sizeof device_path, "\\");
	strcat_s(device_path, sizeof device_path, fpga[openDeviceId].controlOption.device);
	fpga[openDeviceId].controlOption.data = allocate_buffer(fpga[openDeviceId].controlOption.size, fpga[openDeviceId].controlOption.alignment);
	if (!fpga[openDeviceId].controlOption.data) {
		fprintf(stderr, "Error allocating %ld bytes of memory, error code: %ld\n", fpga[openDeviceId].controlOption.size, GetLastError());
		CloseHandle(fpga[openDeviceId].controlDevice);
		return -1;
	}
	memset(fpga[openDeviceId].controlOption.data, 0, fpga[openDeviceId].controlOption.size);
	//start device
	fpga[openDeviceId].controlDevice = CreateFile(device_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fpga[openDeviceId].controlDevice == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error opening device, win32 error code: %ld\n", GetLastError());
		CloseHandle(fpga[openDeviceId].controlDevice);
		return -1;
	}
	return 0;
}

int openDmaDevice(int openDeviceId)
{
	char device_path[MAX_PATH + 1] = "";
	strcpy_s(fpga[openDeviceId].xdma.c2h0_path, sizeof fpga[openDeviceId].xdma.c2h0_path, fpga[openDeviceId].xdma.base_path);
	strcat_s(fpga[openDeviceId].xdma.c2h0_path, sizeof device_path, "\\c2h_0");
	strcpy_s(fpga[openDeviceId].xdma.h2c0_path, sizeof fpga[openDeviceId].xdma.h2c0_path, fpga[openDeviceId].xdma.base_path);
	strcat_s(fpga[openDeviceId].xdma.h2c0_path, sizeof device_path, "\\h2c_0");


	fpga[openDeviceId].xdma.c2h0 = CreateFile(fpga[openDeviceId].xdma.c2h0_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fpga[openDeviceId].xdma.c2h0 == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error opening device, win32 error code: %ld\n", GetLastError());
		CloseHandle(fpga[openDeviceId].xdma.c2h0);
		return -1;
	}
	fpga[openDeviceId].xdma.h2c0 = CreateFile(fpga[openDeviceId].xdma.h2c0_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fpga[openDeviceId].xdma.c2h0 == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error opening device, win32 error code: %ld\n", GetLastError());
		CloseHandle(fpga[openDeviceId].xdma.c2h0);
		return -1;
	}

	fpga[openDeviceId].xdma.buffer_size = DMASIZE; /* buffer size, for easy debugging use a power of two */
	fpga[openDeviceId].xdma.buffer = (PBYTE)_aligned_malloc(fpga[openDeviceId].xdma.buffer_size, DMASIZE);
	if (fpga[openDeviceId].xdma.buffer == NULL) {
		CloseHandle(fpga[openDeviceId].xdma.c2h0);
		return -1;
	}
	/* clear buffer */
	memset(fpga[openDeviceId].xdma.buffer, 0, fpga[openDeviceId].xdma.buffer_size);
	return 0;
}

int closeDevice(int openDeviceId)
{
	CloseHandle(fpga[openDeviceId].controlDevice);
	CloseHandle(fpga[openDeviceId].xdma.c2h0);
	CloseHandle(fpga[openDeviceId].xdma.h2c0);
	return 0;
}

void sys_write32(int bar, long long int addr, unsigned int data, int bus_id)
{

	fpga[bus_id].controlOption.address.QuadPart = addr;
	if (INVALID_SET_FILE_POINTER == SetFilePointerEx(fpga[bus_id].controlDevice, fpga[bus_id].controlOption.address, NULL, FILE_BEGIN)) {
		CloseHandle(fpga[bus_id].controlDevice);
	}

	if (!WriteFile(fpga[bus_id].controlDevice, (LPCVOID)&data, fpga[bus_id].controlOption.size, &fpga[bus_id].controlOption.size, NULL)) {
		CloseHandle(fpga[bus_id].controlDevice);
	}
}
void sys_read32(int bar, long long int addr, unsigned int *data, int bus_id)
{
	fpga[bus_id].controlOption.address.QuadPart = addr;
	if (INVALID_SET_FILE_POINTER == SetFilePointerEx(fpga[bus_id].controlDevice, fpga[bus_id].controlOption.address, NULL, FILE_BEGIN)) {
		fprintf(stderr, "Error setting file pointer, win32 error code: %ld\n", GetLastError());
		CloseHandle(fpga[bus_id].controlDevice);
	}
	if (!ReadFile(fpga[bus_id].controlDevice, data, fpga[bus_id].controlOption.size, &fpga[bus_id].controlOption.size, NULL)) {
		fprintf(stderr, "ReadFile from device %s failed with Win32 error code: %ld\n",
			fpga[bus_id].xdma.base_path, GetLastError());
		CloseHandle(fpga[bus_id].controlDevice);
	}
}


unsigned char* sys_dma_read(int bus_id, long long int address, int size)
{
	fpga[bus_id].xdma.buffer_size = size; /* buffer size, for easy debugging use a power of two */
	fpga[bus_id].xdma.buffer = (PBYTE)_aligned_malloc(fpga[bus_id].xdma.buffer_size, size);
	if (fpga[bus_id].xdma.buffer == NULL) {
		CloseHandle(fpga[bus_id].xdma.c2h0);
		return -1;
	}
	memset(fpga[bus_id].xdma.buffer, 0, fpga[bus_id].xdma.buffer_size);
	DWORD numread;
	fpga[bus_id].xdma.address.QuadPart = address;
	if (INVALID_SET_FILE_POINTER == SetFilePointerEx(fpga[bus_id].xdma.c2h0, fpga[bus_id].xdma.address, NULL, FILE_BEGIN)) {
		fprintf(stderr, "Error setting file pointer, win32 error code: %ld\n", GetLastError());
		goto CleanupDevice;
	}
	if (ReadFile(fpga[bus_id].xdma.c2h0, (LPVOID)fpga[bus_id].xdma.buffer, size, &numread, NULL))
	{
		return  fpga[bus_id].xdma.buffer;
	}
	else
	{
		goto CleanupDevice;
	}
CleanupDevice:
	CloseHandle(fpga[bus_id].xdma.c2h0);
	return -1;
}
unsigned char* sys_dma_write(int bus_id, long long int address,  int *data, int size)
{
	DWORD numread;
	fpga[bus_id].xdma.address.QuadPart = address;
	if (INVALID_SET_FILE_POINTER == SetFilePointerEx(fpga[bus_id].xdma.h2c0, fpga[bus_id].xdma.address, NULL, FILE_BEGIN)) {
		fprintf(stderr, "Error setting file pointer, win32 error code: %ld\n", GetLastError());
		goto CleanupDevice;
	}
	if (!WriteFile(fpga[bus_id].xdma.h2c0, data, size, &numread, NULL))
	{
		fprintf(stderr, "WriteFile from device %s failed: %d\n", fpga[bus_id].xdma.h2c0_path, GetLastError());
		goto CleanupDevice;
		//return -2;
	}

	return NULL;
CleanupDevice:
	CloseHandle(fpga[bus_id].xdma.h2c0);
	return -1;
}

int sys_init()
{
    DWORD num_devices;
    prama_init(fpga);
    num_devices = find_devices(GUID_DEVINTERFACE_XDMA, fpga);

    //
    return num_devices;
}
int sys_open(int DeviceId)
{
    int ret = 0;
    ret = openControlDevice(DeviceId);
    if (ret < 0) {
        return -1;
    }
    ret = openDmaDevice(DeviceId);
    if (ret < 0) {
        return -1;
    }
    return 0;
}
int sys_close(int DeviceId)
{
    closeDevice(DeviceId);
    return 0;
}



struct P12 {
    int d1 : 12;
    int d2 : 12;
    int d3 : 8;

};
struct PU12 {
    unsigned  int  d1 : 12;
    unsigned  int  d2 : 12;
    unsigned  int  d3 : 8;

};
struct P_12
{
    union
    {
        struct P12 p12;
        struct PU12 pu12;
    }d;
};
//#pragma pack(pop)
struct P_12 *p_12;




