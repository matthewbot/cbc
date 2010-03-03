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

#define CBOB_TRANSACTION_DELAY 1200

static struct semaphore cbob_spi;

unsigned long cbob_spi_last_message;

static int cbob_spi_desync;
static void cbob_spi_update_desync(short replycount, short incount);

static void cbob_spi_init_regs(void);
static unsigned int spi_exchange_data(unsigned int dataTx);

void cbob_spi_init()
{
  cbob_spi_init_regs();
    
  sema_init(&cbob_spi, 1);
}

inline static void cbob_spi_wait_transaction(void)
{
	udelay(CBOB_TRANSACTION_DELAY);
}

int cbob_spi_message(short cmd, short *outbuf, short outcount, short *inbuf, short incount)
{
  int i;
  short header[3], replycount = 0;
  header[0] = 0xCB07;
  header[1] = cmd;
  header[2] = (outcount > 0 ? outcount : 1);
  
  if(down_interruptible(&cbob_spi))
    return -EINTR;
  
  // if we are starting a new transaction less than a transaction delay after the previous one
  if (time_before(jiffies, cbob_spi_last_message + usecs_to_jiffies(CBOB_TRANSACTION_DELAY)))
    cbob_spi_wait_transaction(); // we need to insert a transaction delay before starting
  
  for(i = 0;i < 3;i++)
    spi_exchange_data(header[i]);
  cbob_spi_wait_transaction();
  
  if(outcount == 0)
    spi_exchange_data(0);
  else {
    for(i = 0;i < outcount;i++)
      spi_exchange_data(outbuf[i]);
  }
  cbob_spi_wait_transaction();
  
  replycount = spi_exchange_data(0);
  cbob_spi_update_desync(replycount, incount);
  spi_exchange_data(0);
  cbob_spi_wait_transaction();
  
  for(i = 0;i < replycount;i++) {
    if(i < incount)
      inbuf[i] = spi_exchange_data(0);
    else 
      spi_exchange_data(0);
  }
  
  cbob_spi_last_message = jiffies;
  up(&cbob_spi);
  return 1;
}

void cbob_spi_exit(void) 
{
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
  
static void cbob_spi_init_regs(void) 
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

