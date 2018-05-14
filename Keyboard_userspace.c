/*
 * lab6p2_usp.c
 *
 *  Created on: Nov 16, 2016
 *      Author: zxdhf
 */

/*
 * lab5.c
 *
 *  Created on: Oct 27, 2016
 *      Author: zxdhf
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>
#define PORT 5000
#define TRUE 1

#ifndef BOOL
#define BOOL int
#endif

int sockID;
struct sockaddr_in client_addr;
struct sockaddr_in broadcast_addr;
//struct ifreq ifr;
int client_addr_len;
void *Thrd_func(void *ptr);
void *Thrd_func_readkernel(void *ptr);
int broadcast_addr_len;
int master;

int main(void)
{
    char buff1[50];
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("10.3.52.255");
    client_addr_len = sizeof(struct sockaddr_in);
    broadcast_addr_len = sizeof(struct sockaddr_in);
    master = 0;
    int slave = 0;
    int randomNum = 0;
    //char Rndnum[3] = {" 0\n"};
    char buff2[256]={"Zhentao is the Master"};
    int pipe_TtoM;
    int pipe_UtoK;
    system("mkfifo TtoM");
    //system("mkfifo UtoM");
    struct RMESSAGE
    {
        char read_msg[50];
    }rmsg;

    struct SMESSAGE
    {
        char send_msg[50];
    }smsg;




    ////////////////////////Interrupt registers/////////////////////
    unsigned long *ptr;
    unsigned long *VIC2SoftInt;
    int fp;
    fp = open("/dev/mem",O_RDWR);
    if(fp == -1)
    {
        printf("\n open map error\n");
        return(-1);  // failed open
    }
    ptr = (unsigned long*)mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fp,0x800c0000);
    if(ptr == MAP_FAILED)
    {
        printf("\n Unable to map memory space \n");
        return(-1);
    }  // failed mmap
    VIC2SoftInt = ptr+6;



    //////////////get my address ////////////////////////////////////////
    char hostName[50];
    gethostname(hostName,sizeof(hostName));
    struct hostent *host;
    struct in_addr **boardIPList;
    host = (struct hostent*)gethostbyname(hostName);
    boardIPList = (struct in_addr**)host->h_addr_list;
    char *myaddr;
    myaddr = inet_ntoa(*boardIPList[0]); //10.3.52.17
    char fixedMyaddr[50];
    strcpy(fixedMyaddr,myaddr);
    printf("My fixed address is %s\n",fixedMyaddr);
    ///////////////////////////////////////////////////////////////////

    char *cpymyaddr=malloc(50*sizeof(char));
    strcpy(cpymyaddr,myaddr);
    char *cpybuff1_1 = malloc(50*sizeof(char));
    char *cpybuff1_2 = malloc(50*sizeof(char));
    //char *myaddr_Rndnum = malloc(50*sizeof(char));
    char myaddr_Rndnum[50];

    ////////////////get my_PC////////////////////////////////////
    char *token1 = strtok(myaddr,".");
    int j;
    for(j=0; j<3; j++)
    {
        //	printf("token1 is %s\n",token1);
        token1 = strtok(NULL,".");
    }
    int my_PC = atoi(token1); //get my IP number 17
    printf("My_PC is %d\n",my_PC);



    /////////////////build a thread to receive from////////////////////////
    pthread_t thrd;
    pthread_create(&thrd, NULL, Thrd_func, NULL);

    ///////////////////build thrd_K to read data from kernel//////////////
    pthread_t thrd_K;
    pthread_create(&thrd_K, NULL, Thrd_func_readkernel, NULL);


    ///////////////build FIFO TtoM thread to main/////////////////////////
    if((pipe_TtoM = open("TtoM", O_RDONLY))<0)
    {
        printf("Pipe TtoM open in read side in main function error\n");
        exit(-1);
    }
    else
    {
        printf("pipe_TtoM open in read side in main function open successfully\n");
    }

    ///////////build FIFO UtoK user space program to kernel module//////
    if((pipe_UtoK = open("/dev/rtf/1",O_WRONLY))<0)  //open a fifo 1 to read kernel
    {
        printf("pipe 1 open in main func U.S.P error\n");
        exit(-1);
    }
    else
    {
        printf("pipe 1 in main func U.S.P open successfully\n");
    }




    while(1)
    {
        /////////// read from pipe TtoM //////////////////////////////////
        memset(buff1,0,sizeof(buff1));
        memset(rmsg.read_msg,0,50*sizeof(char));
        memset(smsg.send_msg,0,50*sizeof(char));
        printf("Waiting to read pipe TtoM\n");
        if(read(pipe_TtoM,&rmsg,sizeof(rmsg))<0)
        {
            printf("pipe_TtoM read error\n");
            exit(-1);
        }
        else
        {
            strcpy(buff1,rmsg.read_msg);
            //printf("read pipe_TtoM in main successfully\n");
            printf("The read buff1 is %s\n",buff1);
        }

        //////////// receive command WHOIS /////////////////////////////
        if((strcmp(buff1,"WHOIS\n"))==0)
        {
            if(master==1)
            {
                //unicast
                sendto(sockID, buff2, sizeof(buff2), 0, (struct sockaddr*)&client_addr, client_addr_len);
                printf("The master=1\n");
            }
            else
            {
                printf("The master=0\n");
            }
        }

        /////////// receive command VOTE ////////////////////////////////
        else if((strcmp(buff1,"VOTE\n"))==0)
        {
            memset(myaddr_Rndnum,0,sizeof(myaddr_Rndnum));
            master = 0;
            slave = 0;
            srand(time(NULL));
            randomNum = (rand()%10)+1;
            printf("randomNUM=%d\n",randomNum);
            printf("My address is %s\n",cpymyaddr);
            sprintf(myaddr_Rndnum,"# %s %d\n",cpymyaddr,randomNum);
            printf("myaddr_Rndnum is %s\n",myaddr_Rndnum);

            //char myaddr_Rndnum[50] = {"# 10.3.52.15 1"};

            sendto(sockID, myaddr_Rndnum, sizeof(myaddr_Rndnum),0,(struct sockaddr*)&broadcast_addr,broadcast_addr_len);
            printf("The message that I send to friends is %s\n",myaddr_Rndnum);
        }

        //////////// receive others address and number //////////////////////////////
        else if(buff1[0]=='#')
        {
            printf("The message from friends is: %s\n",buff1);
            strcpy(cpybuff1_1,buff1);
            strcpy(cpybuff1_2,buff1);
            //get friend_PC
            char *token2 = strtok(cpybuff1_1,". ");
            int i;
            for(i=0;i<4;i++)
            {
                token2 = strtok(NULL,". ");
            }
            int friend_PC = atoi(token2);
            printf("friend PC is %d\n",friend_PC);

            //get friend random number
            char *token3 = strtok(cpybuff1_2,". ");
            int k;
            for(k=0;k<5;k++)
            {
                token3 = strtok(NULL,". ");
            }
            int friend_randNum = atoi(token3); //get friend random number
            printf("friend random number is %d\n",friend_randNum);

            if(randomNum == friend_randNum)
            {
                if(my_PC >= friend_PC)
                {
                    master = 1;
                }
                else
                {
                    master = 0;
                    slave = 1;
                }
            }
            else if(randomNum > friend_randNum)
            {
                master = 1;
            }
            else
            {
                master = 0;
                slave = 1;
            }

            if(slave == 1)
            {
                master = 0;
            }
            printf("My current master is %d\n",master);
        }

        //////////////////receive @ to change frequency//////////////////////////////
        else if(buff1[0]=='@')
        {

        	*VIC2SoftInt |= 0x80000000; //send interrupt signal
            if(write(pipe_UtoK, &buff1[1], sizeof(char))!=sizeof(char)) //write A to Kernel
            {
                printf("UtoK pipe write error\n");
                exit(-1);
            }
            else
            {
            	printf("Sent the frequency to kernel is %s\n",buff1);
            }



            if (master==1)
            {
            	printf("Before broadcast frequency, the received IP addr is %s, my fixed IP is %s\n",inet_ntoa(client_addr.sin_addr),fixedMyaddr);
            	if(strcmp(fixedMyaddr, inet_ntoa(client_addr.sin_addr))!=0)
                {
            		printf("Broadcast frequency %s\n",buff1);
            		sendto(sockID, buff1, sizeof(buff1),0,(struct sockaddr*)&broadcast_addr,broadcast_addr_len);
                }

            }

        }






        /////////////// receive garbage //////////////////////////////
        else
        {
            printf("read garbage: %s\n",buff1);
        }

    }
    return 0;
}

void *Thrd_func(void *ptr)
{
    struct WMESSAGE
    {
        char write_msg[50];
    }wmsg;

    struct sockaddr_in serv_addr;
    char buff_recv[50];
    if((sockID = socket(AF_INET,SOCK_DGRAM,0))==-1)
    {
        printf("Create socket error\n");
        exit(0);
    }
    bzero((char *)&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);
    if(bind(sockID,(struct sockaddr*)&serv_addr, sizeof(serv_addr))<0)
    {
        printf("Error on binding\n");
        exit(0);
    }
    BOOL bBroadcast = TRUE;
    setsockopt(sockID, SOL_SOCKET, SO_BROADCAST, (const char*)&bBroadcast, sizeof(BOOL));
    int pipe_TtoM;
    if((pipe_TtoM = open("TtoM",O_WRONLY))<0)
    {
        printf("pipe_TtoM in write side in thread function open error\n");
        exit(-1);
    }
    else
    {
        printf("pipe_TtoM open in write side in thread function open successfully\n");
    }
    while(1)
    {
        memset(wmsg.write_msg,0,50*sizeof(char));
        memset(buff_recv,0,sizeof(buff_recv));
        recvfrom(sockID, buff_recv, sizeof(buff_recv),0,(struct sockaddr*)&client_addr, &client_addr_len);
        printf("In thread function: Receive from %s\n",inet_ntoa(client_addr.sin_addr));
        printf("In thread function: The message received from client is:%s\n",buff_recv);

        strcpy(wmsg.write_msg, buff_recv);


        if(write(pipe_TtoM,&wmsg,sizeof(wmsg))!=sizeof(wmsg))
        {
            printf("pipe_TtoM in thread function write error\n");
            exit(-1);
        }
        else
        {
            //printf("write pipe_TtoM in thread successfully\n");
            printf("Thread write to main function is %s\n",wmsg.write_msg);
        }
    }

}


void *Thrd_func_readkernel(void *ptr)
{
	char letter;
	char Temp;
	int pipe_KtoU;
	char *p;
	char send_freq[50];
	send_freq[0] = '@';
	if((pipe_KtoU = open("/dev/rtf/2",O_RDONLY))<0)  //open a fifo 1 to read kernel
	{
		printf("pipe 2 read kernel open error\n");
		exit(-1);
	}
	else
	{
		printf("pipe 2 in thrd_func_readkernel open successfully\n");
	}
	while(1)
	{
		if((read(pipe_KtoU, &letter, sizeof(char)))<0) //read from kernel
		{
			printf("pipe 2 read error\n");
			exit(-1);
		}
		p = &letter;
		send_freq[1] = *p;
		if(master == 1)
		{
			sendto(sockID, send_freq, sizeof(send_freq),0,(struct sockaddr*)&broadcast_addr,broadcast_addr_len);
			printf("Extra broadcast is %s\n",send_freq);
		}

	}


}





































/*
 int main()
 {
	printf("This is the server side\n");
	int sockID;
	struct sockaddr_in serv_addr;
	struct sockaddr_in friend_addr;
	int friend_addr_len = sizeof(struct sockaddr_in);
	char buff1[256];
	char buff2[256]={"I am Master"};

	if((sockID = socket(AF_INET,SOCK_DGRAM,0))==-1)
	{
 printf("Create Socket Error\n");
 exit(0);
	}
	bzero((char *)&serv_addr,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);

	if(bind(sockID,(struct sockaddr*)&serv_addr, sizeof(serv_addr))<0)
	{
 printf("Error on binding\n");
 exit(0);
	}

	BOOL bBroadcast = TRUE;
	setsockopt(sockID, SOL_SOCKET, SO_BROADCAST, (const char*)&bBroadcast, sizeof(BOOL));

	while(1)
	{
 recvfrom(sockID, buff1, sizeof(buff1),0,(struct sockaddr*)&friend_addr, &friend_addr_len);
 printf("Receive from %s\n",inet_ntoa(friend_addr.sin_addr));
 printf("The message received from client is:%s\n",buff1);
 if((strcmp(buff1,"WHOIS\n")) == 0)
 {
 printf("The messages are matched\n");
 sendto(sockID, buff2, sizeof(buff2), 0, (struct sockaddr*)&friend_addr, friend_addr_len);
 }
 else
 {
 printf("Messages are not matched\n");
 }
	}
	return 0;
 }
 */
