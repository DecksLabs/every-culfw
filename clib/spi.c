#include <avr/io.h>
#include "spi.h"
#include "board.h"

void
spi_init(void)
{
    PORTE.DIR |= PIN0_bm; /* Set MOSI pin direction to output */
    PORTE.DIR &= ~PIN1_bm; /* Set MISO pin direction to input */
    PORTE.DIR |= PIN2_bm; /* Set SCK pin direction to output */
    PORTB.DIR |= PIN1_bm; /* Set CS (using PB1 for this) pin direction to output */

    PORTMUX.TWISPIROUTEA = PORTMUX_SPI0_ALT2_gc;

    SPI0.CTRLB |= SPI_SSD_bm; // we're not using SPI CS pin, we're using PB1 GPIO for CS

    SPI0.CTRLA = SPI_CLK2X_bm           /* Enable double-speed */
               | SPI_ENABLE_bm          /* Enable module */
               | SPI_MASTER_bm          /* SPI module in Master mode */
               | SPI_PRESC_DIV4_gc;    /* System Clock divided by 4 */

}

uint8_t
spi_send(uint8_t data)
{
    SPI0.DATA = data;

    while (!(SPI0.INTFLAGS & SPI_IF_bm))  /* waits until data is exchanged*/
    {
        ;
    }

    return SPI0.DATA;
}
