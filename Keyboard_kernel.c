/*
 * lab6p2_kernel.c
 *
 *  Created on: Nov 16, 2016
 *      Author: zxdhf
 */

#ifndef MODULE
#define MODULE
#endif
#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <rtai_sched.h>
#include <rtai_fifos.h>
#include <linux/time.h>
#include <rtai.h>
#include <rtai_sem.h>



MODULE_LICENSE("GPL");

static RT_TASK mytask;
RTIME Basep;

unsigned long *PFDR,*PFDDR,*PBDR,*PBDDR;
unsigned long *GPIOBIntType1, *GPIOBIntType2, *GPIOBEOI, *GPIOBIntEn, *RawIntStsB;
unsigned long *VIC2IntEnable, *VIC2SoftIntClean;
unsigned long *GPIOBDB; // debounce port B
char recv_letter;
char send_letter;
struct MESSAGE
{
    char msg[50];
}get_msg,send_msg;

static void rt_process(int t)
{
    while(1)
    {
        *PFDR = *PFDR | 0x2; // Turn on speaker
        *PBDR |= 0x80; //10000000 Turn on green PortB7
        rt_task_wait_period();
        *PFDR = *PFDR & 0x5; //Turn off speaker
        *PBDR &= 0x7F; //01111111 Turn off green PortB7
        rt_task_wait_period();
    }
}


static void HW_ISR(unsigned irq, void *cookie)
{
    rt_disable_irq(59); //disable interrupt handling
    if(*RawIntStsB & 0x1) //button 1 is pushed
    {
    	send_letter = 'A';
    	rtf_put(2,&send_letter,sizeof(char));
    	rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,100*Basep);

    }
    else if(*RawIntStsB & 0x2) //button 2 is pushed
    {
    	send_letter = 'B';
    	rtf_put(2,&send_letter,sizeof(char));
    	rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,250*Basep);
    }
    else if(*RawIntStsB & 0x4) //button 3 is pushed
    {
    	send_letter = 'C';
    	rtf_put(2,&send_letter,sizeof(char));
    	rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,700*Basep);
    }
    else if(*RawIntStsB & 0x8) //button 4 is pushed
    {
    	send_letter = 'D';
    	rtf_put(2,&send_letter,sizeof(char));
    	rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,1500*Basep);
    }
    else if(*RawIntStsB & 0x10) //button 5 is pushed
    {
    	send_letter = 'E';
        rtf_put(2,&send_letter,sizeof(char));
    	rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,2500*Basep);
    }

    *GPIOBEOI |= 0x1F; //clear interrupt button state
    rt_enable_irq(59); //enable interrupt

}


static void SW_ISR(unsigned irq, void *cookie)
{
    rt_disable_irq(63);
    rtf_get(1,&recv_letter,sizeof(char));

    if (recv_letter == 'A')
    {
        rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,100*Basep);
    }
    else if(recv_letter == 'B')
    {
        rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,250*Basep);
    }
    else if(recv_letter == 'C')
    {
        rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,700*Basep);
    }
    else if(recv_letter == 'D')
    {
        rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,1500*Basep);
    }
    else if(recv_letter == 'E')
    {
        rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,2500*Basep);
    }

    *VIC2SoftIntClean |= 0x80000000;  //clean interrupt signal
    rt_enable_irq(63);                //re-enable interrup



}

int init_module(void)
{
    unsigned long *ptr1;  //create pointer to mmap
    ptr1 = (unsigned long*)__ioremap(0x80840000,4096,0);
    PBDR = ptr1+1;
    PBDDR = ptr1+5;
    PFDR = ptr1+12;
    PFDDR = ptr1+13; // give physical address
    *PBDDR = 0xE0;  // give LEDs output, buttons input. pbddr 11100000
    *PFDDR |= 0x02;  // give speaker output, pfddr 00000010
    *PBDR |= 0x40; //01000000 Turn on yellow PortB6
    GPIOBIntType1 = ptr1+43;
    GPIOBIntType2 = ptr1+44;
    GPIOBEOI = ptr1+45;
    GPIOBIntEn = ptr1+46;
    RawIntStsB = ptr1+47;
    GPIOBDB = ptr1+49;  //setup debounce
    *GPIOBDB |= 0x1F;

    //software interrupt register
    unsigned long *ptr2;
    ptr2 = (unsigned long*)__ioremap(0x800c0000,4096,0);
    VIC2IntEnable = ptr2 + 4;
    VIC2SoftIntClean = ptr2 + 7;


    //setup Hardware interrupt register
    rt_request_irq(59,HW_ISR,0,1);
    *GPIOBIntType1 |= 0x1F;  //edge
    //*GPIOBIntType2 |= 0x1F;  //rising
    *GPIOBIntType2 &= 0xE0; //falling
    *GPIOBIntEn |= 0x1F;     //available to send interrupt
    *GPIOBEOI |= 0x1F;       //clear interrupt
    rt_enable_irq(59);       //enable hardware interrupt handling

    //setup software interrupt register
    rt_request_irq(63,SW_ISR,0,1);
    *VIC2IntEnable |= 0x80000000;
    rt_enable_irq(63);       //enable software interrupt handling


    //setup real time task
    rt_set_periodic_mode(); //set to periodic mode
    Basep = start_rt_timer(nano2count(1000000));  //base period = 1 millisecond
    rt_task_init(&mytask,rt_process,0,256,0,0,0);
    rt_task_make_periodic(&mytask,rt_get_time()+0*Basep,500*Basep);  //start mytask, period 0.5 sec

    //create fifo 1
    rtf_create(1,sizeof(char));
    rtf_create(2,sizeof(char));

    return 0;
}


void cleanup_module(void)
{
    rt_release_irq(59);
    rt_release_irq(63);
    rt_task_delete(&mytask);
    stop_rt_timer();
    *PBDDR = 0xE0;  // give LEDs output, pbddr 11100000
    *PBDR &= 0x1F; //Turn off red yellow green LEDs
    rtf_destroy(1); //destroy fifo 1
    rtf_destroy(2); //destroy fifo 2

}

