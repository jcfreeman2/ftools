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
const   int VERSION_ID = 0x21091300; // Check firmware type
//const int VERSION_ID = 0x18042400;
//const int VERSION_ID = 0x17121100;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int opt;
  int cardnr = 0;
  int lnknr  = -1;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:G:hV")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'G':
          // Link number
          if( sscanf( optarg, "%d", &lnknr ) != 1 )
            arg_error( 'G' );
          if( lnknr < 0 || lnknr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
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

  uint64_t fw_mode = bar2->FIRMWARE_MODE;
  if( fw_mode != FIRMW_FULL )
    cout << "### WARNING: not FULLMODE firmware" << endl;

  uint64_t chans = bar2->NUM_OF_CHANNELS;
  //if( bar2->CARD_TYPE == 711 || bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  if( lnknr == -1 )
    {
      cout << "### Link number required [0.." << chans-1 << "]" << endl;
      flx->card_close();
      return 1;
    }
  else if( lnknr >= (int) chans )
    {
      cout << "### Invalid Link number [0.." << chans-1
           << "] for this FLX-device" << endl;
      flx->card_close();
      return 1;
    }

  uint64_t xoff_tx = bar2->XOFF_FM_SOFT_XOFF;
  if( xoff_tx & (1 << lnknr) )
    xoff_tx &= ~(1 << lnknr);
  else
    xoff_tx |= (1 << lnknr);

  bar2->XOFF_FM_SOFT_XOFF = xoff_tx;

  cout << "Link " << lnknr << ": ";
  if( xoff_tx & (1 << lnknr) )
    cout << "XOFF generated";
  else
    cout << "XON generated";
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
  cout << "fexofftx version " << hex << VERSION_ID << dec << endl <<
    "Generate an XOFF or XON (toggles between them) on a to-frontend "
    "link, in FULL-mode firmware.\n"
    "Usage: fexofftx [-h|V] [-d <devnr>] -G <lnk>\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -G <lnk>   : Link number.\n";
}

// ----------------------------------------------------------------------------
