#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxdefs.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x17121300; // Fix bug in TTC time-out counter set
//const int VERSION_ID = 0x17072600;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr    = 0;
  int  gbtnr     = -1;
  int  egroupnr  = -1;
  int  epathnr   = -1;
  int  elinknr   = -1;
  int  set       = -1;
  bool global    = false;
  int  global_to = -1;
  bool ttc       = false;
  int  ttc_to    = -1;
  string operation;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:e:g:G:hp:TV")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'e':
          // E-link number
          unsigned int x;
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elinknr = x;
          if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
          break;
        case 'G':
          // GBT link number
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'g':
          // E-group number
          if( sscanf( optarg, "%d", &egroupnr ) != 1 )
            arg_error( 'g' );
          if( egroupnr < 0 || egroupnr >= FLX_TOHOST_GROUPS-1 )
            arg_range( 'g', 0, FLX_TOHOST_GROUPS-1-1 );
          //if( egroupnr < 0 || egroupnr >= FLX_FROMHOST_GROUPS-1 )
          //  arg_range( 'g', 0, FLX_FROMHOST_GROUPS-1-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'p':
          // E-path number
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'T':
          ttc = true;
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

  // Check for either a valid -e or valid -G/g/p options
  if( elinknr == -1 && gbtnr == -1 && egroupnr == -1 && epathnr == -1 )
    {
      // Going to read or configure TTC time-out or global time-out
      if( !ttc ) global = true;
    }
  else if( (elinknr != -1 && (gbtnr != -1 || egroupnr != -1 || epathnr != -1)) )
    {
      cout << "### Provide either -e or -G/g/p options" << endl;
      return 1;
    }
  else if( ttc )
    {
      cout << "(ignoring option -T)" << endl;
      ttc = false;
    }
  if( elinknr != -1 )
    {
      // Derive GBT, e-group and e-path numbers from the given e-link number
      gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
      egroupnr = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
      epathnr  = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
    }

  // String defining the operation to perform?
  if( optind < argc )
    {
      operation = string( argv[optind] );
      if( operation == string("set") )
        set = 1;
      else if( operation == string("reset") )
        set = 0;
      else
        {
          cout << "### Unknown qualifier: " << operation << endl;
          usage();
          return 1;
        }

      if( global || ttc )
        {
          ++optind;
          if( optind < argc )
            {
              // Accept hex values "x12" and "0x12", as well as decimal "12"
              int result, i = 0;
              if( argv[optind][0] == 'x' || argv[optind][0] == 'X' )
                i = 1;
              else if( argv[optind][0] == '0' &&
                       (argv[optind][1] == 'x' || argv[optind][1] == 'X') )
                i = 2;
              if( i != 0 )
                {
                  unsigned int g;
                  result = sscanf( &argv[optind][i], "%x", &g );
                  if( result == 1 )
                    global_to = (int) g;
                }
              else
                {
                  result = sscanf( &argv[optind][i], "%d", &global_to );
                }
              if( result != 1 )
                {
                  cout << "### Invalid counter value" << endl;
                  return 1;
                }
            }
          if( ttc )
            ttc_to = global_to;
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

  uint64_t bar2_addr = flx->openBackDoor( 2 );
  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

  if( global || ttc )
    {
      if( global )
        {
          // Read or configure the global time-out and counter
          if( global_to >= 0 )
            bar2->TIMEOUT_CTRL.TIMEOUT = global_to;

          if( set == 1 )
            bar2->TIMEOUT_CTRL.ENABLE = 1;
          else if( set == 0 )
            bar2->TIMEOUT_CTRL.ENABLE = 0;

          cout << "Global Timeout: " << bar2->TIMEOUT_CTRL.ENABLE
               << ", counter=" << bar2->TIMEOUT_CTRL.TIMEOUT << endl;
        }
      else
        {
          // Read or configure the TTC time-out and counter
#if REGMAP_VERSION < 0x500
          if( ttc_to >= 0 )
            bar2->CR_TTC_TOHOST.TIMEOUT_VALUE = global_to;

          if( set == 1 )
            bar2->CR_TTC_TOHOST.TIMEOUT_ENABLE = 1;
          else if( set == 0 )
            bar2->CR_TTC_TOHOST.TIMEOUT_ENABLE = 0;

          cout << "TTC Timeout: " << bar2->CR_TTC_TOHOST.TIMEOUT_ENABLE
               << ", counter=" << bar2->CR_TTC_TOHOST.TIMEOUT_VALUE << endl;
#else
          ttc_to = ttc_to;
#endif // REGMAP_VERSION
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

  uint64_t chans = bar2->NUM_OF_CHANNELS;
  //if( bar2->CARD_TYPE == 711 || bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  if( gbtnr == -1 )
    {
      cout << "### GBT number required [0.." << chans-1 << "]" << endl;
      flx->card_close();
      return 1;
    }
  if( gbtnr < 0 || gbtnr >= (int) chans )
    {
      cout << "### Invalid GBT number [0.." << chans-1
           << "] for this FLX-card" << endl;
      flx->card_close();
      return 1;
    }

  //if( egroupnr == -1 )
  //  {
  //    cout << "### E-group number required [0.."
  //         << FLX_FROMHOST_GROUPS-1 << "]" << endl;
  //    flx->card_close();
  //    return 1;
  //  }
  int max_egrp;
  if( egroupnr == -1 )
    {
      egroupnr = 0;
      //max_egrp = FLX_TOHOST_GROUPS-1;
      max_egrp = FLX_FROMHOST_GROUPS-1;
    }
  else
    {
      max_egrp = egroupnr + 1;
    }

  int max_epath;
  if( epathnr == -1 )
    {
      epathnr = 0;
      max_epath = FLX_ELINK_PATHS;
    }
  else
    {
      max_epath = epathnr + 1;
    }
#if REGMAP_VERSION < 0x500
  uint64_t *pc, config;
  for( int grp=egroupnr; grp<max_egrp; ++grp )
    for( int p=epathnr; p<max_epath; ++p )
      {
        // To-Host
        pc = (uint64_t *) &bar2->CR_GBT_CTRL[gbtnr].EGROUP_TOHOST[grp].TOHOST;
        config = *pc;
        // Bit 51 + epath
        if( set == 1 )
          {
            config |= ((uint64_t)1 << (51 + p));
            //bar2->CR_GBT_CTRL[gbtnr].EGROUP_TH[grp].TH = config;
            *pc = config;
          }
        else if( set == 0 )
          {
            config &= ~((uint64_t)1 << (51 + p));
            //bar2->CR_GBT_CTRL[gbtnr].EGROUP_TH[grp].TH = config;
            *pc = config;
          }

        cout << "GBT " << gbtnr << " egroup " << grp
             << " epath " << p << ": ";
        if( config & ((uint64_t)1<<(51 + p)) )
          cout << "ENABLED";
        else
          cout << "disabled";
        cout << endl;
      }
#else
  max_epath = max_epath;
  max_egrp  = max_egrp;
#endif // REGMAP_VERSION
  // Latch TOHOST configuration data
  //bar2->CR_TH_UPDATE_CTRL = 1;
  //bar2->CR_TH_UPDATE_CTRL = 0;

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
  cout << "feto version " << hex << VERSION_ID << dec << endl <<
    "Enable, disable or display the instant time-out setting,\n"
    "a setting per e-path (e-link), or the so-called global time-out\n"
    "and associated time-out counter (number of clocks until time-out),\n"
    "or the TTC time-out and associated counter.\n"
    "Without keyword '(re)set' the current setting of the requested\n"
    "(group of) time-outs is displayed.\n\n"
    "Usage: feto [-h|V] [-d <devnr>] [-e <elink>] "
    "[-G <gbt> [-g <group>] [-p <path>]]\n"
    "            [-T] [set|reset] [<globcntr>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number (default: all groups).\n"
    "  -p <path>  : E-path number (default: all paths).\n"
    "  -T         : Read or configure TTC time-out.\n"
    "  set        : Enable time-out.\n"
    "  reset      : Disable time-out.\n"
    "  <globcntr> : Global or TTC time-out counter value to set.\n";
}

// ----------------------------------------------------------------------------
