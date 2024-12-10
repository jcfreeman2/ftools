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
const   int VERSION_ID = 0x21091300; // Check firmware type; display idle count
//const int VERSION_ID = 0x21080900;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr   = 0;
  bool config   = false;
  bool ttc_mode = false;
  int  triggers = -1;
  int  idles    = -1;
  int  ranseed  = -1;
  bool ecr      = false;
  bool xonoff   = false;
  bool start    = false;
  int  wordsize = -1;

  // Parse the options
  while( (opt = getopt(argc, argv, "cd:Ehi:R:st:TVw:X")) != -1 )
    {
      switch( opt )
        {
        case 'c':
          config = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'E':
          ecr = true;
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          if( sscanf( optarg, "%d", &idles ) != 1 )
            arg_error( 'i' );
          if( idles < 0 || idles > 0xFFFF )
            arg_range( 'i', 0, 0xFFFF );
          break;
        case 'R':
          if( sscanf( optarg, "%d", &ranseed ) != 1 )
            arg_error( 'R' );
          if( ranseed < 1 || triggers > 0x3FF )
            arg_range( 'R', 1, 0x3FF );
          break;
        case 's':
          start = true;
          break;
        case 't':
          if( sscanf( optarg, "%d", &triggers ) != 1 )
            arg_error( 't' );
          if( triggers < 0 || triggers > 0xFFFE )
            arg_range( 't', 0, 0xFFFE );
          break;
        case 'T':
          ttc_mode = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'w':
          if( sscanf( optarg, "%d", &wordsize ) != 1 )
            arg_error( 'w' );
          if( !(wordsize == 0 || (wordsize > 2 && wordsize < 0x10000)) )
            arg_range( 'w', 3, 0xFFFF );
          break;
        case 'X':
          xonoff = true;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Open FELIX FLX-card
  FlxCard *flx = new FlxCard;
  try {
    flx->card_open( cardnr, LOCK_NONE );
  }
  catch( FlxException &ex ) {
    cout << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }

  uint64_t bar2_addr = flx->bar2Address();
  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

  uint64_t fw_mode = bar2->FIRMWARE_MODE;
  if( fw_mode != FIRMW_FMEMU )
    cout << "### WARNING: not FULLMODE-emulator firmware" << endl;

  if( config || start )
    {
      if( ttc_mode )
        bar2->FMEMU_CONTROL.TTC_MODE = 1;
      else
        bar2->FMEMU_CONTROL.TTC_MODE = 0;

      if( ranseed != -1 )
        bar2->FMEMU_RANDOM_CONTROL.SEED = ranseed;

      if( xonoff )
        bar2->FMEMU_CONTROL.XONXOFF = 1;
      else
        bar2->FMEMU_CONTROL.XONXOFF = 0;

      if( triggers == 0 )
        triggers = 0xFFFF; // Unlimited
      if( triggers != -1 )
        bar2->FMEMU_COUNTERS.L1A_CNT = triggers;

      if( idles != -1 )
        bar2->FMEMU_COUNTERS.IDLE_CNT = idles;

      if( wordsize == 0 )
        {
          bar2->FMEMU_CONTROL.CONSTANT_CHUNK_LENGTH = 0; // Random length
        }
      else if( wordsize > 2 )
        {
          bar2->FMEMU_CONTROL.CONSTANT_CHUNK_LENGTH = 1;
          bar2->FMEMU_COUNTERS.WORD_CNT = wordsize - 3;
        }

      if( ecr )
        {
          bar2->FMEMU_CONTROL.ECR = 1;
          bar2->FMEMU_CONTROL.ECR = 0;
        }

      if( start )
        {
          bar2->FMEMU_CONTROL.EMU_START = 0;
          bar2->FMEMU_CONTROL.EMU_START = 1;
          //bar2->FMEMU_CONTROL.EMU_START = 0;
          cout << "Started FMEMU" << endl;
        }
      else
        {
          // Just in case
          bar2->FMEMU_CONTROL.EMU_START = 0;
        }
    }
  else
    {
      if( triggers != -1 || idles    != -1 ||
          ranseed  != -1 || wordsize != -1 )
        cout << "### Use option -c (or -s) to configure" << endl;
    }

  // Display the current FMEMU settings
  cout << "TTC mode  : ";
  if( bar2->FMEMU_CONTROL.TTC_MODE == 1 )
    cout << "ON" << endl;
  else
    cout << "OFF" << endl;

  cout << "XONOFF    : ";
  if( bar2->FMEMU_CONTROL.XONXOFF == 1 )
    cout << "YES" << endl;
  else
    cout << "NO" << endl;

  cout << "L1A count : ";
   if( bar2->FMEMU_COUNTERS.L1A_CNT == 0xFFFF )
     cout << "UNLIMITED" << endl;
   else
     cout << bar2->FMEMU_COUNTERS.L1A_CNT << endl;

  cout << "Chunk size: ";
  if( bar2->FMEMU_CONTROL.CONSTANT_CHUNK_LENGTH == 0 )
    cout << "RANDOM (seed="
         << bar2->FMEMU_RANDOM_CONTROL.SEED << ")" << endl;
  else
    cout << bar2->FMEMU_COUNTERS.WORD_CNT+3 << " words (bytes: "
         << (bar2->FMEMU_COUNTERS.WORD_CNT+3)*4 << ")" << endl;

  cout << "Idle count: " << bar2->FMEMU_COUNTERS.IDLE_CNT << endl;

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
  cout << "ffmemu version " << hex << VERSION_ID << dec << endl <<
    "Configure or show configuration of the FULLMODE emulator firmware.\n"
    "Starting the emulator is optional.\n"
    "Enable 2-bit E-link 0 (egroup 0, epath 0, 8b10b) for XOFF.\n"
    "Enable 8-bit E-link 9 (egroup 1, epath 1, TTC-3) for TTC.\n"
    "Usage: ffmemu [-h|V] [-d <devnr>] [-c] [-s] [-i <idles> [-t <cnt>] [-T] "
    "[-w <words>] [-X] [-R <seed>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -c         : Only configure the FMEMU; required for options -i,-R,-t,-T,-w,-X,\n"
    "               unless in combination with option -s.\n"
    "  -E         : Issue an ECR.\n"
    "  -i <idles> : Set idles-between-chunks count, range [0..0xFFFF].\n"
    "  -R <seed>  : Set random seed, in combination with random chunk sizes "
    "(-w 0), range [1..0x3FF].\n"
    "  -s         : Start the emulator (after any configuration to be done).\n"
    "  -t <cnt>   : Number of triggers (chunks) to generate "
    "(0=unlimited, [1..65534]).\n"
    "  -T         : Enable TTC-triggered mode (default: free running).\n"
    "  -w <words> : Set chunk 4-byte word size (0=random, [3..65535]).\n"
    "  -X         : Enable XON/XOFF.\n";
}

// ----------------------------------------------------------------------------
