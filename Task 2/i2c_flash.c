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
#include <linux/workqueue.h>

#include "myioctl.h"

#define DEVICE_NAME "i2c_flash"

static struct workqueue_struct *my_wq;  /* work queue structure definition*/

typedef struct 
{
	struct work_struct my_work;
	int count;
	int current_page_position_queue;
	int page_address_read_queue;
	int page_address_write_queue;
	struct i2c_client *client_queue;

}my_work_t;

struct i2c_flash 					/* device structure */
{
	struct i2c_adapter *adap;
	struct device *i2c_flash_device;
	struct cdev cdev;
	unsigned short int page_address;
	int current_page_position;

} *epp;

my_work_t *work1, *work2;

static dev_t dev1;      			/*allocated device number*/
struct class *c1;					/*tie with the device model*/

int flag=1;							/* flag to inidicate if the driver is busy: if 0:: busy and  1:: free */ 
int datacheck=0;					/* flag to indicate if the data is ready: 
										datacheck=0 : data not ready 
										datacheck=1 : data is ready */
int page_position=0;

unsigned short page_address_clear;

struct i2c_client *client;

char *readbuffer;					/*char buffers for storing read and write data*/
char *writebuffer;




/*************************************OPEN METHOD**********************************************************/
 
static int i2c_flash_open(struct inode *inode, struct file *file)
{
	
    struct i2c_adapter *adap;
 	
 	printk("open()\n");
	
    /* Get the per-device structure that contains this cdev */
    epp = container_of(inode->i_cdev , struct i2c_flash , cdev);  
		 
    adap = i2c_get_adapter(0);						/*pass the minor number to the adapter structure pointer*/	
    if (!adap)
		return -ENODEV;
 
    client = kzalloc(sizeof(*client), GFP_KERNEL);	/*allocation of memory to the client structure pointer*/
    if (!client) 
    {
		i2c_put_adapter(adap);
        return -ENOMEM;
    }
    
    strcpy(client->name,"i2c - client");			/* give a name to the client */
    printk("%s opened",client->name);
 
    client->adapter = adap;						
  
    /* set the eeprom addresss in the chip address of the client structure */
   	client->addr=0x54;							
  
   	/*	ease of access for rest of the program */	 
    file->private_data = client;
 
	return 0;
}
/*************************************RELEASE DRIVER**************************************************************/

  static int i2c_flash_release(struct inode *inode, struct file *file)
{
	client = file->private_data;
	printk("in release()! \n");
	//printk("%s ", client->name);
	
	return 0;
}

/********************************WORK QUEUE FUNCTION :: READ  ************************************************/

static void my_workqueue_function_read(struct work_struct *work)
{
	int ret;
	int i;
	int ret3;
	
	unsigned short temphilo;

	my_work_t *my_work=(my_work_t *)work2;
	
	printk("In Read() .. workqueue function\n");

	gpio_set_value_cansleep(26,1); 			// glow the LED at io pin 8 indicating read operation
	 
	flag=0;									// reset the busy flag
	
	datacheck=0; 							// the data is not ready

	/*define the buffer size to the total no of bytes necessay :: no of pages x 64 */
	
	readbuffer= kmalloc(my_work->count*64,GFP_KERNEL);  
	
	memset(readbuffer,0,my_work->count*64);
	
	temphilo= my_work->page_address_read_queue;
	
	/* swap the bits of the page address as we need to send the higher address first then 
	the lower address to the eeprom*/
   	
   	temphilo=htons(temphilo); 

   	ret3 = i2c_master_send(my_work->client_queue,(char*)&(temphilo), 2);
    	msleep(20);
		
    /* for the no of pages mentioned by the user run the loop such that we read 
    	the data from that page address and store it the buffer */
   	
   	for(i = 0; i < my_work->count; i++)
 	{
    	ret = i2c_master_recv(my_work->client_queue,&readbuffer[i*64], 64);
    	msleep(20);    	
	}		
   
    datacheck=1;				        // set the datacheck flag:: data avaliable in the read buffer 

	flag=1;						        // set the flag :: eeprom not busy

    kfree((void*)work2);		       // free the work pointer 
   
    gpio_set_value_cansleep(26,0);	   // turn off the LED at io pin 8 indicating read operation is over
}    

/**********************************WORK QUEUE FUNCTION :: WRITE *******************************************/

static void my_workqueue_function_write(struct work_struct *work1)
{
	int ret;
	int i;
	char *temp1;
	unsigned short temphilo;
	
	my_work_t *my_work=	(my_work_t*)work1;

	printk("In Write ().. workqueue function\n");
	
	gpio_set_value_cansleep(26,1);					// glow the LED at io pin 8 indicating write operation

   	temp1 = kmalloc(66, GFP_KERNEL);

   	my_work->current_page_position_queue=my_work->page_address_write_queue;

	flag=0;											// reset the flag :: eeprom busy
   	
   	/*for the number of pages mentioned by the user run the loop, each time 64bytes of data is written onto the eeprom
   	the page address is incremented and 64bytes is written again*/
  	 
    
    for (i = 0; i < my_work->count; i++)
	{
		memset(temp1,0,66);

		/*calculate the page address by multiplying it by 64 and swapping the bytes of the address */

		temphilo=my_work->page_address_write_queue*64;
		
		temphilo=temphilo & 0x7fc0;
		
		temphilo=htons(temphilo);

		memcpy(temp1,&temphilo,2);
		
		memcpy(&temp1[2],&writebuffer[i*64] ,64);
		
		ret = i2c_master_send(my_work->client_queue, temp1, 66);
		msleep(20);
		
		/* incrementing the page pointer, if it equal to 512 the loop it back to 0*/
		
		my_work->current_page_position_queue++;
		
		page_position=my_work->current_page_position_queue;
		
		if(my_work->current_page_position_queue==512)
		{
			my_work->current_page_position_queue=0;
		}
		
		/* incrementing the page address, if it equal to 512 the loop it back to 0*/
		
		my_work->page_address_write_queue++;
		
		if(my_work->page_address_write_queue==512)
		{
			my_work->page_address_write_queue=0;
		}
		
	}
    
   	kfree(temp1);						// free the memory of the buffers
	kfree((void*)work1);
	kfree(writebuffer);

	flag=1;								// set the flag :: eeprom not busy

	gpio_set_value_cansleep(26,0);		// turn off the LED at io pin 8 indicating read operation is over
}

/**********************************I2C FLASH READ ******************************************************/

static ssize_t i2c_flash_read(struct file *file, char *buf, size_t count, loff_t *off)
{
	
	int ret_workqueue;
	int ret1=1;

	/*allocate memory to the work structure pointer */
	
	work2 = (my_work_t*)kmalloc(sizeof(my_work_t), GFP_KERNEL);
	
	/* if the eeprom is not busy and the data and not ready, then queuing the read function into the queue
	else copy the data to the user( data ready and eeprom not busy)
	but if the eeprom is busy, we return -EBUSY */
	
	if(flag!=0)
	{
		if(datacheck==0)
		{
		INIT_WORK((struct work_struct*)work2,my_workqueue_function_read);
		
		work2->count=count;
		
		work2->page_address_read_queue= epp->page_address;
		
		work2->client_queue =file->private_data;

		ret_workqueue =queue_work(my_wq, (struct work_struct*)work2 );
		
		return -EAGAIN;
		}
		else
		{
			ret1=copy_to_user(buf,readbuffer,count*64);
			datacheck=0;
			kfree(readbuffer);
			return 0;
		}
	}
	else
	{
		return -EBUSY;
	}
	return 0;
}

/************************************I2C FLASH WRITE*******************************************************/  

static ssize_t i2c_flash_write(struct file *file,const char *buf, size_t count, loff_t *off)
{
	int ret_workqueue;
	int ret;
	
	writebuffer=kmalloc(count*64, GFP_KERNEL); //memory allocation to the write buffer 

	ret=copy_from_user(writebuffer,buf,count*64); //copy the data from the user into this write buffer 
	
	/* if the eeprom is not busy queue the work and if it is return -EBUSY */

	if(flag==1)
	{
		work1 = (my_work_t*)kmalloc(sizeof(my_work_t), GFP_KERNEL);

		if (work1) 
		{
			INIT_WORK((struct work_struct*)work1,my_workqueue_function_write);
		
			work1->count=count;
			
			work1->current_page_position_queue = epp->current_page_position;
			
			work1->page_address_write_queue = epp->page_address;
			
			work1->client_queue = file->private_data;

			ret_workqueue =queue_work(my_wq, (struct work_struct*)work1 );
		}
		else
		{
			return -ENOMEM;
		}
	}
	else
	{
		return -EBUSY;
	}
	
    return 0;
}

/*********************************CLEAR THE DATA OF THE EEPROM*********************************************/


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



/*******************************IOCTL FUNCTION DEFINITION*************************************************/


long i2c_flash_ioctl(struct file *file , unsigned int cmd ,unsigned long arg)
{
	int ret1;
	int ret;
	int ret2;
	unsigned short tempaddr=0;
	
	struct i2c_client *client = file->private_data;

	switch(cmd)
	{
		/*get the status of the eeprom by checking the value of the flag*/
		
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
		
		/* set the pointer position if the eeprom is not busy and if the address is valid  */

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

/********************FILE OPERATION DEFINITIONS *********************************************************/

static struct file_operations i2c_flash_fops =
{
  	.owner = THIS_MODULE,
  	.open = i2c_flash_open,
  	.release = i2c_flash_release,
  	.read = i2c_flash_read,
  	.write = i2c_flash_write,
  	.unlocked_ioctl = i2c_flash_ioctl,
  
};
 
 /*****************************MODULE INITILAISATION***************************************************/

static int __init i2c_flash_init(void) 
{
 	int ret1;
  	struct i2c_adapter *adap;
  	
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

	
	/* Connect the major/minor number to the cdev */
	
	ret1 = cdev_add(&epp->cdev,dev1, 1);

	if (ret1) 
	{
		printk("Bad cdev\n");
		return ret1;
	}

	/*Leads to the selection of the SDA and SCL lines : when gpio 29 is 0 */
	
	gpio_request(29,"mux"); 
	gpio_direction_output(29,0);
	gpio_set_value_cansleep(29,0);
	
	/* enabling the io pin 8 where the LED is connected : gpio no 26 */
	
	gpio_request(26,"led");
	gpio_direction_output(26,0);
	gpio_set_value_cansleep(26,0);
	
	/* default initialising the page address as 0 */ 
	
	epp->page_address=0;

	/* creating the work queue */
	
	my_wq=create_workqueue("my_queue");
	
	printk("work queue created !\n");
	if (!my_wq) 
	{
		printk("error in initialisation of work queue \n");
	}
	
	printk("EEPROM driver initialized.\n");
	return 0;
}

 /***************************************CLEAN UP MODULE ********************************************/

static void __exit i2c_flash_exit(void) 
{	
	
	msleep(10);
	printk("in exit()\n");
	
	/* destroy the work queue */
	destroy_workqueue(my_wq);
	
	/* free the memory of the client pointer */
    kfree(client);
   
	/* free the gpio*/
	gpio_free(26);
	gpio_free(29);
    
    
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

module_init(i2c_flash_init);
module_exit(i2c_flash_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Veena Rajan, referenced from linux/i2c-dev.c");