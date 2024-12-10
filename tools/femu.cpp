#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x20042100; // Check for firmware mode 8 (MROD)
//const int VERSION_ID = 0x18112800; // Wait before touching fan-out at disable
//const int VERSION_ID = 0x17091300;

//const uint64_t FO_SEL_LOCKBIT = 0x80000000;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr               = 0;
  bool use_emulator         = false;
  bool emulator_external    = false;
  bool unlock_fanout        = false;
  bool lock_fanout          = false;
  bool tohost_fanout_to_emu = false;
  bool get_status           = true;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:eEfhlLnV")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'E':
          if( use_emulator || get_status == false )
            {
              cout << "### Select -e, -E or -n\n";
              return 1;
            }
          emulator_external = true;
          use_emulator = true;
          get_status = false;
          break;
        case 'e':
          if( use_emulator || get_status == false )
            {
              cout << "### Select -e, -E or -n\n";
              return 1;
            }
          use_emulator = true;
          get_status = false;
          break;
        case 'f':
          tohost_fanout_to_emu = true;
          break;
        case 'h':
          usage();
          return 0;
        case 'l':
          unlock_fanout = true;
          break;
        case 'L':
          lock_fanout = true;
          break;
        case 'n':
          if( use_emulator || get_status == false )
            {
              cout << "### Select -e, -E or -n\n";
              return 1;
            }
          use_emulator = false;
          get_status = false;
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

  if( lock_fanout && unlock_fanout )
    {
      cout << "###Choose lock (-l) OR unlock (-L) FanOut-Select" << endl;
      lock_fanout = false;
      unlock_fanout = false;
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

  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) flx->bar2Address();

  if( lock_fanout || unlock_fanout )
    {
      if( lock_fanout )
        {
          bar2->GBT_TOFRONTEND_FANOUT.LOCK = 1;
          bar2->GBT_TOHOST_FANOUT.LOCK     = 1;
          cout << "FanOut-Select locked\n";
        }
      else
        {
          bar2->GBT_TOFRONTEND_FANOUT.LOCK = 0;
          bar2->GBT_TOHOST_FANOUT.LOCK     = 0;
          cout << "FanOut-Select unlocked\n";
        }
    }
  uint64_t lockbit = ((bar2->GBT_TOFRONTEND_FANOUT.LOCK == 1) |
                      (bar2->GBT_TOHOST_FANOUT.LOCK == 1));

  cout << hex << setfill('0');

  uint64_t fw_mode = bar2->FIRMWARE_MODE;

  if( get_status )
    {
      // Just display the current content of the relevant registers
      cout << "Status:" << endl;
      if( fw_mode == FIRMW_FULL )
#if REGMAP_VERSION < 0x500
        cout << "FM_EMU_ENA_TOHOST=" << bar2->GBT_FM_EMU_ENA_TOHOST;
#else
        cout << "FM_EMU_ENA_TOHOST=" << bar2->FE_EMU_ENA.EMU_TOHOST;
#endif
      else if( fw_mode == FIRMW_FELIG )
        cout << "FELIG";
      else if( fw_mode == FIRMW_FMEMU )
        cout << "FMEMU";
      else if( fw_mode == FIRMW_MROD )
        cout << "MROD";
      else
#if REGMAP_VERSION < 0x500
        cout << "EMU_ENA_TOFE=" << bar2->GBT_EMU_ENA.TOFRONTEND
             << ", EMU_ENA_TOHOST=" << bar2->GBT_EMU_ENA.TOHOST;
#else
        cout << "EMU_ENA_TOFE=" << bar2->FE_EMU_ENA.EMU_TOFRONTEND
             << ", EMU_ENA_TOHOST=" << bar2->FE_EMU_ENA.EMU_TOHOST;
#endif
    }
  else
    {
      if( use_emulator )
        {
          if( emulator_external )
            {
              // FanOut-Select registers
              bar2->GBT_TOFRONTEND_FANOUT.SEL = 0xFFFFFF; // From emulator
              bar2->GBT_TOHOST_FANOUT.SEL     = 0x000000;
              // Emulators
#if REGMAP_VERSION < 0x500
              bar2->GBT_EMU_ENA.TOFRONTEND = 1;
              bar2->GBT_EMU_ENA.TOHOST     = 0;
              bar2->GBT_FM_EMU_ENA_TOHOST  = 0;
#else
              bar2->FE_EMU_ENA.EMU_TOFRONTEND = 1;
              bar2->FE_EMU_ENA.EMU_TOHOST     = 0;
#endif
              cout << "TOFRONTEND Emulator";
            }
          else
            {
              // FanOut-Select registers
              bar2->GBT_TOFRONTEND_FANOUT.SEL = 0x000000;
              bar2->GBT_TOHOST_FANOUT.SEL     = 0xFFFFFF; // From emulator
              // Emulators
              if( fw_mode == FIRMW_FULL )
                {
#if REGMAP_VERSION < 0x500
                  bar2->GBT_FM_EMU_ENA_TOHOST = 1;
#else
                  bar2->FE_EMU_ENA.EMU_TOHOST = 1;
#endif
                  cout << "FullMode Emulator";  
                }
              else
                {
#if REGMAP_VERSION < 0x500
                  bar2->GBT_EMU_ENA.TOFRONTEND = 0;
                  bar2->GBT_EMU_ENA.TOHOST     = 1;
#else
                  bar2->FE_EMU_ENA.EMU_TOFRONTEND = 0;
                  bar2->FE_EMU_ENA.EMU_TOHOST  = 1;
#endif
                  cout << "TOHOST Emulator";
                }
            }
        }
      else
        {
          // Disable emulators
#if REGMAP_VERSION < 0x500
          bar2->GBT_EMU_ENA.TOFRONTEND = 0;
          bar2->GBT_EMU_ENA.TOHOST     = 0;
          bar2->GBT_FM_EMU_ENA_TOHOST  = 0;
#else
          bar2->FE_EMU_ENA.EMU_TOFRONTEND = 0;
          bar2->FE_EMU_ENA.EMU_TOHOST  = 0;
#endif
          cout << "Emulator disabled";

          // Touching fan-out with data still flowing is a bad idea:
          // give it some time...
          usleep( 250000 );

          // FanOut-Select registers
          bar2->GBT_TOFRONTEND_FANOUT.SEL = 0x000000;
          if( tohost_fanout_to_emu )
            bar2->GBT_TOHOST_FANOUT.SEL = 0xFFFFFF;
          else
            bar2->GBT_TOHOST_FANOUT.SEL = 0x000000;
        }
    }

  if( !(fw_mode == FIRMW_FELIG || fw_mode == FIRMW_FMEMU) )
    {
      cout << ", FROMHOST_FANOUT=" << setw(8) << bar2->GBT_TOFRONTEND_FANOUT.SEL
           << " LOCK=" << bar2->GBT_TOFRONTEND_FANOUT.LOCK
           << ", TOHOST_FANOUT=" << setw(8) << bar2->GBT_TOHOST_FANOUT.SEL
           << " LOCK=" << bar2->GBT_TOHOST_FANOUT.LOCK;
      if( lockbit != 0 )
        cout << " (Locked)";
    }
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
  cout << "femu version " << hex << VERSION_ID << dec << endl <<
    "Show or configure 'FanOut-Select' registers and start/stop emulator.\n"
    "Usage: femu [-h|V] [-d <devnr>] [-e|E|n] [-l]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e|E|n     : Enable FLX-device data emulator, internal (e) or "
    "external (E) or disable emulator (n).\n"
    "               When no option is given the current status is displayed.\n"
    "  -f         : When disabling emulator set TOHOST_FANOUT to emulator "
    "(default: to external).\n"
    "  -l         : 'Unlock' FanOut-Select registers.\n"
    "  -L         : 'Lock' FanOut-Select registers.\n";
}

// ----------------------------------------------------------------------------
