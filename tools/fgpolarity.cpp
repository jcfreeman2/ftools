#include <iostream>
#include <iomanip>
#include <cstdint>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxdefs.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x21121600; // Option -c instead of -d (card-based)
//const int VERSION_ID = 0x17060900;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    cardnr   = 0;
  int    devnr    = 0;
  int    gbtnr    = -1;
  int    set      = -1;
  bool   do_tx   = true;
  bool   do_rx   = true;
  string operation;

  // Parse the options
  while( (opt = getopt(argc, argv, "c:G:rthV")) != -1 )
    {
      switch( opt )
        {
        case 'c':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'c' );
          break;
        case 'G':
          // GBT link number
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'r':
          do_tx = false;
          break;
        case 't':
          do_rx = false;
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

  uint64_t chans = bar2->NUM_OF_CHANNELS;
  uint64_t typ   = bar2->CARD_TYPE;
  if( typ == 712 || typ == 711 || typ == 710 || typ == 128 )
    chans *= 2;

  int max_gbtnr;
  if( gbtnr == -1 )
    {
      gbtnr = 0;
      max_gbtnr = chans;
    }
  else if( gbtnr >= 0 && gbtnr < (int) chans )
    {
      max_gbtnr = gbtnr + 1;
    }
  else
    {
      cout << "### Invalid GBT number [0.." << chans-1
           << "] for this FLX-device" << endl;
      flx->card_close();
      return 1;
    }

  uint64_t rx_pol = bar2->GBT_RXPOLARITY;
  uint64_t tx_pol = bar2->GBT_TXPOLARITY;
  for( int gbt=gbtnr; gbt<max_gbtnr; ++gbt )
    {
      cout << "GBT " << setw(2) << gbt << " polarity: ";
      if( do_rx )
        {
          if( set == 1 )
            rx_pol |= (1 << gbt);
          else if( set == 0 )
            rx_pol &= ~(1 << gbt);

          cout << " RX = ";
          if( rx_pol & (1 << gbt) )
            cout << "1";
          else
            cout << "0";
        }
      if( do_tx )
        {
          if( set == 1 )
            tx_pol |= (1 << gbt);
          else if( set == 0 )
            tx_pol &= ~(1 << gbt);

          if( do_rx ) cout << " ";
          cout << " TX = ";
          if( tx_pol & (1 << gbt) )
            cout << "1";
          else
            cout << "0";
        }
      cout << endl;
    }

  if( set == 1 || set == 0 )
    {
      if( do_rx )
        bar2->GBT_RXPOLARITY = rx_pol;
      if( do_tx )
        bar2->GBT_TXPOLARITY = tx_pol;
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
  cout << "fgpolarity version " << hex << VERSION_ID << dec << endl <<
    "Configure or display the GBT transceivers RX and TX polarity.\n"
    "Usage: fgpolarity [-h|V] [-c <cardnr>] [-G <gbt>] [set|reset]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -c <cardnr>: FLX-card to use (default: 0).\n"
    "  -G <gbt>   : GBT-link number (default: all).\n"
    "  -r         : Configure RX only.\n"
    "  -t         : Configure TX only.\n"
    "  set        : Set reverse polarity for given GBT transceiver(s).\n"
    "  reset      : Set default polarity for given GBT transceiver(s).\n"
    " (without keyword '(re)set' the current setting is displayed)\n";
}

// ----------------------------------------------------------------------------
