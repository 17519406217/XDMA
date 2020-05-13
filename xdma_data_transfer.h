#pragma once

int sys_init(); 
int sys_open(int DeviceId);
int sys_close(int DeviceId);
void sys_write32(int bar, long long int addr, unsigned int data, int bus_id);
void sys_read32(int bar, long long int addr, unsigned int *data, int bus_id);
unsigned char*  sys_dma_read(int bus_id, long long int address, int size);
unsigned char* sys_dma_write(int bus_id, long long int address, int *data, int size);



