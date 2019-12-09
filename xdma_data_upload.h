#pragma once

int sys_device_slot();

unsigned char* sys_dma_read(int bus_id);
int sys_analysis(int dma_size,int analysis_data[32*1024],int card);

int sys_init();
int sys_open(int DeviceId);
int sys_close(int DeviceId);
void sys_write32(int bar, long addr, unsigned int data, int bus_id);
void sys_read32(int bar, long addr, unsigned int *data, int bus_id);