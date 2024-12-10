#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x21111200; // Option -b: TTC emu busy-in enable
//const int VERSION_ID = 0x21091300; // Check TTC emulator included in firmw
//const int VERSION_ID = 0x20062200; // ECR in ms period instead of Hz rate
//const int VERSION_ID = 0x20061000; // Added TTC Decoder reset to emu reset
//const int VERSION_ID = 0x20032000;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr           = 0;
  int  devnr            = 0;
  bool reset_emulator   = false;
  bool enable_emulator  = false;
  bool disable_emulator = false;
  bool generate_bcr     = false;
  bool generate_ecr     = false;
  int  bcr_period       = -1;
  int  ecr_period       = -1;
  int  l1a_freq         = -1;
  int  l1a_cnt          = 0;
  int  l1a_delay        = 0; // Time between 'manual' L1A (in us)
  int  busy_in_enable   = -1;

  // Parse the options
  while( (opt = getopt(argc, argv, "b:B:c:eE:f:hL:nRt:V")) != -1 )
    {
      switch( opt )
        {
        case 'b':
          if( sscanf( optarg, "%d", &busy_in_enable ) != 1 )
            arg_error( 'b' );
          if( busy_in_enable != 0 && busy_in_enable != 1 )
            arg_range( 'b', 0, 1 );
          break;
        case 'B':
          if( sscanf( optarg, "%d", &bcr_period ) != 1 )
            arg_error( 'B' );
          if( bcr_period < 0 )
            arg_range( 'B', 0, 0x7FFFFFFF );
          if( bcr_period >= 0 )
            enable_emulator = true;
          if( bcr_period == 0 )
            generate_bcr = true;
          break;
        case 'c':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'c' );
          break;
        case 'e':
          enable_emulator = true;
          break;
        case 'E':
          if( sscanf( optarg, "%d", &ecr_period ) != 1 )
            arg_error( 'E' );
          if( ecr_period < 0 || ecr_period > 0xFFFFFF )
            arg_range( 'E', 0, 0xFFFFFF );
          if( ecr_period >= 0 )
            enable_emulator = true;
          if( ecr_period == 0 )
            generate_ecr = true;
          break;
        case 'f':
          if( sscanf( optarg, "%d", &l1a_freq ) != 1 )
            arg_error( 'f' );
          if( l1a_freq < 0 || l1a_freq > 0xFFFFFF )
            arg_range( 'f', 0, 0xFFFFFF );
          enable_emulator = true;
          break;
        case 'h':
          usage();
          return 0;
        case 'L':
          if( sscanf( optarg, "%d", &l1a_cnt ) != 1 )
            arg_error( 'L' );
          if( l1a_cnt < 0 || l1a_cnt > 0xFFFF )
            arg_range( 'L', 0, 0xFFFF );
          if( l1a_cnt > 0 )
            enable_emulator = true;
          break;
        case 'n':
          disable_emulator = true;
          break;
        case 'R':
          reset_emulator = true;
          break;
        case 't':
          if( sscanf( optarg, "%d", &l1a_delay ) != 1 )
            arg_error( 't' );
          if( l1a_delay < 0 || l1a_delay > 1000000 )
            arg_range( 't', 0, 1000000 );
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

  if( enable_emulator && disable_emulator )
    {
      cout << "### WARNING: you've selected '-n' (disable emulator): "
           << "other options are ignored" << endl;
      enable_emulator = false;
    }
  if( disable_emulator )
    {
      generate_bcr = false;
      generate_ecr = false;
      bcr_period   = -1;
      ecr_period   = -1;
      l1a_freq     = -1;
      l1a_cnt      = 0;
      l1a_delay    = 0;
    }

  // Open the FLX-device associated with the given card number
  FlxCard *flx = new FlxCard;
  try {
    // Map card number to a device number
    devnr = FlxCard::card_to_device_number( cardnr );
    if( devnr < 0 )
      {
        printf( "### -c: card number out-of-range\n" );
        return 1;
      }
    flx->card_open( devnr, LOCK_NONE );
  }
  catch( FlxException &ex ) {
    cout << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }

  uint64_t bar2_addr = flx->openBackDoor( 2 );
  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

#if REGMAP_VERSION < 0x500
  if( (bar2->TTC_EMU_CONST_GENERATE_TTC_EMU & 2) == 0 )
#else
  if( (bar2->GENERATE_TTC_EMU & 2) == 0 )
#endif // REGMAP_VERSION
    cout << "### WARNING: TTC Emulator not included in firmware" << endl;

  // Stop ongoing auto-trigger when appropriate:
  // generating single triggers or changing the frequency
  if( l1a_cnt > 0 || l1a_freq >= 0 )
    bar2->TTC_EMU_L1A_PERIOD = 0;

  if( reset_emulator )
    {
      bar2->TTC_EMU_RESET = 1;

      // Also reset the TTC decoder (added: 10 Jun 2020)
      bar2->TTC_DEC_CTRL.TOHOST_RST = 1;
      bar2->TTC_DEC_CTRL.TOHOST_RST = 0;
      //bar2->TTC_DEC_CTRL.XL1ID_RST  = 1;
      //bar2->TTC_DEC_CTRL.XL1ID_RST  = 0;

      cout << "TTC Emu + TTC reset: done" << endl;
    }

  if( busy_in_enable != -1 )
    {
      bar2->TTC_EMU_CONTROL.BUSY_IN_ENABLE = busy_in_enable;
    }

  if( enable_emulator )
    {
      // Enable emulator
      if( bar2->TTC_EMU.SEL == 0 || bar2->TTC_EMU.ENA == 0 )
        {
          bar2->TTC_EMU.SEL = 1;
          bar2->TTC_EMU.ENA = 1;
          cout << "TTC Emulator enabled" << endl;
        }
      else
        {
          cout << "TTC Emulator already enabled" << endl;
        }
    }
  else if( disable_emulator )
    {
      if( bar2->TTC_EMU.SEL != 0 || bar2->TTC_EMU.ENA != 0 )
        {
          // Disable emulator
          bar2->TTC_EMU.ENA = 0;
          bar2->TTC_EMU.SEL = 0;
          cout << "TTC Emulator disabled" << endl;
        }
      else
        {
          cout << "TTC Emulator already disabled" << endl;
        }
    }

  if( generate_bcr )
    {
      // Stop ongoing BCR auto-trigger
      bar2->TTC_EMU_BCR_PERIOD = 0;

      bar2->TTC_EMU_CONTROL.BCR = 1;
      bar2->TTC_EMU_CONTROL.BCR = 0;
      cout << "Generate single BCR" << endl;
    }

  if( generate_ecr )
    {
      // Stop ongoing ECR auto-trigger
      bar2->TTC_EMU_ECR_PERIOD = 0;

      // Generate a single ECR
      bar2->TTC_EMU_CONTROL.ECR = 1;
      bar2->TTC_EMU_CONTROL.ECR = 0;
      cout << "Generate single ECR" << endl;
    }

  // Generate any requested specific number of L1A triggers
  for( int i=0; i<l1a_cnt; ++i )
    {
      bar2->TTC_EMU_CONTROL.L1A = 1;
      bar2->TTC_EMU_CONTROL.L1A = 0;
      if( l1a_delay > 0 )
        usleep( l1a_delay );
    }
  if( l1a_cnt > 0 )
    {
      cout << "Generated " << l1a_cnt << " L1As";
      if( l1a_delay > 0 )
        cout << ", at least " << l1a_delay << " us apart";
      cout << endl;
    }

  // Set the requested BCR period (in BC)
  if( bcr_period > 0 )
    {
      bar2->TTC_EMU_BCR_PERIOD = bcr_period;
      cout << "Set BCR period: " << bcr_period << " BC" << endl;
    }

  // Set the requested ECR period, given in milliseconds
  if( ecr_period > 0 )
    {
      //float f = 40000000.0 / ((float) ecr_freq);
      //bar2->TTC_EMU_ECR_PERIOD = (uint64_t) f;
      bar2->TTC_EMU_ECR_PERIOD = (uint64_t) ecr_period * 40000;
      cout << "Set ECR period: " << ecr_period << " ms" << endl;
    }

  // Set the requested L1A frequency (could be 0)
  if( l1a_freq >= 0 )
    {
      float f = 0.0;
      if( l1a_freq > 0 )
        f = 40000000.0 / ((float) l1a_freq);
      bar2->TTC_EMU_L1A_PERIOD = (uint64_t) f;
      cout << "Set L1A frequency: " << l1a_freq << " Hz" << endl;
    }

  // Display the current content of the relevant registers
  cout << "Status:" << endl
       << "TTC_EMU_SEL=" << bar2->TTC_EMU.SEL
       << ", TTC_EMU_ENA=" << bar2->TTC_EMU.ENA
       << endl
       << "TTC_EMU_BUSY_IN_ENABLE=" << bar2->TTC_EMU_CONTROL.BUSY_IN_ENABLE
       << endl
       << "TTC_EMU_BCR_PERIOD=" << bar2->TTC_EMU_BCR_PERIOD
       << " (0x" << hex << setfill('0') << setw(3) << bar2->TTC_EMU_BCR_PERIOD
       << ")" << dec << endl
       << "TTC_EMU_ECR_PERIOD=" << bar2->TTC_EMU_ECR_PERIOD;
  if( bar2->TTC_EMU_ECR_PERIOD != 0 )
    cout << " (" << (float)(40000000.0/((float)bar2->TTC_EMU_ECR_PERIOD))
         << " Hz)";
  cout << endl;
  cout << "TTC_EMU_L1A_PERIOD=" << bar2->TTC_EMU_L1A_PERIOD;
  if( bar2->TTC_EMU_L1A_PERIOD != 0 )
    cout << " (" << (float)(40000000.0/((float)bar2->TTC_EMU_L1A_PERIOD))
         << " Hz)";
  cout << endl;

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
  cout << "fttcemu version " << hex << VERSION_ID << dec << endl <<
    "Show or configure TTC emulator registers and enable or disable "
    "the TTC emulator.\n\n"
    "Usage: fttcemu [-h|V] [-c <cardnr>] [-e|n] [-B <bc>] [-E <period>]\n"
    "               [-f <freq>] [-L <cnt>] [-t <us>] [-R]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -c <cardnr>: FLX-card to use (default: 0).\n"
    "  -e|n       : Enable (-e) or disable (-n) the TTC emulator on the "
    "selected card.\n"
    "  -b <ena>   : Enable (1) or disable (0) TTC emu Busy-In "
    "(default: leave untouched)\n"
    "  -B <bc>    : Set the BCR period, in units of BC (Bunch Count);\n"
    "               for <bc> equal to 0 a single BCR is generated.\n"
    "  -E <period>: Set the ECR period, in ms;\n"
    "               for <period> equal to 0 a single ECR is generated.\n"
    "  -f <freq>  : Set the TTC emulator L1A freqency, in Hz.\n"
    "               (any individually generated L1As (option -L) done first)\n"
    "  -L <cnt>   : Generate <cnt> L1A triggers, using the interval set "
    "by -t (default: 0).\n"
    "               (any single BCR or ECR is generated first).\n"
    "  -t <us>    : The interval (in microseconds, default: 0)\n"
    "               between individually generated L1As (option -L).\n"
    "  -R         : Reset the TTC emulator; also reset the TTC decoder.\n"
    "Note: options -B, -E, -f and -L also enable the TTC emulator, "
    "if necessary.\n"
    "When no option is given the current TTC emulator status "
    "(register contents) is displayed.\n";
}

// ----------------------------------------------------------------------------
