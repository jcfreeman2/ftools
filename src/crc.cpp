#include "crc.h"

static uint8_t bitReversed( uint8_t byt );

// ----------------------------------------------------------------------------

uint16_t crc16( uint8_t *data, int nbytes )
{
  // Calculate CRC-16 value; uses The CCITT-16 Polynomial,
  // expressed as X^16 + X^12 + X^5 + 1
  // NB: for GBT-SCA packets need to bit-reverse each byte
  //     because the bytes are transmitted LSB to MSB (?) (added 28 Oct 2016)
  uint16_t crc = (uint16_t) 0xFFFF;
  int      index, b;
  uint8_t  tmp;
  for( index=0; index<nbytes; ++index )
    {
      //crc ^= (((uint16_t) data[index]) << 8); // Non bit-reversed
      tmp = bitReversed( data[index] );
      crc ^= (((uint16_t) tmp) << 8);
      for( b=0; b<8; ++b )
        {
          if( crc & (uint16_t) 0x8000 )
            crc = (crc << 1) ^ (uint16_t) 0x1021;
          else
            crc = (crc << 1);
        }
    }
  uint8_t tmp1, tmp2;
  tmp1 = bitReversed( (uint8_t) ((crc >> 8) & 0xFF) );
  tmp2 = bitReversed( (uint8_t) (crc & 0xFF) );
  crc = (uint16_t) tmp2 | ((uint16_t) tmp1 << 8);
  return crc;
}

// ----------------------------------------------------------------------------

static const uint8_t NibbleReversed[16] =
  {
   0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
   0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
  };

static uint8_t bitReversed( uint8_t byt )
{
  // Reverse the top and bottom nibble and swap them
  return( (NibbleReversed[byt & 0xF] << 4) | NibbleReversed[byt >> 4] );
}

// ----------------------------------------------------------------------------
/*
static uint8_t bitReversed_old( uint8_t byt )
{
  // Return 8-bit value 'byt' bit-reversed
  uint8_t tmp = 0;
  for( int i=0; i<8; ++i, byt>>=1 )
    {
      tmp <<= 1;
      if( byt & 1 )
        tmp |= 1;
    }
  return tmp;
}
*/
// ----------------------------------------------------------------------------
