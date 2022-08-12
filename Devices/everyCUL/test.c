#define F_CPU 8000000

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

#define USART_BAUD_RATE(BAUD_RATE) ((float)(F_CPU * 64 / (16 * (float)BAUD_RATE)) + 0.5)

// TX complete (data buffer empty)
ISR(USART3_DRE_vect)
{
  if (0/*TTY_Tx_Buffer.nbytes*/) {
    USART3.TXDATAL = 0;// rb_get(&TTY_Tx_Buffer);
  } else {
    USART3.CTRLA &= ~USART_DREIE_bm;
  }
}

// RX complete
ISR(USART3_RXC_vect)
{
  //LED_TOGGLE();

  /* read UART status register and UART data register */
  uint8_t data = USART3.RXDATAL;
  uint8_t usr  = USART3.RXDATAH; //status of receiver is in HIGH byte

  if ((usr & (USART_FERR_bm | USART_BUFOVF_bm)) == 0) //check if the received data is not from overflow or frame error
    (void)0; //rb_put(&TTY_Rx_Buffer, data);
}


void uart_init(unsigned int baudrate)
{
  int8_t sigrow_val = SIGROW.OSC16ERR5V;
  uint32_t baud_setting = (((8 * F_CPU) / baudrate) + 1) / 2;
  baud_setting += (baud_setting * sigrow_val) / 1024;
  USART3.BAUD = USART_BAUD_RATE(baudrate);//(uint16_t)baud_setting;

  PORTMUX.USARTROUTEA |= PORTMUX_USART3_ALT1_gc; //alt 1 mode to use pins connected to serial over usb

  //USART3.CTRLA |= USART_RXCIE_bm;// | USART_RXMODE_CLK2X_gc; //not sure if 2x mode needed
  USART3.CTRLB |= USART_RXEN_bm | USART_TXEN_bm | USART_RXMODE_NORMAL_gc;
  USART3.CTRLC |= USART_CMODE_ASYNCHRONOUS_gc | USART_CHSIZE_8BIT_gc | USART_PMODE_DISABLED_gc | USART_SBMODE_1BIT_gc;
  
  PORTB.PIN5CTRL |= PORT_PULLUPEN_bm;
  PORTB.DIR &= ~PIN5_bm; //Rx
  PORTB.OUT |= PIN4_bm;
  PORTB.DIR |= PIN4_bm; //Tx

}

void uart_flush(void)
{
  if (!(USART3.CTRLA & USART_DREIE_bm)/* && TTY_Tx_Buffer.nbytes*/)
	  USART3.CTRLA |= USART_DREIE_bm;
}

void uart_task(void)
{
  //input_handle_func(DISPLAY_USB);
  uart_flush();
}

void USART3_sendChar(char c)
{
    while (!(USART3.STATUS & USART_DREIF_bm))
    {
        ;
    }
    USART3.TXDATAL = c;
}

void USART3_sendString(char *str)
{
    for(size_t i = 0; i < strlen(str); i++)
    {
        USART3_sendChar(str[i]);
    }
}

// FUSE Settings
FUSES = {
    .WDTCFG     = FUSE_WDTCFG_DEFAULT,
    .BODCFG     = FUSE_BODCFG_DEFAULT,
    .OSCCFG     = FUSE_FREQSEL0,  // FREQSEL to 16MHz
    .SYSCFG0    = (FUSE_CRCSRC0 | FUSE_CRCSRC1 | FUSE_RSTPINCFG | FUSE_EESAVE),
    .SYSCFG1    = FUSE_SYSCFG1_DEFAULT,
    .APPEND     = FUSE_APPEND_DEFAULT,
    .BOOTEND    = FUSE_BOOTEND_DEFAULT,
};


int main()
{
  _PROTECTED_WRITE(CLKCTRL_MCLKCTRLB, (CLKCTRL_PEN_bm | CLKCTRL_PDIV_2X_gc));

  //_PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_2X_gc | CLKCTRL_PEN_bm);
  //_PROTECTED_WRITE(CLKCTRL.MCLKCTRLA, !CLKCTRL_CLKOUT_bm | CLKCTRL_CLKSEL_OSC20M_gc);

  uart_init(9600);

  PORTE.DIR |= PIN2_bm;

  while (1)
  {
    _delay_ms(500);
    PORTE.OUT |= PIN2_bm;
    USART3_sendString("Hello World!\n");
    _delay_ms(500);
    PORTE.OUT &= ~PIN2_bm;
  }
}
