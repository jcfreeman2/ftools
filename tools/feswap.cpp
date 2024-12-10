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
const int VERSION_ID = 0x17040600;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int opt;
  int cardnr   = 0;
  int gbtnr    = -1;
  int egroupnr = -1;
  int set      = -1;
  string operation;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:g:G:hV")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
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
          if( egroupnr < 0 || egroupnr >= FLX_FROMGBT_GROUPS-1 )
            arg_range( 'g', 0, FLX_FROMGBT_GROUPS-1-1 );
          //if( egroupnr < 0 || egroupnr >= FLX_TOGBT_GROUPS-1 )
          //  arg_range( 'g', 0, FLX_TOGBT_GROUPS-1-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        default: // '?'
          usage();
          return 1;
        }
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

  int max_egrp;
  if( egroupnr == -1 )
    {
      egroupnr = 0;
      //max_egrp = FLX_FROMGBT_GROUPS-1;
      max_egrp = FLX_TOGBT_GROUPS-1;
    }
  else
    {
      max_egrp = egroupnr + 1;
    }

  uint64_t config;
  for( int grp=egroupnr; grp<max_egrp; ++grp )
    {
      // To-Host
      //config = bar2->CR_GBT_CTRL[gbtnr].EGROUP_TH[grp].TH;
      config = bar2->CR_GBT_CTRL[gbtnr].EGROUP_TOHOST[grp].
        TOHOST.SWAP_2BIT_ELINKS;
      // Bit 43 (MSB of 11th nibble)
      if( set == 1 )
        {
          //config |= 0x080000000000;
          //bar2->CR_GBT_CTRL[gbtnr].EGROUP_TH[grp].TH = config;
          config = 1;
          bar2->CR_GBT_CTRL[gbtnr].EGROUP_TOHOST[grp].
            TOHOST.SWAP_2BIT_ELINKS = config;
        }
      else if( set == 0 )
        {
          //config &= ~0x080000000000;
          //bar2->CR_GBT_CTRL[gbtnr].EGROUP_TH[grp].TH = config;
          config = 0;
          bar2->CR_GBT_CTRL[gbtnr].EGROUP_TOHOST[grp].
            TOHOST.SWAP_2BIT_ELINKS = config;
        }

      cout << "GBT " << gbtnr << " egroup " << grp << " TH: ";
      //if( config & 0x080000000000 )
      if( config == 1 )
        cout << "ENABLED";
      else
        cout << "disabled";
      cout << endl;
    }

  // Latch TOHOST configuration data
  //bar2->CR_TH_UPDATE_CTRL = 1;
  //bar2->CR_TH_UPDATE_CTRL = 0;

  for( int grp=egroupnr; grp<max_egrp; ++grp )
    {
      // From-Host
      //config = bar2->CR_GBT_CTRL[gbtnr].EGROUP_FH[grp].FH;
      config = bar2->CR_GBT_CTRL[gbtnr].EGROUP_FROMHOST[grp].
        FROMHOST.SWAP_2BIT_ELINKS;
      // Bit 47 (MSB of 12th nibble)
      if( set == 1 )
        {
          //config |= 0x800000000000;
          //bar2->CR_GBT_CTRL[gbtnr].EGROUP_FH[grp].FH = config;
          config = 1;
          bar2->CR_GBT_CTRL[gbtnr].EGROUP_FROMHOST[grp].
            FROMHOST.SWAP_2BIT_ELINKS = config;
        }
      else if( set == 0 )
        {
          //config &= ~0x800000000000;
          //bar2->CR_GBT_CTRL[gbtnr].EGROUP_FH[grp].FH = config;
          config = 0;
          bar2->CR_GBT_CTRL[gbtnr].EGROUP_FROMHOST[grp].
            FROMHOST.SWAP_2BIT_ELINKS = config;
        }

      cout << "GBT " << gbtnr << " egroup " << grp << " FH: ";
      //if( config & 0x800000000000 )
      if( config == 1 )
        cout << "ENABLED";
      else
        cout << "disabled";
      cout << endl;
    }

  // Latch FROMHOST configuration data
  //bar2->CR_FH_UPDATE_CTRL = 1;
  //bar2->CR_FH_UPDATE_CTRL = 0;

  // Close the FLX-card
  try {
    flx->card_close();
  }
  catch( FlxException &ex ) {
    cout << "FlxCard close: " << ex.what() << endl;
  }

  cout << " NB: USE **fereverse** INSTEAD" << endl;

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "feswap version " << hex << VERSION_ID << dec << endl <<
    "Enable, disable or display the bitswap feature for 2-bit e-links,\n"
    "a setting per e-group.\n"
    " NB: USE **fereverse** INSTEAD, NOW SETTING PER E-PATH\n"
    "Usage: feswap [-h|V] [-d <devnr>] -G <gbt> [-g <group>] [set|reset]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number (default: all groups).\n"
    "  set        : Enable bitswap.\n"
    "  reset      : Disable bitswap.\n"
    " (without keyword '(re)set' the current setting is displayed)\n";
}

// ----------------------------------------------------------------------------
