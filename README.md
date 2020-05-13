
***Install the Xilinx driver before using this project.***   

Modify xdma_data_uplode.c deviceName and xdma_data_uplode.h DEFINE_GUID(deviceName、DEFINE_GUID is Device instance path).  
code：  
```
int main()
{
  sys_init();//return device number
  sys_open(int device_num);//First device_num=0,second device_num=1.........
  
  //FPGA Register write/read 
  sys_write32(int bar, long long int addr, unsigned int data, int bus_id);//bus_id=device_num
  //sys_write32 Example:
  //sys_write32(Default bar = 0,addr,data,device_num)
  
  sys_read32(int bar, long long int addr, unsigned int *data, int bus_id);
  //sys_read32 Example:
  //return_val  = sys_read32(Default bar = 0, addr, *data, int bus_id);//bus_id=device_num
  //Register val = return_val
  
  //FPGA dma
  unsigned char* sys_dma_read(int bus_id, long long int address, int size);
  int sys_dma_write(int bus_id, long long int address,  int *data, int size);
  //Example:
  //unsigned char *buf = (unsigned char *)malloc(0x1000);
  //memset(buf,0x4c,0x1000);
  //int val = sys_dma_write(1,0x200000000,buf,0x1000);
  //buf = sys_dma_read(1,0x200000000,0x1000);
}
```
