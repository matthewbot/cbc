/*
       cbob_spi.c
       
       Created from linux-2.6.16-chumby-1.6.0/drivers/char/chumby_accel.c

       This file is part of the chumby accelerometer driver in the linux kernel.
       Copyright (c) Chumby Industries, 2007

       The accelerometer driver is free software; you can redistribute it and/or modify
       it under the terms of the GNU General Public License as published by
       the Free Software Foundation; either version 2 of the License, or
       (at your option) any later version.

       The accelerometer driver is distributed in the hope that it will be useful,
       but WITHOUT ANY WARRANTY; without even the implied warranty of
       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
       GNU General Public License for more details.

       You should have received a copy of the GNU General Public License
       along with the Chumby; if not, write to the Free Software
       Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "cbob_spi.h"

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/irqs.h>
#include <asm/arch/hardware.h>
#include <linux/interrupt.h>
#include <linux/wait.h>

static struct semaphore cbob_spi;

unsigned long cbob_spi_last_message;
static struct cbob_message *cbob_spi_current_message;
static short cbob_spi_current_cboblength;
DECLARE_COMPLETION(cbob_spi_message_completion);

#define CBOB_TRANSACTION_DELAY 1600

enum {
  CBOB_TRANSACTION_CHUMBYHEADER,
  CBOB_TRANSACTION_CHUMBYDATA,
  CBOB_TRANSACTION_CBOBLENGTH,
  CBOB_TRANSACTION_CBOBDATA,
  CBOB_TRANSACTION_END
};

static int cbob_spi_current_transaction;
static int cbob_spi_do_transaction(void);

static int cbob_spi_desync;
static void cbob_spi_update_desync(short replycount, short incount);

static void cbob_spi_init_timer(void);
static void cbob_spi_shutdown_timer(void);

#define TIMER IMX_TIM2_BASE
#define TIMER_IRQ TIM2_INT
static irqreturn_t cbob_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);

static void cbob_spi_init_spi(void);
static unsigned int spi_exchange_data(unsigned int dataTx);

void cbob_spi_init()
{
  cbob_spi_init_spi();
  cbob_spi_init_timer();
    
  sema_init(&cbob_spi, 1);
}

int cbob_spi_message(short cmd, short *outbuf, short outcount, short *inbuf, short incount)
{
  struct cbob_message msg;
  msg.cmd = cmd;
  msg.outbuf = outbuf;
  msg.outcount = outcount;
  msg.inbuf = inbuf;
  msg.incount = incount;
  
  return cbob_spi_sendmessage(&msg);
}

int cbob_spi_sendmessage(struct cbob_message *msg)
{
  if(down_interruptible(&cbob_spi))
    return -EINTR;
  
  cbob_spi_current_message = msg;
  cbob_spi_current_transaction = 0;

  // if enough time has passed since the last transaction
  if (time_after(jiffies, cbob_spi_last_message + usecs_to_jiffies(CBOB_TRANSACTION_DELAY)))
    cbob_spi_do_transaction(); // go ahead and do one now to save time
  
  IMX_TCTL(TIMER) |= TCTL_TEN; // enable timer
  wait_for_completion(&cbob_spi_message_completion);
  
  cbob_spi_last_message = jiffies;
  up(&cbob_spi);
  return 1;
}

static int cbob_spi_do_transaction()
{
  int i;
  struct cbob_message *msg = cbob_spi_current_message;
 
  if (cbob_spi_current_transaction == CBOB_TRANSACTION_END)
    return 1;
  
  switch (cbob_spi_current_transaction) {
    case CBOB_TRANSACTION_CHUMBYHEADER: {
      spi_exchange_data(0xCB07);
      spi_exchange_data(msg->cmd);
      spi_exchange_data(msg->outcount > 0 ? msg->outcount : 1);
      break;
    }
    
    case CBOB_TRANSACTION_CHUMBYDATA:
      if(msg->outcount == 0)
        spi_exchange_data(0);
      else {
        for(i = 0;i < msg->outcount;i++)
         spi_exchange_data(msg->outbuf[i]);
      }
      break;
      
    case CBOB_TRANSACTION_CBOBLENGTH:
      cbob_spi_current_cboblength = spi_exchange_data(0);
      spi_exchange_data(0);
      cbob_spi_update_desync(cbob_spi_current_cboblength, msg->incount);
      break;
      
    case CBOB_TRANSACTION_CBOBDATA:
      for(i = 0;i < cbob_spi_current_cboblength;i++) {
        if(i < msg->incount)
          msg->inbuf[i] = spi_exchange_data(0);
        else 
          spi_exchange_data(0);
      }
      break;
  }
  
  if (++cbob_spi_current_transaction == CBOB_TRANSACTION_END) {
    complete(&cbob_spi_message_completion);
    return 1;
  } else
    return 0;
}

void cbob_spi_exit(void) 
{
  cbob_spi_shutdown_timer();
}

static void cbob_spi_init_timer(void)
{
  IMX_TCTL(TIMER) = TCTL_SWR; // reset timer
  udelay(100);
  
  IMX_TCTL(TIMER) = TCTL_CC // counter clear when timer is disabled
                  | TCTL_COMPEN // enable compare mode
                  | TCTL_CLK_32; // use 32khz clock source
  IMX_TCMP(TIMER) = (int)(CBOB_TRANSACTION_DELAY / 30.52); // 30.52 us per tick
  
  request_irq(TIMER_IRQ, cbob_timer_interrupt, 0, "CBOB", 0);
}

static void cbob_spi_shutdown_timer(void)
{
  IMX_TCTL(TIMER) &= ~TCTL_TEN; // stop timer
  free_irq(TIMER_IRQ, 0);
}

static irqreturn_t cbob_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  if (!(IMX_TSTAT(TIMER) & TSTAT_COMP))
    return IRQ_NONE;

  IMX_TSTAT(TIMER) = TSTAT_CAPT | TSTAT_COMP; // clear interrupts
  
  if (cbob_spi_do_transaction()) // if transaction finished
    IMX_TCTL(TIMER) &= ~TCTL_TEN; // shut off timer
    
  return IRQ_HANDLED;
}

static void cbob_spi_update_desync(short replycount, short incount) {
  int desync = (replycount > incount || replycount <= 0);
  
  if (desync == cbob_spi_desync)
    return;
    
  cbob_spi_desync = desync;
  
  if (desync)
    printk(KERN_WARNING "CBOB desync detected. replycount is %hd, incount is %hd\n", replycount, incount);
  else
    printk(KERN_NOTICE "CBOB resynced.\n");
}

/* Most of the following code was taken from chumby_accel.c
 * Thanks go to Chumby for providing this :)
 */
 
#define CLK_DIVIDER  ((PCDR1 & PCDR1_PERDIV2_MASK) >> PCDR1_PERDIV2_POS)
#define BASE_FREQ    350
#define TARGET_FREQ  5

#define SPI_CHAN 0

static int spi_tx_fifo_empty(void)
{ return (SSP_INT_REG(SPI_CHAN) & SSP_INT_TE);}

static int spi_rx_fifo_data_ready(void)
{ return (SSP_INT_REG(SPI_CHAN) & SSP_INT_RR);}

static unsigned int spi_exchange_data(unsigned int dataTx) 
{
  while(!spi_tx_fifo_empty());

  SSP_TX_REG(SPI_CHAN)   = dataTx;	     // transfer data
  SSP_CTRL_REG(SPI_CHAN) |= SSP_XCH;         // exchange data
    
  while(!spi_rx_fifo_data_ready());

  return SSP_RX_REG(SPI_CHAN);
}
  
static void cbob_spi_init_spi(void) 
{
  // hardware init
  // map GPIO ports appropriately
  // CSPI1_SS1    F16    CSPI1_SS1 (GPIO   )     PD27  out 1  ** not controlled by CSPI 
  // CSPI1_SS0    F19    CSPI1_SS0 (GPIO   )     PD28  out 1  ** not controlled by CSPI
  // CSPI1_SCLK   H10    CSPI1_SCLK(primary)     PD29  out 1
  // CSPI1_MISO   F17    CSPI1_MISO(primary)     PD30  in  0
  // CSPI1_MOSI   J12    CSPI1_MOSI(primary)     PD31  out 1
  // ** note that SS0,1 is decoded on the sensor card to 4 individual chipselects
  //    so these values have to be manually controlled by the software
  
  //imx_gpio_request_irq(CBOB_SERIAL_PIN, cbob_serial_handler, 0);
  //imx_gpio_mode( GPIO_PORTD | 27 | GPIO_IN | GPIO_GPIO | GPIO_IRQ_MASK | GPIO_IRQ_RISING | GPIO_IRQ_HIGH );
  imx_gpio_mode( GPIO_PORTD | 28 | GPIO_OUT | GPIO_PF );

  imx_gpio_mode( GPIO_PORTD | 29 | GPIO_OUT | GPIO_PF );
  imx_gpio_mode( GPIO_PORTD | 30 | GPIO_IN  | GPIO_PF );
  imx_gpio_mode( GPIO_PORTD | 31 | GPIO_OUT | GPIO_PF );

  // turn on the clock
  PCCR0 |= PCCR0_CSPI1_EN;

  // setup the SPI interfaces
  // reset the interface
  SSP_RESET_REG(SPI_CHAN) = 0x00000001;
  udelay(200);      //wait

  // use 32.768kHz clock speed, with 0 clock insertion
  SSP_PER_REG(SPI_CHAN) = 0x00008000;

  // init the hardware config register
  // 31-24 00000000
  //    23 0     BURST off
  //    22 0     SDHC off
  //    21 0     SWAP off
  // 20-19 00    SS asserted
  // 18-14 00000 ((BASE_FREQ/CLK_DIVIDER) / (TARGET_FREQ) >> 1)
  // 13-12 00    DRCTL ignore ready
  //    11 1     MODE = master
  //    10 1*    SPIEN enable interface (toggle to reset Tx FIFO)
  //     9 0     XCH  exchange off for now
  //     8 0     SSPOL is active low
  //     7 1     SSCTL CS toggles between bursts
  //     6 1     PHA phase 1 operation (falling edge)
  //     5 1     POL (active 1 idle)
  //   4-0 01111 BITCOUNT 16-bit transfer for Kionix
  SSP_CTRL_REG(SPI_CHAN) = 0;
  SSP_CTRL_REG(SPI_CHAN) |= (SSP_MODE_MASTER | SSP_ENABLE | SSP_SS_PULSE | SSP_PHA1 | SSP_POL1 | SSP_WS(16) );
  SSP_CTRL_REG(SPI_CHAN) |= (((BASE_FREQ/CLK_DIVIDER) / (TARGET_FREQ) >> 1) + 1) << 14;
  SSP_CTRL_REG(SPI_CHAN) &= ~SSP_MODE_MASTER;  // reset fifo

  udelay(100);      //wait
  SSP_CTRL_REG(SPI_CHAN) &= 0xFFFFFFE0;
  SSP_CTRL_REG(SPI_CHAN) |= SSP_WS(16);
  SSP_CTRL_REG(SPI_CHAN) |= SSP_MODE_MASTER;  // reset fifo
}

