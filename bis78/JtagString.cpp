#include <iostream>
#include <iomanip>
using namespace std;

#include "JtagString.h"

//-----------------------------------------------------------------------------

JtagString::JtagString( int nbits, const unsigned char *str )
  : _nbits(nbits)
{
  int nchars = (nbits + 7) / 8;

  // Allocate memory as multiple of 8 bytes
  int alloc_sz = nchars + (8 - (nchars%8));
  _bitString = new unsigned char[alloc_sz];

  //_bitString = new unsigned char[nchars];

  for( int i=0; i<nchars; ++i )
    _bitString[i] = str[i];
}

//-----------------------------------------------------------------------------

JtagString::JtagString( int nbits )
  : _nbits(nbits)
{
  int nchars = (nbits + 7) / 8;

  // Allocate memory as multiple of 8 bytes
  int alloc_sz = nchars + (8 - (nchars%8));
  _bitString = new unsigned char[alloc_sz];

  //_bitString = new unsigned char[nchars];

  for( int i=0; i<nchars; ++i )
    _bitString[i] = static_cast<unsigned char> (0x00);
}

//-----------------------------------------------------------------------------

JtagString::~JtagString()
{
  delete [] _bitString;
}

//-----------------------------------------------------------------------------

void JtagString::setBits( int i_dst, int nbits, int value )
{
  // 'nbits' limited to 32 !
  if( nbits > 32 ) nbits = 32;

  int bitmask = 0x00000001;
  int i_char, i_bit;
  for( int i=0; i<nbits; ++i )
    {
      i_char = (i_dst / 8);
      i_bit  = (i_dst & 0x7);

      if( (value & bitmask) != 0 )
        // Set bit
        _bitString[i_char] |= (1 << i_bit);
      else
        // Clear bit
        _bitString[i_char] &= ~(1 << i_bit);

      bitmask <<= 1;
      ++i_dst;
    }
}

//-----------------------------------------------------------------------------

void JtagString::setBits( int i_dst, int nbits,
                          int i_src, unsigned char *bytes )
{
  int i_char_dst, i_bit_dst;
  int i_char_src, i_bit_src;

  for( int i=0; i<nbits; ++i )
    {
      i_char_dst = (i_dst / 8);
      i_bit_dst  = (i_dst & 0x7);

      i_char_src = (i_src / 8);
      i_bit_src  = (i_src & 0x7);

      if( (bytes[i_char_src] & (1 << i_bit_src)) != 0 )
        // Set bit
        _bitString[i_char_dst] |= (1 << i_bit_dst);
      else
        // Clear bit
        _bitString[i_char_dst] &= ~(1 << i_bit_dst);

      ++i_dst;
      ++i_src;
    }
}

//-----------------------------------------------------------------------------

void JtagString::clearBits( unsigned int i_dst, unsigned int nbits )
{
  unsigned int i_char_dst, i_bit_dst;

  for( unsigned int i=0; i<nbits; ++i )
    {
      i_char_dst = (i_dst / 8);
      i_bit_dst  = (i_dst & 0x7);

      // Clear bit
      _bitString[i_char_dst] &= ~(1 << i_bit_dst);

      ++i_dst;
    }
}

//-----------------------------------------------------------------------------

int JtagString::bits( int index, int nbits ) const
{
  int value   = 0x00000000;
  int bitmask = 0x00000001;
  int i_char;
  int i_bit;
  for( int i=0; i<nbits; ++i )
    {
      i_char = (index / 8);
      i_bit  = (index & 0x7);

      if( (_bitString[i_char] & (1 << i_bit)) != 0 )
        value |= bitmask;

      bitmask <<= 1;
      ++index;
    }
  return value;
}

//-----------------------------------------------------------------------------

bool JtagString::operator!=( const JtagString &s ) const
{
  // Compare string length
  if( s.length() != _nbits )
    return true;

  // Compare string content
  int nchars = (_nbits + 7) / 8;
  unsigned char *str = s.string();
  for( int i=0; i<nchars; ++i )
    if( str[i] != _bitString[i] )
      return true;
  return false;
}

//-----------------------------------------------------------------------------

void JtagString::show( int len )
{
  if( len == 0 ) len = this->length();
  cout << len << " bits:" << endl;
  cout << setfill( '0' );// << setiosflags( ios::hex|ios::uppercase );
  cout << hex << uppercase;
  unsigned char *str = _bitString;
  int i;
  for( i=0; i<(len+7)/8; ++i )
    {
      cout << setw( 2 );
      cout << static_cast<unsigned int>(str[i]) << " ";
      if( ((i+1) & 0xF) == 0 )
        cout << endl;
    }
  if( (i & 0xF) != 0 )
    cout << endl;
  cout << setfill( ' ' ) << dec;
}

//-----------------------------------------------------------------------------
