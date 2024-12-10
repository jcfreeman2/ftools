#include <fstream>
#include <iostream>
#include <iomanip>
#include <exception>
using namespace std;

#include "JtagSequence.h"

// ----------------------------------------------------------------------------

JtagSequence::JtagSequence()
  : _valid( false )
{
  // Create a bit-reversed array of bytes (all 256 possible values)
  unsigned int i, j, bitmask;
  for( i=0; i<256; ++i )
    {
      _bitReversedMap[i] = 0;
      bitmask = 1;
      for( j=0; j<8; ++j, bitmask<<=1 )
        if( i & bitmask )
          _bitReversedMap[i] |= (1 << (7-j));
    }
}

// ----------------------------------------------------------------------------

JtagSequence::~JtagSequence()
{
}

// ----------------------------------------------------------------------------

bool JtagSequence::isInstruction( int i )
{
  if( i >= (int) _jtagStrings.size() )
    return false;
  return _jtagStrings[i].instr;
}

// ----------------------------------------------------------------------------

bool JtagSequence::readTdi( int i )
{
  if( i >= (int) _jtagStrings.size() )
    return false;
  return _jtagStrings[i].read_tdi;
}

// ----------------------------------------------------------------------------

int JtagSequence::bitCount( int i )
{
  if( i >= (int) _jtagStrings.size() )
    return 0;
  return _jtagStrings[i].nbits;
}

// ----------------------------------------------------------------------------

unsigned char *JtagSequence::bits( int i, bool reversed )
{
  if( i >= (int) _jtagStrings.size() )
    return 0;

  if( _jtagStrings[i].reversed != reversed )
    {
      int nbits = _jtagStrings[i].nbits;
      unsigned char *b = _jtagStrings[i].data.data();

      // Reverse bits within bytes
      int j;
      unsigned int index;
      for( j=0; j<nbits/8; ++j, ++b )
        {
          index = (unsigned int) *b;
          *b = _bitReversedMap[index];
        }

      // In the last byte reverse only the number of significant bits!
      if( (nbits & 7) != 0 )
        {
          nbits &= 7;
          unsigned char tmp = 0, bitmask = 0x01;
          for( j=0; j<nbits; ++j, bitmask<<=1 )
            {
              if( (*b) & bitmask )
                tmp |= (1 << ((nbits-1)-j));
            }
          *b = tmp;
        }

      // Update administration
      _jtagStrings[i].reversed = reversed;
    }

  return _jtagStrings[i].data.data();
}

// ----------------------------------------------------------------------------

void JtagSequence::setFile( string filename )
{
  /* Here is an example of valid file content:
     #
     # JTAG strings to read the HPTDC ID(s)
     #

     # For 1 TDC:
     #I  5 11
     #d 32 00 00 00 00

     # For 2 TDCs:
     #I 10 31 02
     #d 64 00 00 00 00 00 00 00 00

     # For 3 TDCs:
     I 15 31 46
     d 96 00 00 00 00 00 00 00 00 00 00 00 00
  */
  _valid    = false;
  _fileName = "";

  ifstream fin( filename );
  if( !fin.is_open() )
    throw std::runtime_error( "failed to open file" );;

  string        line, s;
  int           line_nr = 0;
  int           nbits;
  jtagstr_t     jtagstr;
  ostringstream oss;
  istringstream iss;
  jtagstr.reversed = false;
  iss >> skipws; // Skip whitespace
  while( !fin.eof() )
    {
      getline( fin, line );
      if( fin.eof() ) break;

      // Going to process this line
      ++line_nr;
      iss.clear();
      iss.str( line );

      // Get 'instruction' or 'data' character
      // (if the line is empty, iss.fail() will be true)
      iss >> s;

      if( iss.fail() || s[0] == '#' || s[0] == '\n' )
        continue; // Skip this line: comment line or empty line
      else if( s[0] == 'I' || s[0] == 'i' )
        jtagstr.instr = true;  // JTAG Instruction string
      else if( s[0] == 'D' || s[0] == 'd' )
        jtagstr.instr = false; // JTAG Data string
      else
        {
          oss << "Line " << line_nr << ", illegal first char=" << s[0];
          throw std::runtime_error( oss.str() );
        }

      // SPECIAL: *lower case* letter to indicate we want to read back
      //          the TDI bits while shifting this string
      if( s[0] == 'd' || s[0] == 'i' )
        jtagstr.read_tdi = true;
      else
        jtagstr.read_tdi = false;

      // Get the number of bits
      iss >> nbits;
      if( iss.fail() )
        {
          oss << "Line " << line_nr << ", error in number of bits";
          throw std::runtime_error( oss.str() );
        }
      jtagstr.nbits = nbits;

      // Get the JTAG instruction or data bits
      iss >> hex;
      unsigned int byt;
      int          nbytes = 0;
      while( !iss.fail() )
        {
          iss >> byt;
          if( iss.fail() ) break;
          ++nbytes;
          jtagstr.data.push_back( (unsigned char) (byt & 0xFF) );
        }
      iss >> dec;
      if( nbytes != (nbits+7)/8 )
        {
          oss << "Line " << line_nr << ", number of bits does not match data";
          throw std::runtime_error( oss.str() );
        }

      _jtagStrings.push_back( jtagstr );
      jtagstr.data.clear(); // Reset, ready for next string
    }

  _valid    = true;
  _fileName = filename;
}

// ----------------------------------------------------------------------------

void JtagSequence::displayInfo( bool debug )
{
  // Display more with debug==true
  
  cout << "==> JTAG sequence contains "
       << _jtagStrings.size() << " strings" << endl
       << "File: " << fileName() << endl;

  for( unsigned int i=0; i<_jtagStrings.size(); ++i )
    {
      cout << setw(2) << i+1 << ": ";
      if( _jtagStrings[i].instr )
        cout << "INSTR";
      else
        cout << "DATA ";

      cout << " bits=" << _jtagStrings[i].nbits;

      if( _jtagStrings[i].read_tdi )
        cout << " (Read)";

      if( debug )
        {
          cout << hex << setfill( '0' );
          //unsigned char *bytes = _jtagStrings[i].data.data();
          unsigned char *bytes = this->bits( i, false );
          int j;
          for( j=0; j<(_jtagStrings[i].nbits+7)/8; ++j )
            {
              if( (j & 0x1F) == 0 )
                cout << endl << "    ";
              cout << setw(2) << (unsigned int) bytes[j] << " ";
            }
          if( (j & 0x3F) != 0x21 )
            cout << endl;
        }
      else
        {
          cout << endl;
        }
      cout << dec << setfill( ' ' );
    }
  cout << dec << setfill( ' ' );
}

// ----------------------------------------------------------------------------
