#include <iostream>
#include <iomanip>
#include <cstdint>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x20011400; // Added option -r
//const int VERSION_ID = 0x17041800;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int      opt;
  int      cardnr = 0;
  int      barnr  = 2;
  int      nbits  = 64;
  int      range  = 1;
  uint32_t addr   = -1;
  uint64_t data   = -1;
  bool     write  = false;

  // Parse the options
  while( (opt = getopt(argc, argv, "b:d:hin:r:V")) != -1 )
    {
      switch( opt )
        {
        case 'b':
          // BAR selection: 0, 1 or 2
          if( sscanf( optarg, "%d", &barnr ) != 1 )
            arg_error( 'b' );
          if( barnr < 0 || barnr > 2 )
            arg_range( 'b', 0, 2 );
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          nbits = 32;
          break;
        case 'n':
          // Number of bits: 64 or 32
          if( sscanf( optarg, "%d", &nbits ) != 1 )
            arg_error( 'n' );
          if( !(nbits == 32 || nbits == 64) )
            {
              cout << "### -n: only 64 or 32 allowed" << endl;
              return 0;
            }
          break;
        case 'r':
          // Range of addresses to read
          if( sscanf( optarg, "%d", &range ) != 1 )
            arg_error( 'r' );
          if( range < 1 || range > 1024 )
            arg_range( 'r', 1, 1024 );
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

  // Expect an address and optionally a dataword to write (both hexadecimal)
  if( optind < argc )
    {
      // Tolerate values like "x12" and "0x12", not just "12"
      int i = 0;
      if( argv[optind][0] == 'x' || argv[optind][0] == 'X' )
        i = 1;
      else if( argv[optind][0] == '0' &&
               (argv[optind][1] == 'x' || argv[optind][1] == 'X') )
        i = 2;
      if( sscanf( &argv[optind][i], "%x", &addr ) == 1 )
        {
          // Address
          if( (addr & 0x3) != 0 ||
              (nbits == 64 && (addr & 0x7) != 0) )
            {
              cout << "### Address not aligned to ";
              if( nbits == 64 )
                cout << "8 bytes";
              else
                cout << "4 bytes";
              cout << endl;
              return 1;
            }

          // Data
          ++optind;
          if( optind < argc )
            {
              // Tolerate values like "x12" and "0x12", not just "12"
              i = 0;
              if( argv[optind][0] == 'x' || argv[optind][0] == 'X' )
                i = 1;
              else if( argv[optind][0] == '0' &&
                       (argv[optind][1] == 'x' || argv[optind][1] == 'X') )
                i = 2;
              if( sscanf( &argv[optind][i], "%lx", &data ) == 1 )
                {
                  // Dataword to write
                  write = true;
                }
              else
                {
                  cout << "### Invalid data" << endl;
                  return 1;
                }
            }
        }
      else
        {
          cout << "### Invalid address" << endl;
          return 1;
        }
    }
  else
    {
      cout << "### Provide an address" << endl;
      return 1;
    }

  // Open FELIX FLX-card
  FlxCard *flx = new FlxCard;
  try {
    flx->card_open( cardnr, LOCK_NONE, true );
  }
  catch( FlxException &ex ) {
    cout << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }

  uint64_t  bar_addr = flx->openBackDoor( barnr );
  uint64_t *bar64    = (uint64_t *) bar_addr;
  uint32_t *bar32    = (uint32_t *) bar_addr;
  int       sz       = nbits/8;
  int       index    = addr / sz;
  int       min      = index;
  int       max      = index + range;

  if( write && range > 1 )
    {
      cout << "NB: a range (-r) is not taken into account for writing!" << endl;
      range = 1;
      max = index + range;
    }

  // Read/write as 8-byte or 4-byte unit
  if( nbits == 64 )
    {
      for( index=min; index<max; ++index )
        {
          if( write ) bar64[index] = data;

          cout << hex << setfill('0')
               << setw(4) << index*8 << ": "
               << setw(sz*2) << bar64[index]
               << endl;
        }
    }
  else
    {
      for( index=min; index<max; ++index )
        {
          if( write ) bar32[index] = data;

          cout << hex << setfill('0')
               << setw(4) << index*4 << ": "
               << setw(sz*2) << bar32[index]
               << endl;
        }
    }

  // Close the FLX-card
  try {
    flx->card_close();
  }
  catch( FlxException &ex ) {
    cout << "FlxCard close: " << ex.what() << endl;
  }

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fpepo version " << hex << VERSION_ID << dec << endl <<
    "Peek or poke FLX-device BAR0, BAR1 or BAR2 registers using "
    "hexadecimal register addresses.\n"
    "Optionally reads multiple registers.\n"
    "Examples:\n"
    "  Read 8-byte BAR2 register address 0x1100:  fpepo 1100\n"
    "  Read 8-byte BAR0 register address 0x10:    fpepo -b 0 10\n"
    "  Write 8-byte BAR2 register address 0x1100: fpepo 1100 000000002aaaff88\n"
    "  (zeroes in dataword not required; just emphasizing that "
    "it is a 64-bit write)\n"
    "  Write 4-byte BAR2 register address 0x1104: fpepo -n 32 1104 12345678\n\n"
    "Usage: fpepo [-h|V] [-d <devnr>] [-b <bar>] [-i] [-n <bits>] [-r <cnt>] "
    "<addr> [<data>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device number to use (default: 0).\n"
    "  -b <bar>   : Selected BAR segment 0, 1 or 2 (default: 2).\n"
    "  -i         : Select 4-byte register size (default: 8 bytes).\n"
    "               (Alternative for (legacy) '-n' option).\n"
    "  -n <bits>  : Select register size of 4 or 8 bytes, "
    "i.e. 32 or 64 bits (default:64)\n"
    "               (also see option -i).\n"
    "  -r <cnt>   : Read 'cnt' addresses starting from the given address;\n"
    "               does not apply when writing (range [1..1024]; default: 1).\n"
    "   <addr>    : Address (hex) of register address (within the selected "
    "BAR segment)\n"
    "               to be read or written to.\n"
    "   <data>    : Dataword (hex) to write to the given register address;\n"
    "               the resulting register content is displayed.\n";
}

// ----------------------------------------------------------------------------
