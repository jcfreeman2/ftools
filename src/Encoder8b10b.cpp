#include <iostream>
#include <iomanip>
using namespace std;

#include "Encoder8b10b.h"

// ----------------------------------------------------------------------------

static uint32_t bin2dec( const char *s )
{
  // Convert a string representing a binary number to an integer
  uint32_t val = 0;
  while( *s )
    {
      val <<= 1;
      if( *s == '1' )
        val += 1;
      else if( *s != '0' )
        break;
      ++s;
    }
  return val;
}

// ----------------------------------------------------------------------------
// Code translation tables for 8b10b encoding
// (RD = Running Disparity)

// 5b/6b code for data:
// Input (= array index)   : EDCBA  (ordered MSB to LSB)
// Output RD=-1, RD=+1 resp: abcdei (ordered LSB to MSB)
static uint32_t DLUT_5b6b[32][2] = {
  { bin2dec("100111"), bin2dec("011000") },
  { bin2dec("011101"), bin2dec("100010") },
  { bin2dec("101101"), bin2dec("010010") },
  { bin2dec("110001"), bin2dec("110001") },
  { bin2dec("110101"), bin2dec("001010") },
  { bin2dec("101001"), bin2dec("101001") },
  { bin2dec("011001"), bin2dec("011001") },
  { bin2dec("111000"), bin2dec("000111") },
  { bin2dec("111001"), bin2dec("000110") },
  { bin2dec("100101"), bin2dec("100101") },
    
  { bin2dec("010101"), bin2dec("010101") },
  { bin2dec("110100"), bin2dec("110100") },
  { bin2dec("001101"), bin2dec("001101") },
  { bin2dec("101100"), bin2dec("101100") },
  { bin2dec("011100"), bin2dec("011100") },
  { bin2dec("010111"), bin2dec("101000") },
  { bin2dec("011011"), bin2dec("100100") },
  { bin2dec("100011"), bin2dec("100011") },
  { bin2dec("010011"), bin2dec("010011") },
  { bin2dec("110010"), bin2dec("110010") },
    
  { bin2dec("001011"), bin2dec("001011") },
  { bin2dec("101010"), bin2dec("101010") },
  { bin2dec("011010"), bin2dec("011010") },
  { bin2dec("111010"), bin2dec("000101") },
  { bin2dec("110011"), bin2dec("001100") },
  { bin2dec("100110"), bin2dec("100110") },
  { bin2dec("010110"), bin2dec("010110") },
  { bin2dec("110110"), bin2dec("001001") },
  { bin2dec("001110"), bin2dec("001110") },
  { bin2dec("101110"), bin2dec("010001") },

  { bin2dec("011110"), bin2dec("100001") },
  { bin2dec("101011"), bin2dec("010100") }
};

// 3b/4b code for data:
// Input (= array index)   : HGF  (ordered MSB to LSB)
// Output RD=-1, RD=+1 resp: fghj (ordered LSB to MSB)
static uint32_t DLUT_3b4b[8+1][2] = {
  { bin2dec("1011"), bin2dec("0100") },
  { bin2dec("1001"), bin2dec("1001") },
  { bin2dec("0101"), bin2dec("0101") },
  { bin2dec("1100"), bin2dec("0011") },
  { bin2dec("1101"), bin2dec("0010") },
  { bin2dec("1010"), bin2dec("1010") },
  { bin2dec("0110"), bin2dec("0110") },
  { bin2dec("1110"), bin2dec("0001") }, //  D.x.P7, primary encoding of 7
  //
  // D.x.A7, alternate encoding of 7:
  // used when RD=-1 and x=17 or x=18 or x=20
  // used when RD=+1 and x=11 or x=13 or x=14
  { bin2dec("0111"), bin2dec("1000") } //  D.x.A7
};

/* ### Arrays KLUT_5b6b and KLUT_3b4b replaced by single array KLUT[12][2] ###
// 5b/6b code for data:
// Input (= array index)   : EDCBA  (ordered MSB to LSB)
// Output RD=-1, RD=+1 resp: abcdei (ordered LSB to MSB)
static uint32_t KLUT_5b6b[32][2] = {
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },

  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },

  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { DLUT_5b6b[23][0],  DLUT_5b6b[23][1] },  // K23
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { DLUT_5b6b[27][0],  DLUT_5b6b[27][1] },  // K27
  { bin2dec("001111"), bin2dec("110000") }, // K28
  { DLUT_5b6b[29][0],  DLUT_5b6b[29][1] },  // K29

  { DLUT_5b6b[30][0],  DLUT_5b6b[30][1] },  // K30
  { 0, 0 }
};

// 3b/4b code for control symbols:
// Input (= array index)   : HGF  (ordered MSB to LSB)
// Output RD=-1, RD=+1 resp: fghj (ordered LSB to MSB)
static uint32_t KLUT_3b4b[8][2] = {
  { bin2dec("1011"), bin2dec("0100") },
  { bin2dec("0110"), bin2dec("1001") },
  { bin2dec("1010"), bin2dec("0101") },
  { bin2dec("1100"), bin2dec("0011") },
  { bin2dec("1101"), bin2dec("0010") },
  { bin2dec("0101"), bin2dec("1010") },
  { bin2dec("1001"), bin2dec("0110") },
  { bin2dec("0111"), bin2dec("1000") }
};
*/

// Control symbols
static uint32_t KLUT[12][2] = {
  { bin2dec("0011110100"), bin2dec("1100001011") }, // K28.0
  { bin2dec("0011111001"), bin2dec("1100000110") }, // K28.1
  { bin2dec("0011110101"), bin2dec("1100001010") }, // K28.2
  { bin2dec("0011110011"), bin2dec("1100001100") }, // K28.3
  { bin2dec("0011110010"), bin2dec("1100001101") }, // K28.4
  { bin2dec("0011111010"), bin2dec("1100000101") }, // K28.5
  { bin2dec("0011110110"), bin2dec("1100001001") }, // K28.6
  { bin2dec("0011111000"), bin2dec("1100000111") }, // K28.7
  { bin2dec("1110101000"), bin2dec("0001010111") }, // K23.7
  { bin2dec("1101101000"), bin2dec("0010010111") }, // K27.7
  { bin2dec("1011101000"), bin2dec("0100010111") }, // K29.7
  { bin2dec("0111101000"), bin2dec("1000010111") }  // K30.7
};

// ----------------------------------------------------------------------------

Encoder8b10b::Encoder8b10b()
  : _errIndex( -1 )
{
}

// ----------------------------------------------------------------------------

Encoder8b10b::~Encoder8b10b()
{
}

// ----------------------------------------------------------------------------

bool Encoder8b10b::encode( uint32_t *data, uint32_t size,
                           bool soc_is_k287, bool fei4b_kchars )
{
  if( data == 0 ) return false;
  _errIndex = -1;

  uint32_t i, byte, b3, b5, b4, b6;
  int      index, rd = -1;
  int      soc_i, eoc_i, idle_i, sob_i, eob_i;
  if( fei4b_kchars )
    {
      soc_i  = 7;
      eoc_i  = 5;
      idle_i = 1;
      sob_i  = 2;
      eob_i  = 3;
    }
  else
    {
      if( soc_is_k287 )
        soc_i = 7; // K28.7
      else
        soc_i = 1; // K28.1
      eoc_i  = 6;  // K28.6
      idle_i = 5;  // K28.5
      sob_i  = 2;  // K28.2
      eob_i  = 3;  // K28.3
    }
  for( i=0; i<size; ++i )
    {
      byte = data[i];
      if( byte > 0xFF )
        {
          // Control symbol
          if( byte == SOC_CODE )
            index = soc_i;
          else if( byte == EOC_CODE )
            index = eoc_i;
          else if( byte == COMMA_CODE )
            index = idle_i;
          else if( byte == SOB_CODE )
            index = sob_i;
          else if( byte == EOB_CODE )
            index = eob_i;
          else
            {
              _errIndex = i;
              return false;
            }
          if( rd == -1 )
            data[i] = KLUT[index][0];
          else
            data[i] = KLUT[index][1];

          if( bitsInWord(data[i]) != 5 ) // Not equal number of 1s and 0s
            // Update running disparity (toggles between +1 and -1)
            rd *= (-1);
        }
      else
        {
          b5 = byte & 0x1F;
          b3 = (byte & 0xE0) >> 5;
          b6 = encode5b6b( b5, &rd );
          b4 = encode3b4b( b3, b5, &rd );
          data[i] = (b6 << 4) | b4;
        }
    }
  return true;
}

// ----------------------------------------------------------------------------

uint32_t Encoder8b10b::encode5b6b( uint32_t b5, int *rd )
{
  uint32_t code;
  if( *rd == -1 )
    code = DLUT_5b6b[b5][0];
  else
    code = DLUT_5b6b[b5][1];

  if( bitsInWord(code) != 3 ) // Not equal number of 1s and 0s
    *rd *= (-1); // Update running disparity (toggles between +1 and -1)

  // Exceptions
  if( code == 0x07 )
    *rd = 1;
  else if( code == 0x38 )
    *rd = -1;

  return code;
}

// ----------------------------------------------------------------------------

uint32_t Encoder8b10b::encode3b4b( uint32_t b3, uint32_t b5, int *rd )
{
  uint32_t code;
  if( b3 == 0x7 &&
      (((b5 == 17 || b5 == 18 || b5 == 20) && *rd == -1) ||
       ((b5 == 11 || b5 == 13 || b5 == 14) && *rd == 1)) )
    {
      // D.x.A7, alternate encoding of 0x7
      if( *rd == -1 )
        code = DLUT_3b4b[8][0];
      else
        code = DLUT_3b4b[8][1];
    }
  else
    {
      if( *rd == -1 )
        code = DLUT_3b4b[b3][0];
      else
        code = DLUT_3b4b[b3][1];
    }

  if( bitsInWord(code) != 2 ) // Not equal number of 1s and 0s
    *rd *= (-1); // Update running disparity (toggles between +1 and -1)

  // Exceptions
  if( code == 0x3 )
    *rd = 1;
  else if( code == 0xC )
    *rd = -1;

  return code;
}

// ----------------------------------------------------------------------------

uint32_t Encoder8b10b::bitsInWord( uint32_t word )
{
  /* Calculate the number of bits set in a byte
     (method taken from www.devx.com, 'C++ tips'):
     Add up 4 pairs of 1-bit numbers (resulting in four 2-bit numbers):
     val = ((val >> 1) & 0x55) + (val & 0x55);
     Optimisation: save an AND (&) instruction by exploiting this clever
     relationship for each pair of bits: the two bits 'ab' represent the
     number 2a+b; to count the bits we subtract 'a' (i.e. 2a+b - a = a+b)
  byte = byte - ((byte >> 1) & 0x55);
  // Add up four 2-bit numbers (resulting in two 4-bit numbers
  byte = ((byte >> 2) & 0x33) + (byte & 0x33);
  // Add up two 4-bit numbers resulting in one 8-bit number
  byte = ((byte >> 4) & 0x0F) + (byte & 0x0F);
  return byte;
  */
  // Count how many bits are set in 'word' and return it
  uint32_t cnt;
  word = word - ((word >> 1) & 0x55555555);
  word = (word & 0x33333333) + ((word >> 2) & 0x33333333);
  cnt  = (((word + (word >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
  return cnt;
}

// ----------------------------------------------------------------------------

bool Encoder8b10b::decode( uint32_t *data, uint32_t size )
{
  if( data == 0 ) return false;
  _errIndex = -1;

  uint32_t i, code, b3, b5, b4, b6, index;
  bool     found;
  for( i=0; i<size; ++i )
    {
      code = data[i];
      // First look for currently used control symbols
      // (added K28.1, 3 June 2016)
      if( code == KLUT[7][0] || code == KLUT[7][1] ||
          code == KLUT[1][0] || code == KLUT[1][1] )
        data[i] = SOC_CODE;
      else if( code == KLUT[6][0] || code == KLUT[6][1] )
        data[i] = EOC_CODE;
      else if( code == KLUT[5][0] || code == KLUT[5][1] )
        data[i] = COMMA_CODE;
      else if( code == KLUT[2][0] || code == KLUT[2][1] )
        data[i] = SOB_CODE;
      else if( code == KLUT[3][0] || code == KLUT[3][1] )
        data[i] = EOB_CODE;
      else
        {
          // It's a data word...
          b6 = (code & 0x3F0) >> 4;
          b4 = (code & 0x00F);

          // Simply search for the code by looping through the tables
          // 6b -> 5b
          found = false;
          for( index=0; index<32; ++index )
            if( b6 == DLUT_5b6b[index][0] || b6 == DLUT_5b6b[index][1] )
              {
                found = true;
                break;
              }
          if( !found )
            {
              _errIndex = i;
              return false;
            }
          b5 = index;

          // 4b -> 3b
          found = false;
          for( index=0; index<9; ++index )
            if( b4 == DLUT_3b4b[index][0] || b4 == DLUT_3b4b[index][1] )
              {
                found = true;
                break;
              }
          if( !found )
            {
              _errIndex = i;
              return false;
            }
          // D.x.A7, alternate encoding of 0x7, stored at index 8
          if( index == 8 ) index = 7;
          b3 = index;

          // The decoded byte
          data[i] = (b3 << 5) | b5;
        }
    }

  return true;
}

// ----------------------------------------------------------------------------

void Encoder8b10b::displayTables()
{
  int i;
  cout << "DLUT_5b6b:" << endl;
  for( i=0; i<32; ++i )
    cout << dec << setw(2) << i << " " << hex << setw(2) << DLUT_5b6b[i][0]
         << ", " << setw(2) << DLUT_5b6b[i][1] << endl;
  cout << "DLUT_3b4b:" << endl;
  for( i=0; i<8+1; ++i )
    cout << dec << setw(2) << i << " " << hex << setw(2) << DLUT_3b4b[i][0]
         << ", " << setw(2) << DLUT_3b4b[i][1] << endl;
}

// ----------------------------------------------------------------------------
