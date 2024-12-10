// FPGA bit-file (.bit) reader class.

/*
The Xilinx .bit format is pretty simple.  It uses keys and lengths to 
divide the file.
Here is an example.  Below is a hex dump from the beginning of a .bit file:

00000000:  00 09 0f f0 0f f0 0f f0 0f f0 00 00 01 61 00 0a  .............a..
00000010:  78 66 6f 72 6d 2e 6e 63 64 00 62 00 0c 76 31 30  xform.ncd.b..v10
00000020:  30 30 65 66 67 38 36 30 00 63 00 0b 32 30 30 31  00efg860.c..2001
00000030:  2f 30 38 2f 31 30 00 64 00 09 30 36 3a 35 35 3a  /08/10.d..06:55:
00000040:  30 34 00 65 00 0c 28 18 ff ff ff ff aa 99 55 66  04.e..(.......Uf

Field 1
2 bytes       length 0x0009           (big endian)
9 bytes       some sort of header

Field 2
2 bytes       length 0x0001 
1 byte        key 0x61                (The letter "a")

Field 3
2 bytes       length 0x000a           (value depends on file name length)
10 bytes      string design name "xform.ncd" (including a trailing 0x00)

Field 4
1 byte        key 0x62                (The letter "b")
2 bytes       length 0x000c           (value depends on part name length)
12 bytes      string part name "v1000efg860" (including a trailing 0x00)

Field 4
1 byte        key 0x63                (The letter "c")
2 bytes       length 0x000b
11 bytes      string date "2001/08/10" (including a trailing 0x00)

Field 5
1 byte        key 0x64                (The letter "d")
2 bytes       length 0x0009
9 bytes       string time "06:55:04"  (including a trailing 0x00)

Field 6
1 byte        key 0x65                (The letter "e")
4 bytes       length 0x000c9090       (value depends on device type,
                                       and maybe design details)
8233440 bytes  raw bit stream starting with 0xffffffff aa995566 sync
               word documented below.
*/
//#include <stdio.h>
//#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <exception>
using namespace std;

#include "BitFile.h"

// ----------------------------------------------------------------------------

BitFile::BitFile()
  : _valid( false ),
    _data( 0 ),
    _nbytes ( 0 )
{
}

// ----------------------------------------------------------------------------

BitFile::~BitFile()
{
  for( field_t& f : _field )
    {
      if( f.data )
        {
          delete [] f.data;
          f.data = nullptr;
        }
    }
}

// ----------------------------------------------------------------------------

int BitFile::fieldId( unsigned int index )
{
  if( index < _field.size() )
    return _field[index].id;
  return -1;
}

// ----------------------------------------------------------------------------

int BitFile::fieldLength( unsigned int index )
{
  if( index < _field.size() )
    return _field[index].length;
  return -1;
}

// ----------------------------------------------------------------------------

unsigned char* BitFile::fieldData( unsigned int index )
{
  if( index < _field.size() )
    return (unsigned char *) _field[index].data;
  return 0;
}

// ----------------------------------------------------------------------------

void BitFile::setFile( const string &filename, bool skip_data )
{
  _valid    = false;
  _fileName = "";

  ifstream fin( filename );
  if( !fin.is_open() )
    throw std::runtime_error( "failed to open file" );

  char bytes[4] = { 0, 0, 0, 0 };
  unsigned char *p = (unsigned char *) bytes;
  unsigned int   len;
  ostringstream  oss;
  field_t        f;

  // Field 1
  fin.read( bytes, 2 );
  len = (((unsigned int) *p) << 8) | ((unsigned int) *(p+1));
  if( len != 9 )
    {
      oss << "Field-" << 1 << " length=" << len
          << ", it doesn't look like a bit-file";
      throw runtime_error( oss.str() );
    }
  f.id     = 1;
  f.length = len;
  f.data   = new char[len];
  fin.read( f.data, len );
  _field.push_back( f );

  // 'Field 2' (just the "00 01" bytes?)
  fin.read( bytes, 2 );
  len = (((unsigned int) *p) << 8) | ((unsigned int) *(p+1));
  if( len != 1 )
    {
      oss << "Field-" << 2 << " length=" << len
          << ", it doesn't look like a bit-file";
      throw runtime_error( oss.str() );
    }
  f.id     = 2;
  f.length = 0;
  f.data   = 0;
  _field.push_back( f );

  // Fields '0x61' and up
  while( !fin.eof() )
    {
      // ID
      fin.read( bytes, 1 );
      if( fin.eof() ) break;
      f.id = (unsigned int) bytes[0];

      // Length
      if( f.id == 0x65 )
        {
          fin.read( bytes, 4 );
          len = ((((unsigned int) *p) << 24) |
                 (((unsigned int) *(p+1)) << 16) |
                 (((unsigned int) *(p+2)) << 8) |
                 (((unsigned int) *(p+3)) << 0));
        }
      else
        {
          fin.read( bytes, 2 );
          len = (((unsigned int) *p) << 8) | ((unsigned int) *(p+1));
        }
      f.length = len;

      // Data
      if( f.id == 0x65 && skip_data )
        {
          f.data = 0;
          fin.seekg( len, ios_base::cur );
        }
      else
        {
          f.data = new char[len];
          fin.read( f.data, len );
          if( f.id == 0x65 )
            {
              _data   = (unsigned char *) f.data;
              _nbytes = len;
            }
        }

      _field.push_back( f );
    }

  _valid    = true;
  _fileName = filename;
}

// ----------------------------------------------------------------------------

void BitFile::reverseBytes()
{
  if( !(_data && _nbytes > 0) ) return;

  // Create a bit-reversed array of bytes (all 256 possible values)
  unsigned char bitreversed[256];
  unsigned int  i, j, bitmask;
  for( i=0; i<256; ++i )
    {
      bitreversed[i] = 0;
      bitmask = 1;
      for( j=0; j<8; ++j, bitmask<<=1 )
        if( i & bitmask )
          bitreversed[i] |= (1 << (7-j));
    }

  // Apply the conversion table to the data
  unsigned char *b = _data;
  for( i=0; i<_nbytes; ++i, ++b )
    {
      j = (unsigned int) (*b);
      *b = bitreversed[j];
    }

  /*
  // Bit-reverse the data array byt-by-byte
  unsigned int  i, j;
  unsigned char tmp, bitmask;
  unsigned char *b = _data;
  for( i=0; i<_nbytes; ++i, ++b )
    {
      tmp = 0;
      bitmask = 1;
      for( j=0; j<8; ++j, bitmask<<=1 )
        if( (*b) & bitmask )
          tmp |= (1 << (7-j));
      *b = tmp;
    }
  */
}

// ----------------------------------------------------------------------------

void BitFile::displayInfo( bool debug )
{
  // Display more with debug==true
  cout << "==> BIT-file " << fileName();
  if( debug )
    cout << ", fields = " << fieldCount();
  cout << ":" << endl;

  for( unsigned int i=0; i<fieldCount(); ++i )
    {
      unsigned int   id  = fieldId( i );
      unsigned int   len = fieldLength( i );
      unsigned char *b   = fieldData( i );
      if( debug )
        {
          cout << dec << setfill( ' ' );
          cout << id << ", len=" << setw(2) << len << ": ";
          cout << hex << setfill( '0' );
          if( b != 0 )
            {
              unsigned int j;
              for( j=0; j<len; ++j )
                {
                  if( (j & 0x1F) == 0x00 )
                    cout << "\n   ";
                  cout << setw(2) << (unsigned int) b[j] << " ";
                  // Display only up to x bytes...
                  if( j >= 127 )
                    {
                      if( len > 127+1 ) cout << "...";
                      break;
                    }
                }
              cout << endl;
            }
        }
      // Display some fields as a string
      if( id > 2 && id != 0x65 && b != 0 )
        {
          string str( (char *) b, len );
          if( debug )
            cout << "   ";
          else
            cout << "- ";
          cout << "\"" << str << "\"" << endl;
        }
    }
  cout << dec << setfill( ' ' ) << endl;
}

// ----------------------------------------------------------------------------
