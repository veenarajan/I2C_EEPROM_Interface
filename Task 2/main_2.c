#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "myioctl.h"

int fd,ret1,ret4;
int ioctl_ret1,ioctl_ret2,ioctl_ret3,ioctl_ret4;
int pagecount;
char *readbuff, *tempbuff;
int	 k, i;
int  *arg;
int a,num,start,range;
char *p;
int *t;
char choice;

int main()
{	
	fd=open("/dev/i2c_flash",O_RDWR); 	//open the device 
	
	if(fd<0)
	{
		printf("unable to open device \n ");
	
		return -1;
	}

	/* Enter the option : loops till y is enterd if not breaks out */ 
	
	printf("Enter your choice\n");

	do{
		printf("\n\n1.Enter the page count\n2.Set the pointer(read/write)\n3.Write\n4.Read the data\n5.Get status\n6.Get current page position\n7.Clear the data\n8.Exit\n\n");
	
		scanf("%d",&a);

		switch(a)
		{
			case 1:
			
			printf("enter the number of pages\n");
			
			scanf("%d",&pagecount);
			
			printf("the no of pages entered %d\n", pagecount);
			break;

			case 2:

			arg=(int*)malloc(2);	
			printf("set the pointer from which you wish to read/write \n");
			scanf("%d",arg);
			errno=0;
			ioctl_ret3=ioctl(fd, FLASHSETP,arg);		
			
			if (ioctl_ret3<0)
			{	if(errno==16)
					printf("the eeprom was busy \n");
				else
					printf("the number entered is invalid. Please enter no from 0-511 \n");
			}
			else
				printf("the value has been set to %d\n", *arg);
			break;

			case 3:
			
			/* source :: random string generation ::
			https://www.youtube.com/watch?v=bugoz05pY3c*/
	
			tempbuff=malloc(pagecount*64);
			if(tempbuff== NULL)
			{
				printf("Error in allocation of memory\n");
				return -ENOMEM;
			}
		
			memset(tempbuff,0,pagecount*64);

			srand((unsigned int)time(NULL));
			p=(char *)malloc(sizeof(tempbuff));

			start=(int)('a');
			range=(int)('z')- (int)('a');

			for (p = tempbuff ,i=1; i <= pagecount*64 ;p++, ++i)
			{
				num=rand()%range;
				*p=(char)(num+start);
			}
			//strcat(tempbuff,"\n");

			printf("Writing data...\n");
		
			ret1=write(fd,tempbuff,pagecount);
		
			printf("the data has been written\n");
			break;

	
			case 4:
		
			readbuff=malloc(pagecount*64);
			if(readbuff== NULL)
			{
				printf("Error in allocation of memory\n");
				return -1;
			}

			printf("Now will read data .....\n");
			
			do{
			
			errno=0;
			
			ret4=read(fd,readbuff,pagecount);
			
			if(ret4!=0)
			{
				if(errno == 11)
					printf("Data is not ready \n");
				else
					printf("EEPROM is busy\n");
				
				printf("error in reading data\n");
				
				sleep(1);
			}
		
			else
				printf(" the string is :: %s \n", readbuff);
				//sleep(1);
			
			}while(errno);	
			break;

	
			case 5:
		
			printf("the current status of the eeprom :: \n");
			
			ioctl_ret1=ioctl(fd,FLASHGETS);
			
			if(ioctl_ret1<0)
			{
				printf("EEPROM is busy\n");
			}
			else
				printf("the eeprom is not busy !!\n");
			break;

	
			case 6:
		
			ioctl_ret2=ioctl(fd,FLASHGETP,&t);
			
			if(ioctl_ret2!=-1)
				printf("Current pointer position %d \n", t);
			else
				printf("eeprom was busy\n");
			break;

			case 7:
			
			printf("Erasing data ....\n");
			
			ioctl_ret4=ioctl(fd,FLASHERASE);
		
			if(ioctl_ret4!=-1)
			{
				printf("The erase was successful\n");
			}
			else
				printf("the eeprom was busy and clearing was unsuccessful\n");
			break;

			case 8:
			printf("releasing fd... \n");
			close(fd);
			exit(0);

			
			default:
			printf("None matched! \n");
			printf("releasing fd ...\n");
			close(fd);
			return -1;
		}
	
	}while(1);
	
	return 0;
}