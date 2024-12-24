
#include <avr/interrupt.h>
#include <avr/io.h>
#include "ringbuffer.h"
#include "ttydata.h"
#include "display.h"
#include "led.h"
#include "serial.h"

#define USART_BAUD_RATE(BAUD_RATE) ((float)(F_CPU * 64 / (16 * (float)BAUD_RATE)) + 0.5)

void (*usbinfunc)(void);

// TX complete (data buffer empty)
ISR(USART2_DRE_vect)
{
  if (TTY_Tx_Buffer.nbytes) {
    USART2.TXDATAL = rb_get(&TTY_Tx_Buffer);
  } else {
    USART2.CTRLA &= ~USART_DREIE_bm;
  }
}

// RX complete
ISR(USART2_RXC_vect)
{
  /* read UART status register and UART data register */
  uint8_t data = USART2.RXDATAL;
  uint8_t usr  = USART2.RXDATAH; //status of receiver is in HIGH byte

  if ((usr & (USART_FERR_bm | USART_BUFOVF_bm)) == 0) //check if the received data is not from overflow or frame error
    rb_put(&TTY_Rx_Buffer, data);
}


void uart_init(unsigned int baudrate)
{
  PORTF.DIRCLR = PIN1_bm;
  PORTF.DIRSET = PIN0_bm;

  USART2.BAUD = USART_BAUD_RATE(baudrate);

  PORTMUX.USARTROUTEA |= PORTMUX_USART2_DEFAULT_gc; //alt 1 mode to use pins connected to serial over usb

  USART2.CTRLA |= USART_RXCIE_bm;// | USART_RXMODE_CLK2X_gc; //not sure if 2x mode needed
  USART2.CTRLB |= USART_RXEN_bm | USART_TXEN_bm | USART_RXMODE_NORMAL_gc;
  USART2.CTRLC |= USART_CMODE_ASYNCHRONOUS_gc | USART_CHSIZE_8BIT_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc;
}

void uart_task(void) 
{
  input_handle_func(DISPLAY_USB);
  uart_flush();
}

void uart_flush(void) 
{
  if (!(USART2.CTRLA & USART_DREIE_bm) && TTY_Tx_Buffer.nbytes)
	  USART2.CTRLA |= USART_DREIE_bm;
}
