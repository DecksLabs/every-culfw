/*
 * Copyright by D.Tostmann
 * Inspired by code from TI.com AN
 * License: GPL v2
 */

#include "board.h"
#ifdef HAS_MBUS
#include <string.h>
#include <avr/pgmspace.h>
#include "cc1100.h"
#include "delay.h"
#include "rf_receive.h"
#include "display.h"

#include "rf_mbus.h"
#include "mbus/mbus_defs.h"
#include "mbus/smode_rf_settings.h"
#include "mbus/tmode_rf_settings.h"
#include "mbus/mbus_packet.h"
#include "mbus/manchester.h"
#include "mbus/3outof6.h"

// Buffers
uint8 MBpacket[291];
uint8 MBbytes[584];

// Radio Mode
#define RADIO_MODE_NONE  0
#define RADIO_MODE_TX    1
#define RADIO_MODE_RX    2

uint8   radio_mode = RADIO_MODE_NONE;
uint8_t  mbus_mode = WMBUS_NONE;
RXinfoDescr RXinfo;
TXinfoDescr TXinfo;

static void halRfReadFifo(uint8* data, uint8 length, uint8 *rssi, uint8 *lqi) {
  CC1100_ASSERT;

  cc1100_sendbyte( CC1100_RXFIFO|CC1100_READ_BURST );
  for (uint8_t i = 0; i < length; i++)
    data[i] = cc1100_sendbyte( 0 );

  if (rssi) {
    *rssi = cc1100_sendbyte( 0 );
    if (lqi) {
      *lqi =  cc1100_sendbyte( 0 );
    }
  }
  CC1100_DEASSERT;
}

uint8_t halRfWriteFifo(const uint8* data, uint8 length) {

    CC1100_ASSERT;

    cc1100_sendbyte( CC1100_TXFIFO|CC1100_WRITE_BURST );
    for (uint8_t i = 0; i < length; i++)
      cc1100_sendbyte( data[i] );

    CC1100_DEASSERT;

    return 1;
}


static void halRfWriteReg( uint8_t reg, uint8_t value ) {
  cc1100_writeReg( reg, value );
}

uint8_t halRfGetTxStatus(void) {
  return(ccStrobe(CC1100_SNOP));
}

static uint8_t rf_mbus_on(uint8_t force) {

  // already in RX?
  if (!force && (cc1100_readReg( CC1100_MARCSTATE ) == MARCSTATE_RX))
    return 0;

  // init RX here, each time we're idle
  RXinfo.state = 0;

  ccStrobe( CC1100_SIDLE );
  while((cc1100_readReg( CC1100_MARCSTATE ) != MARCSTATE_IDLE));
  ccStrobe( CC1100_SFTX  );
  ccStrobe( CC1100_SFRX  );

  // Initialize RX info variable
  RXinfo.lengthField = 0;           // Length Field in the wireless MBUS packet
  RXinfo.length      = 0;           // Total length of bytes to receive packet
  RXinfo.bytesLeft   = 0;           // Bytes left to to be read from the RX FIFO
  RXinfo.pByteIndex  = MBbytes;     // Pointer to current position in the byte array
  RXinfo.format      = INFINITE;    // Infinite or fixed packet mode
  RXinfo.start       = TRUE;        // Sync or End of Packet
  RXinfo.complete    = FALSE;       // Packet Received
  RXinfo.mode        = mbus_mode;   // Wireless MBUS radio mode
  RXinfo.framemode   = WMBUS_NONE;  // Received frame mode (Distinguish between C- and T-mode)
  RXinfo.frametype   = 0;           // Frame A or B in C-mode

  // Set RX FIFO threshold to 4 bytes
  halRfWriteReg(CC1100_FIFOTHR, RX_FIFO_START_THRESHOLD);
  // Set infinite length
  halRfWriteReg(CC1100_PKTCTRL0, INFINITE_PACKET_LENGTH);

  ccStrobe( CC1100_SRX   );
  while((cc1100_readReg( CC1100_MARCSTATE ) != MARCSTATE_RX));

  RXinfo.state = 1;

  return 1; // this will indicate we just have re-started RX
}

static void rf_mbus_init(uint8_t mmode, uint8_t rmode) {

  PORTF.DIRCLR = PIN5_bm; //CLEAR_BIT( GDO0_DDR, GDO0_BIT ); //PF5
  PORTA.DIRCLR = PIN0_bm; //CLEAR_BIT( GDO2_DDR, GDO2_BIT ); //PA0

  mbus_mode  = WMBUS_NONE;
  radio_mode = RADIO_MODE_NONE;

  PORTD.PIN2CTRL &= ~PORT_ISC_gm; //EIMSK &= ~_BV(CC1100_INT);                 // disable INT - we'll poll...
  PORTB.DIR |= PIN1_bm; //SET_BIT( CC1100_CS_DDR, CC1100_CS_PIN );   // CS as output

  CC1100_DEASSERT;                           // Toggle chip select signal
  my_delay_us(30);
  CC1100_ASSERT;
  my_delay_us(30);
  CC1100_DEASSERT;
  my_delay_us(45);

  ccStrobe( CC1100_SRES );                   // Send SRES command
  my_delay_us(100);

  // load configuration
  switch (mmode) {
    case WMBUS_SMODE:
      for (uint8_t i = 0; i<200; i += 2) {
        if (sCFG(i)>0x40)
          break;
        cc1100_writeReg( sCFG(i), sCFG(i+1) );
      }
      break;
    case WMBUS_TMODE:
    // T-mode settings work for C-mode, which allows receiving
    // both T-mode and C-mode frames simultaneously. See SWRA522D.
    case WMBUS_CMODE:
      for (uint8_t i = 0; i<200; i += 2) {
        if (tCFG(i)>0x40)
          break;
        cc1100_writeReg( tCFG(i), tCFG(i+1) );
      }
      break;
    default:
      return;
  }

  if (rmode == RADIO_MODE_TX) {
    // IDLE after TX and RX
    halRfWriteReg(CC1100_MCSM1, 0x00);

    // Set FIFO threshold
    halRfWriteReg(CC1100_FIFOTHR, TX_FIFO_THRESHOLD);

    if (mmode == WMBUS_SMODE) {
      // SYNC
      // The TX FIFO must apply the last byte of the
      // Synchronization word
      halRfWriteReg(CC1100_SYNC1, 0x54);
      halRfWriteReg(CC1100_SYNC0, 0x76);
    } else {
      // SYNC
      halRfWriteReg(CC1100_SYNC1, 0x54);
      halRfWriteReg(CC1100_SYNC0, 0x3D);

      // Set Deviation to 50 kHz
      halRfWriteReg(CC1100_DEVIATN, 0x50);

      // Set data rate to 100 kbaud
      halRfWriteReg(CC1100_MDMCFG4, 0x5B);
      halRfWriteReg(CC1100_MDMCFG3, 0xF8);

    }

    // Set GDO0 to be TX FIFO threshold signal
    halRfWriteReg(CC1100_IOCFG0, 0x02);
    // Set GDO2 to be high impedance
    halRfWriteReg(CC1100_IOCFG2, 0x2e);
  }

  mbus_mode  = mmode;
  radio_mode = rmode;

  ccStrobe( CC1100_SCAL );

  memset( &RXinfo, 0, sizeof( RXinfo ));
  memset( &TXinfo, 0, sizeof( TXinfo ));

  my_delay_ms(4);
}

void rf_mbus_task(void) {
  uint8 bytesDecoded[2];
  uint8 fixedLength;

  if (radio_mode != RADIO_MODE_RX)
    return;

  if (mbus_mode == WMBUS_NONE)
    return;

  switch (RXinfo.state) {
    case 0:
      rf_mbus_on( TRUE );
      return;

     // RX active, awaiting SYNC
    case 1:
      if ((PORTA.IN & PIN0_bm)) {
        RXinfo.state = 2;
      }
      break;

    // awaiting pkt len to read
    case 2:
      if ((PORTF.IN & PIN5_bm)) {
        // Read the 3 first bytes
        halRfReadFifo(RXinfo.pByteIndex, 3, NULL, NULL);

        // - Calculate the total number of bytes to receive -
        if (RXinfo.mode == WMBUS_SMODE) {
          // S-Mode
          // Possible improvment: Check the return value from the deocding function,
          // and abort RX if coding error.
          if (manchDecode(RXinfo.pByteIndex, bytesDecoded) != MAN_DECODING_OK) {
            RXinfo.state = 0;
            return;
          }
          RXinfo.lengthField = bytesDecoded[0];
          RXinfo.length = byteSize(1, 0, (packetSize(RXinfo.lengthField)));
        } else {
          // In C-mode we allow receiving T-mode because they are similar. To not break any applications using T-mode,
          // we do not include results from C-mode in T-mode.

          // If T-mode preamble and sync is used, then the first data byte is either a valid 3outof6 byte or C-mode
          // signaling byte. (http://www.ti.com/lit/an/swra522d/swra522d.pdf#page=6)
          if (RXinfo.mode == WMBUS_CMODE && RXinfo.pByteIndex[0] == 0x54) {
            RXinfo.framemode = WMBUS_CMODE;
            // If we have determined that it is a C-mode frame, we have to determine if it is Type A or B.
            if (RXinfo.pByteIndex[1] == 0xCD) {
              RXinfo.frametype = WMBUS_FRAMEA;

              // Frame format A
              RXinfo.lengthField = RXinfo.pByteIndex[2];

              if (RXinfo.lengthField < 9) {
                RXinfo.state = 0;
                return;
              }

              // Number of CRC bytes = 2 * ceil((L-9)/16) + 2
              // Preamble + L-field + payload + CRC bytes
              RXinfo.length = 2 + 1 + RXinfo.lengthField + 2 * (2 + (RXinfo.lengthField - 10)/16);
            } else if (RXinfo.pByteIndex[1] == 0x3D) {
              RXinfo.frametype = WMBUS_FRAMEB;
              // Frame format B
              RXinfo.lengthField = RXinfo.pByteIndex[2];

              if (RXinfo.lengthField < 12 || RXinfo.lengthField == 128) {
                RXinfo.state = 0;
                return;
              }

              // preamble + L-field + payload
              RXinfo.length = 2 + 1 + RXinfo.lengthField;
            } else {
              // Unknown type, reset.
              RXinfo.state = 0;
              return;
            }
          // T-Mode
          // Possible improvment: Check the return value from the deocding function,
          // and abort RX if coding error.
          } else if (decode3outof6(RXinfo.pByteIndex, bytesDecoded, 0) != DECODING_3OUTOF6_OK) {
            RXinfo.state = 0;
            return;
          } else {
            RXinfo.framemode = WMBUS_TMODE;
            RXinfo.frametype = WMBUS_FRAMEA;
            RXinfo.lengthField = bytesDecoded[0];
            RXinfo.length = byteSize(0, 0, (packetSize(RXinfo.lengthField)));
          }
        }

        // check if incoming data will fit into buffer
        if (RXinfo.length > sizeof(MBbytes)) {
          RXinfo.state = 0;
          return;
        }

        // we got the length: now start setup chip to receive this much data
        // - Length mode -
        // Set fixed packet length mode is less than 256 bytes
        if (RXinfo.length < (MAX_FIXED_LENGTH)) {
          halRfWriteReg(CC1100_PKTLEN, (uint8)(RXinfo.length));
          halRfWriteReg(CC1100_PKTCTRL0, FIXED_PACKET_LENGTH);
          RXinfo.format = FIXED;
        }

        // Infinite packet length mode is more than 255 bytes
        // Calculate the PKTLEN value
        else {
          fixedLength = RXinfo.length  % (MAX_FIXED_LENGTH);
          halRfWriteReg(CC1100_PKTLEN, (uint8)(fixedLength));
        }

        RXinfo.pByteIndex += 3;
        RXinfo.bytesLeft   = RXinfo.length - 3;

        // Set RX FIFO threshold to 32 bytes
        RXinfo.start = FALSE;
        RXinfo.state = 3;
        halRfWriteReg(CC1100_FIFOTHR, RX_FIFO_THRESHOLD);
      }
      break;

    // awaiting more data to be read
    case 3:
      if ((PORTF.IN & PIN5_bm)) {
        // - Length mode -
        // Set fixed packet length mode is less than MAX_FIXED_LENGTH bytes
        if (((RXinfo.bytesLeft) < (MAX_FIXED_LENGTH )) && (RXinfo.format == INFINITE)) {
          halRfWriteReg(CC1100_PKTCTRL0, FIXED_PACKET_LENGTH);
          RXinfo.format = FIXED;
        }

        // Read out the RX FIFO
        // Do not empty the FIFO (See the CC110x or 2500 Errata Note)
        halRfReadFifo(RXinfo.pByteIndex, RX_AVAILABLE_FIFO - 1, NULL, NULL);

        RXinfo.bytesLeft  -= (RX_AVAILABLE_FIFO - 1);
        RXinfo.pByteIndex += (RX_AVAILABLE_FIFO - 1);

      }
      break;
  }

  // END OF PAKET
  if (!(PORTA.IN & PIN0_bm) && RXinfo.state>1) {
    uint8_t rssi = 0;
    uint8_t lqi = 0;
    halRfReadFifo(RXinfo.pByteIndex, (uint8)RXinfo.bytesLeft, &rssi, &lqi);
    RXinfo.complete = TRUE;

    // decode!
    uint16_t rxStatus = PACKET_CODING_ERROR;
    uint16_t rxLength;

    if (RXinfo.mode == WMBUS_SMODE) {
      rxStatus = decodeRXBytesSmode(MBbytes, MBpacket, packetSize(RXinfo.lengthField));
      rxLength = packetSize(MBpacket[0]);
    } else if (RXinfo.framemode == WMBUS_TMODE) {
      rxStatus = decodeRXBytesTmode(MBbytes, MBpacket, packetSize(RXinfo.lengthField));
      rxLength = packetSize(MBpacket[0]);
    } else if (RXinfo.framemode == WMBUS_CMODE) {
      if (RXinfo.frametype == WMBUS_FRAMEA) {
        rxLength = RXinfo.lengthField + 2 * (2 + (RXinfo.lengthField - 10)/16) + 1;
        rxStatus = verifyCrcBytesCmodeA(MBbytes + 2, MBpacket, rxLength);
      } else if (RXinfo.frametype == WMBUS_FRAMEB) {
        rxLength = RXinfo.lengthField + 1;
        rxStatus = verifyCrcBytesCmodeB(MBbytes + 2, MBpacket, rxLength);
      }
    }

    if (rxStatus == PACKET_OK) {

      DC( 'b' );
      if (RXinfo.mode == WMBUS_CMODE) {
        if (RXinfo.frametype == WMBUS_FRAMEB) {
          DC('Y'); // special marker for frame type B to make it distinguishable 
                   // from frame type A
        }
      }

      for (uint8_t i=0; i < rxLength; i++) {
        DH2( MBpacket[i] );
  //      DC( ' ' );
      }

      if (tx_report & REP_RSSI) {
        DH2(lqi);
        DH2(rssi);
      }
      DNL();
    }
    RXinfo.state = 0;
    return;
  }

  rf_mbus_on( FALSE );
}

#ifndef MBUS_NO_TX

uint16 txSendPacket(uint8* pPacket, uint8* pBytes, uint8 mode) {
  uint16  bytesToWrite;
  uint16  fixedLength;
  uint8   txStatus;
  uint8   lastMode = WMBUS_NONE;
  uint16  packetLength;

  // Calculate total number of bytes in the wireless MBUS packet
  packetLength = packetSize(pPacket[0]);

  // Check for valid length
  if ((packetLength == 0) || (packetLength > 290))
    return TX_LENGTH_ERROR;

  // are we coming from active RX?
  if (radio_mode == RADIO_MODE_RX)
    lastMode = mbus_mode;

  rf_mbus_init( mode, RADIO_MODE_TX);

  // - Data encode packet and calculate number of bytes to transmit
  // S-mode
  if (mode == WMBUS_SMODE) {
    encodeTXBytesSmode(pBytes, pPacket, packetLength);
    TXinfo.bytesLeft = byteSize(1, 1, packetLength);
  }

  // T-mode
  else {
    encodeTXBytesTmode(pBytes, pPacket, packetLength);
    TXinfo.bytesLeft = byteSize(0, 1, packetLength);
  }

  // Check TX Status
  txStatus = halRfGetTxStatus();
  if ( (txStatus & CC1100_STATUS_STATE_BM) != CC1100_STATE_IDLE ) {
    ccStrobe(CC1100_SIDLE);
    return TX_STATE_ERROR;
  }

  // Flush TX FIFO
  // Ensure that FIFO is empty before transmit is started
  ccStrobe(CC1100_SFTX);

  // Initialize the TXinfo struct.
  TXinfo.pByteIndex   = pBytes;
  TXinfo.complete     = FALSE;

  // Set fixed packet length mode if less than 256 bytes to transmit
  if (TXinfo.bytesLeft < (MAX_FIXED_LENGTH) ) {
    fixedLength = TXinfo.bytesLeft;
    halRfWriteReg(CC1100_PKTLEN, (uint8)(TXinfo.bytesLeft));
    halRfWriteReg(CC1100_PKTCTRL0, FIXED_PACKET_LENGTH);
    TXinfo.format = FIXED;
  }

  // Else set infinite length mode
  else {
    fixedLength = TXinfo.bytesLeft % (MAX_FIXED_LENGTH);
    halRfWriteReg(CC1100_PKTLEN, (uint8)fixedLength);
    halRfWriteReg(CC1100_PKTCTRL0, INFINITE_PACKET_LENGTH);
    TXinfo.format = INFINITE;
  }

  // Fill TX FIFO
  bytesToWrite = MIN(TX_FIFO_SIZE, TXinfo.bytesLeft);
  halRfWriteFifo(TXinfo.pByteIndex, bytesToWrite);
  TXinfo.pByteIndex += bytesToWrite;
  TXinfo.bytesLeft  -= bytesToWrite;

  // Check for completion
  if (!TXinfo.bytesLeft)
    TXinfo.complete = TRUE;

  // Strobe TX
  ccStrobe(CC1100_STX);

  // Critical code section
  // Interrupts must be disabled between checking for completion
  // and enabling low power mode. Else if TXinfo.complete is set after
  // the check, and before low power mode is entered the code will
  // enter a dead lock.

  // Wait for available space in FIFO
  while (!TXinfo.complete) {

    if ((PORTF.IN & PIN5_bm)) {
      // Write data fragment to TX FIFO
      bytesToWrite = MIN(TX_AVAILABLE_FIFO, TXinfo.bytesLeft);
      halRfWriteFifo(TXinfo.pByteIndex, bytesToWrite);

      TXinfo.pByteIndex   += bytesToWrite;
      TXinfo.bytesLeft    -= bytesToWrite;

      // Indicate complete when all bytes are written to TX FIFO
      if (!TXinfo.bytesLeft)
	TXinfo.complete = TRUE;

      // Set Fixed length mode if less than 256 left to transmit
      if ((TXinfo.bytesLeft < (MAX_FIXED_LENGTH - TX_FIFO_SIZE)) && (TXinfo.format == INFINITE)) {
	halRfWriteReg(CC1100_PKTCTRL0, FIXED_PACKET_LENGTH);
	TXinfo.format = FIXED;
      }
    } else {

      // Check TX Status
      txStatus = halRfGetTxStatus();
      if ( (txStatus & CC1100_STATUS_STATE_BM) == CC1100_STATE_TX_UNDERFLOW ) {
	ccStrobe(CC1100_SFTX);
	return TX_STATE_ERROR;
      }

    }

  }

  while((cc1100_readReg( CC1100_MARCSTATE ) != MARCSTATE_IDLE));

  // re-enable RX if ...
  if (lastMode != WMBUS_NONE)
    rf_mbus_init( lastMode, RADIO_MODE_RX);

  return (TX_OK);
}
#endif


static void mbus_status(void) {
  if (radio_mode == RADIO_MODE_RX ) {
    switch (mbus_mode) {
    case WMBUS_SMODE:
      DS_P(PSTR("SMODE"));
      break;
    case WMBUS_TMODE:
      DS_P(PSTR("TMODE"));
      break;
    case WMBUS_CMODE:
      DS_P(PSTR("CMODE"));
      break;
    default:
      DS_P(PSTR("OFF"));
    }
  }
  else
    DS_P(PSTR("OFF"));
  DNL();
}

void rf_mbus_func(char *in) {
  if((in[1] == 'r') && in[2]) {     // Reception on
    if(in[2] == 's') {
      rf_mbus_init(WMBUS_SMODE,RADIO_MODE_RX);
    } else if(in[2] == 't') {
      rf_mbus_init(WMBUS_TMODE,RADIO_MODE_RX);
    } else if(in[2] == 'c') {
      rf_mbus_init(WMBUS_CMODE,RADIO_MODE_RX);
    } else {                        // Off
      rf_mbus_init(WMBUS_NONE,RADIO_MODE_NONE);
    }

  } else if(in[1] == 's') {         // Send

#ifndef MBUS_NO_TX

    uint8_t i = 0;
    while (in[2*i+3] && in [2*i+4]) {
      fromhex(in+3+(2*i), &MBpacket[i], 1);
      i++;
    }

    /*
    for (uint8_t i=0; i < packetSize(MBpacket[0]); i++) {
      DH2( MBpacket[i] );
      DC( ' ' );
    }
    DNL();
    */

    if(in[2] == 's') {
      txSendPacket(MBpacket, MBbytes, WMBUS_SMODE);
    } else if(in[2] == 't') {
      txSendPacket(MBpacket, MBbytes, WMBUS_TMODE);
    }

#else
    DS_P(PSTR("not compiled in\r\n"));
#endif

    return;
  }

  mbus_status();
}

#endif

// valid tx example data:
// bss0F44AE0C7856341201074447780B12436587255D
// - or -
// bst0F44AE0C7856341201074447780B12436587255D
