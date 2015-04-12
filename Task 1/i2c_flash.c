#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>

#include "myioctl.h"

#define DEVICE_NAME "i2c_flash"

struct i2c_flash 							/* device structure */
{
	struct i2c_adapter *adap;
	char name[20];
	struct device *i2c_flash_device;
	struct cdev cdev;
	int current_page_position;
	unsigned short int page_address;

} *epp;

static dev_t dev1;      					/*allocated device number*/
struct class *c1;							/*tie with the device model*/
 
static int flag=1;							/* flag to indicate if the driver is busy: if 0:: busy and  1:: free */

unsigned short int page_address_clear;

int page_position=0;


/********************************I2C DEVICE OPEN*********************************************************/
 
static int i2c_flash_open(struct inode *inode, struct file *file)
{
	struct i2c_client *client;
    struct i2c_adapter *adap;
 	
 	printk("open()\n");
	
   /* Get the per-device structure that contains this cdev */
    epp = container_of(inode->i_cdev , struct i2c_flash , cdev);  
		 
    adap = i2c_get_adapter(0);
    if (!adap)
		return -ENODEV;
 
    client = kzalloc(sizeof(*client), GFP_KERNEL); 		/*allocation of memory to the client structure pointer*/
    if (!client) 
    {
		i2c_put_adapter(adap);
        return -ENOMEM;
    }
    
    strcpy(client->name,"i2c -client");					/* give a name to the client */
    printk("%s \n ",client->name);
 
    client->adapter = adap;
   	client->addr=0x54;							/* set the eeprom address in the chip address of the client structure */
    file->private_data = client;				/*	ease of access for rest of the program */
 
	return 0;
}
/************************I2C DEVICE RELEASE *********************************************************/
  
static int i2c_flash_release(struct inode *inode, struct file *file)
{
	struct i2c_client *client = file->private_data;
	
	printk("in release()! \n");
	
	i2c_put_adapter(client->adapter);			/* release the adapter */
	kfree(client);								/* free the memory of the client */
 	
 	printk("%s - released\n", client->name);

  	return 0;
}

/*******************************I2C DEVICE READ*********************************************************/

static ssize_t i2c_flash_read(struct file *file, char *buf, size_t count, loff_t *off)
{
	char *tmp;
	int ret;
	int i;
	int ret2;
	int ret3;

	unsigned short temphilo;
	
	/* store the private data in the client sructure pointer */
	struct i2c_client *client=file->private_data;
	
    tmp = kmalloc(count*64, GFP_KERNEL);			/*memory allocation for temporary buffer */
    
    if (tmp == NULL)
        return -ENOMEM;
 	
   	flag=0; 										/* reset the flag : eeprom is busy */
	
	gpio_set_value_cansleep(26,1);					/* glow the LED : write operation in progress */

    printk("In Read()\n");
    printk("Reading %d pages\n",count);
   
 	/* since the higher byte needs to be sent first swap the 2 bytes */
 	
 	temphilo=epp->page_address;
 	temphilo=htons(temphilo);
		
	/* set the starting address */

	ret3 = i2c_master_send(client,(char*)&(temphilo), 2);
    msleep(20);

    /* for the no of pages mentioned by the user run the loop such that the data is read
     from that page address and store it the buffer, which will be copied to the user */

    for(i = 0; i < count; i++)
 	{
 		memset(tmp,0,64);
    	
    	ret = i2c_master_recv(client,tmp, 64);
    	
    	msleep(20);
    	
    	if (ret >= 0)
    	{
        	ret2 = copy_to_user(&buf[i*64], tmp, 64);
        	if(ret2!=0)
     			{
     				printk(" error in reading data..copy to user ..\n");
     				return -EFAULT;
     			}
       	}
    	else
    	{
    		printk("error in reception of data from eeprom \n ");	
		}
	}
   
    kfree(tmp);						/* free the buffer memory*/
    
    gpio_set_value_cansleep(26,0);	/* switch off the LED to indicate the read operation is over*/ 
    
    flag=1;							/* set the flag : eeprom not busy */
   
    return 0;
}

 /***************************************DEVICE WRITE ********************************************/ 

static ssize_t i2c_flash_write(struct file *file, const char *buf, size_t count, loff_t *off)
{
	struct i2c_client *client = file->private_data;
	
	int ret;
	int ret1; 
	char *temp1;
	int i;
	unsigned short temphilo;
	
	flag=0;											// reset the flag :: eeprom busy
	
	gpio_set_value_cansleep(26,1);					// glow the LED at io pin 8 indicating write operation

	epp->current_page_position=epp->page_address; 	// update the value of the current position if not 0
	
	temp1 = kmalloc(count*66, GFP_KERNEL);			//allocate memory 
   
    if (temp1 == NULL)
    {
    	printk("error in memory allocation\n");
        return -ENOMEM;
    }

  	printk("In Write()\n");
    printk("Writing %d pages\n", count);
   
    /*for the number of pages mentioned by the user run the loop, each time 64bytes of data is written onto the eeprom
   	the page address is incremented and 64bytes is written again*/
  
    for (i = 0; i < count; i++)
	{
		memset(temp1,0,66);
		
		/*multiply the page address by 64 and swap the 2 bytes: higher byte needs to be sent first */

		temphilo = epp->page_address*64;
		
		temphilo = temphilo&0x7fc0;
		
		temphilo = htons(temphilo);

		memcpy(temp1,&temphilo,2);

	   	ret1=copy_from_user(&temp1[2],&buf[i*64],64);
    	msleep(1);
   
    	if(ret1== 64)
    	{
    		printk("error in writing data...\n ");
    		return -EFAULT;
    	}
   
    	ret = i2c_master_send(client, temp1, 66);
		msleep(20);
		
		/* incrementing the page pointer, if it equal to 512 the loop it back to 0*/
		
		epp->current_page_position++;
		page_position=epp->current_page_position;
		
		if(epp->current_page_position==512)
		{
			epp->current_page_position=0;
		}
		
		/* incrementing the page address, if it equal to 512 the loop it back to 0*/

		epp->page_address++;
				
		if(epp->page_address==512)
		{
			epp->page_address=0;
		}
	}
    
    kfree(temp1);						// free the memory of the buffers
    
    gpio_set_value_cansleep(26,0);		// turn off the LED at io pin 8 indicating read operation is over
    
   	flag=1;								// set the flag :: eeprom not busy
    
    return 0;
}

/*********************************FUNCTION TO CLEAR DATA ****************************************************/

int mywrclear(struct i2c_client *client) 
{
	int ret;
	char *temp1;
	int i;
	unsigned short temphilo;
	
	page_address_clear=0;

   	temp1 = kmalloc(66, GFP_KERNEL);
   	if (temp1 == NULL)
    {
    	printk("error temp1 memory allocation");
        return -ENOMEM;
    }
    printk("Now  clearing data...\n");
    gpio_set_value_cansleep(26,1);
    
    /* for all the 512 pages of the eeprom clear the data by writing 1 to every bit */
   
    for (i = 0; i < 512; i++)
	{
		memset(temp1,0xFF,66);
		
		temphilo=page_address_clear*64;
		
		temphilo=temphilo&0x7fc0;

		temphilo=htons(temphilo);

		memcpy(temp1,&temphilo,2);
   		
   		ret = i2c_master_send(client, temp1, 66);
		
		msleep(10);
		
		page_address_clear++;
		if(page_address_clear==512)
		{
			page_address_clear=0;
		}
			
	}
    
    gpio_set_value_cansleep(26,0);
    
    kfree(temp1);
    
    return 0;
}

/********************************IOCTL FNCTION *************************************************************/


long i2c_flash_ioctl(struct file *file , unsigned int cmd ,unsigned long arg)
{
	int ret1;
	int ret;
	int ret2;
	unsigned short tempaddr=0;
	
	struct i2c_client *client = file->private_data;

	switch(cmd)
	{
		/*get the staus of the eeprom by checking the value of the flag*/
		
		case FLASHGETS:		
	
		if(flag==0)
		{
			return -EBUSY;
		}
		else
			return 0;
		
		break;

		/* get the current pointer position if the eeprom is not busy */

		case FLASHGETP:
	
		if(flag==0)
		{
			return -EBUSY;
		}
		else
		{
			ret=copy_to_user((int*)arg, &page_position,sizeof(page_position));

			if(ret<0)
			printk("value of ret %d\n", ret);
		}
		break;
		
		/* set the pointer position if the eeprom is not busy and the pointer value is valid */

		case FLASHSETP:
		
		if (flag==0)
		{
			return -EBUSY;
		}
		else 
		{
		ret2=copy_from_user(&tempaddr,(int*)arg,2);
		if(ret2<0)
			{
				printk("value of return :%d\n", ret2);
			}
		if(tempaddr>=512)
		{
			return -ENOMEM;
		}		
		else
			epp->page_address=tempaddr;
		//printk("the pointer has been set");
		}

		break;
		
		/* if the eeprom is not busy clear the data */

		case FLASHERASE:
		
		if(flag==0)
			return -EBUSY;
		else
		{
		ret1= mywrclear(client);
			return ret1;
		}
		break;
		
		/* return error if none of the macros were matched*/

		default :
		printk("in default none matched !!");
		return -ENOTTY;
	}
	return 0;
}

/*******************FILE OPERATION DEFINITION ******************************************************/

static struct file_operations i2c_flash_fops =
{
  	.owner = THIS_MODULE,
  	.open = i2c_flash_open,
  	.release = i2c_flash_release,
  	.read = i2c_flash_read,
  	.write = i2c_flash_write,
  	.unlocked_ioctl = i2c_flash_ioctl,
  
};
 
 /***********************************MODULE INITIALISATION***********************************************************/

static int __init i2c_flash_init(void) 
{
  	int ret;
  	struct i2c_adapter *adap;
  	
  	printk("Initialising .... \n");

	/* Request dynamic allocation of a device major number */
	
	if (alloc_chrdev_region(&dev1, 0, 1, DEVICE_NAME) < 0) 
	{
			printk(KERN_DEBUG "Can't register device\n"); 
			return -1;
	}
	
	/* Populate sysfs entries */

	c1 = class_create(THIS_MODULE, DEVICE_NAME);
	
	adap = to_i2c_adapter(0);

	/* Allocate memory for the per-device structure */
	
	epp = kzalloc(sizeof(*epp), GFP_KERNEL);
    if (!epp)
        return -ENOMEM;
       
    epp->adap = adap;

    /* create the device */

    epp->i2c_flash_device = device_create(c1, NULL ,dev1, NULL, DEVICE_NAME );

	/* link the file operations to the one with cdev */

	cdev_init(&epp->cdev, &i2c_flash_fops);
	epp->cdev.owner = THIS_MODULE;
	printk("cdev initiated \n");
	
	/* Connect the major/minor number to the cdev */
	
	ret = cdev_add(&epp->cdev,dev1, 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/*Leads to the selection of the SDA and SCL lines : when gpio 29 is 0 */
	
	gpio_request(29,"i2c_mux"); 
	gpio_direction_output(29,0);
	gpio_set_value_cansleep(29,0);
	
	/* enabling the io pin 8 where the LED is connect : gpio no 26 */
	
	gpio_request(26,"led");
	gpio_direction_output(26,0);
	gpio_set_value_cansleep(26,0);
	
	/* default initialising the page address as 0 */ 

	epp->page_address=0;

	
	printk("EEPROM driver initialized.\n");
	return 0;
}

/*****************MODULE CLEAN UP ***************************************************/

static void __exit i2c_flash_exit(void) 
{	
	printk("Exiting ....!\n");
	
	msleep(10);
	
	/* free the gpio*/
	gpio_free(29);
	gpio_free(26);
	
	 /* delete the device */
	cdev_del(&epp->cdev);

	device_destroy (c1, dev1);
	
	kfree(epp);

	/*destroy the driver class */
	
	class_destroy(c1);
	
	/*unregister the character device region */

	unregister_chrdev_region((dev1), 1);

	printk("EEPROM driver removed.\n");
}


/***************************************************************************************************/

module_init(i2c_flash_init);
module_exit(i2c_flash_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Veena Rajan,referrenced from linux/i2c-dev.c");