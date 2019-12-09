
***Install the Xilinx driver before using this project.***   
Modify xdma_data_uplode.c deviceName and xdma_data_uplode.h DEFINE_GUID(deviceName、DEFINE_GUID is Device instance path).  
code：  
```
int main()
{
  sys_init();
  sys_open(1);//First device=0,second device=1.........
  sys_wite32(int bar, long addr, unsigned int data, int bus_id);//0,0x0,1,1
  sys_read32(int bar, long addr, unsigned int *data, int bus_id);//0,0x0,&data,1
}
```
