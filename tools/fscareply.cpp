#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxParser.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x21050500; // Added options -c and -r.
//const int VERSION_ID = 0x21041500;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  bool is_cmd   = false;
  bool reversed = false;

  // Parse the options
  while( (opt = getopt(argc, argv, "chVr")) != -1 )
    {
      switch( opt )
        {
        case 'c':
          is_cmd = true;
          break;
        case 'h':
          usage();
          return 0;
        case 'r':
          reversed = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        default: // '?'
          usage();
          return 1;
        }
    }

  FlxParser fparser;

  std::vector<uint8_t> f;
  unsigned int x;
  while( optind < argc )
    {
      if( sscanf( argv[optind], "%x", &x ) != 1 )
        {
          cout << "### Invalid parameter (" << optind << "): "
               << argv[optind] << endl;
          return 1;
        }
      f.push_back( (uint8_t)(x & 0xFF) );
      ++optind;
    }
  if( reversed )
    {
      uint8_t tmp;
      for( size_t i=0; i<f.size()/2; ++i )
        {
          tmp = f[i];
          f[i] = f[f.size()-1-i];
          f[f.size()-1-i] = tmp;
        }
      // Display the reversed byte sequence
      cout << hex;
      for( unsigned int x : f )
        cout << x << " ";
      cout << ": ";
    }
  if( f.size() > 0 )
    {
      fparser.displayScaFrame( f.data(), f.size(), is_cmd, true );
      cout << endl;
      return 0;
    }

  return 0;
  std::vector<uint8_t> frame;

  //frame = { 0x00, 0xa8, 0xd6, 0x00, 0x00, 0x03, 0x00, 0xfe, 0x52, 0xeb };
  //fparser.displayScaFrame( frame.data(), frame.size() );

  // Check CRC error reporting:
  //frame = { 0x00, 0xa8, 0xd6, 0x00, 0x00, 0x03, 0x00, 0xfe, 0x51, 0xec };
  //fparser.displayScaFrame( frame.data(), frame.size() );

  //frame = { 0x00, 0x42, 0x88, 0x14, 0x00, 0x06, 0x00, 0x00, 0x28, 0x07, 0x2b, 0x7f };
  //fparser.displayScaFrame( frame.data(), frame.size() );

  // FLXUSERS-380
  // "0-byte messages" entry
  frame = { 0x00, 0x0e, 0x7d, 0x00, 0x00, 0x03, 0x00, 0xfe, 0xe6, 0xfb }; 
  fparser.displayScaFrame( frame.data(), frame.size() );

  frame = { 0x00, 0x20, 0xc9, 0x14, 0x00, 0x06, 0x99, 0x29 };
  fparser.displayScaFrame( frame.data(), frame.size() );

  frame = { 0x00, 0x42, 0xca, 0x14, 0x00, 0x06, 0x00, 0x00, 0xf1, 0x0b, 0xfa, 0x53 };
  fparser.displayScaFrame( frame.data(), frame.size() );

  frame = { 0x00, 0x64, 0xcb, 0x14, 0x00, 0x06, 0xdd, 0xfc };
  fparser.displayScaFrame( frame.data(), frame.size() );

  std::cout << endl;

  // FLXUSERS-380
  // "big messages" entry
  frame = { 0x00, 0xa8, 0xd6, 0x00, 0x00, 0x03, 0x00, 0xfe, 0x52, 0xeb };
  fparser.displayScaFrame( frame.data(), frame.size() );

  frame = { 0x00, 0xca, 0x27, 0x14, 0x00, 0x06, 0x1f, 0x54 };
  fparser.displayScaFrame( frame.data(), frame.size() );

  frame = { 0x00, 0xec, 0x28, 0x14, 0x00, 0x06, 0x00, 0x00, 0xf3, 0x0b, 0xfd, 0x9b };
  fparser.displayScaFrame( frame.data(), frame.size() );

  std::cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscareply version " << hex << VERSION_ID << dec << endl <<
    "Tool to parse and display a sequence of bytes as a GBT-SCA reply frame\n"
    "or optionally as a request (command) frame (option -c).\n"
    "It will indicate the CTRL byte, transaction ID, channel, error byte,\n"
    "data size and data word, if any.\n"
    "In addition it will indicate the type of error, if any,\n"
    "and whether the frame has a CRC error.\n"
    "Usage:\n"
    " fscareply [-h|V] [-c] [-r] [<byte0>] [<byte1>] ...\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -c         : Interpret the sequence as a GBT-SCA *request* frame.\n"
    "  -r         : Reverse the byte order before interpretation.\n"
    " <byteX>     : Byte X of the reply frame to be parsed "
    "(provide it as hex number)\n\n"
    "Examples:\n"
    "> fscareply 00 0e 7d 00 00 03 00 fe e6 fb\n"
    "CTRL=0E(r=0,s=7) TRID=125 Chan=CONF ERR=00 LEN=3 data=0xFE00\n"
    "\n"
    "> fscareply 00 ec 28 14 00 06 00 00 f3 0b fd 9b\n"
    "CTRL=EC(r=7,s=6) TRID=40 Chan=ADC ERR=00 LEN=6 data=0x00000BF3\n"
    "\n"
    "Bytes provided in reversed order:\n"
    "> fscareply -r 5c 09 06 00 13 36 ec 00\n"
    "0 ec 36 13 0 6 9 5c : CTRL=EC(r=7,s=6) TRID=54 Chan=JTAG ERR=00 LEN=6 \n"
    "\n"
    "Indication of a CRC error (could be in CRC itself or in the data bytes),\n"
    "(here CRC has been changed from 095C to 195C):\n"
    "> fscareply 00 ec 36 13 00 06 19 5c\n"
    "CTRL=EC(r=7,s=6) TRID=54 Chan=JTAG ERR=00 LEN=6  ###CRC=095C\n";
}

// ----------------------------------------------------------------------------
