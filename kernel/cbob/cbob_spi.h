#ifndef __CBC_SPI__
#define __CBC_SPI__

#include <linux/config.h>
#include <linux/types.h>
#include <asm/semaphore.h>

void cbob_spi_init(void);
void cbob_spi_exit(void);

struct cbob_message {
  short cmd;
  short *outbuf;
  short outcount;
  short *inbuf;
  short incount;
};

int cbob_spi_sendmessage(struct cbob_message *msg);
int cbob_spi_message(short cmd, short *outbuf, short outcount, short *inbuf, short incount);

#endif
