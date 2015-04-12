
#ifndef MYIOCTL_H
#define MYIOCTL_H

#include <linux/ioctl.h>

#define I2C_FLASH_IOC_MAGIC 'g'
 

#define FLASHGETS _IO(I2C_FLASH_IOC_MAGIC,1)
#define FLASHGETP _IOR(I2C_FLASH_IOC_MAGIC,2,int)
#define FLASHSETP _IOW(I2C_FLASH_IOC_MAGIC,3,int)
#define FLASHERASE _IO(I2C_FLASH_IOC_MAGIC,4)

#endif