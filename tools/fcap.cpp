#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x20052500;

string firmware_version( FlxCard *flx );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  devnr = 0;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:hV")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &devnr ) != 1 )
            arg_error( 'd' );
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
    flx->card_open( devnr, LOCK_NONE );
  }
  catch( FlxException &ex ) {
    cout << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }

  uint64_t bar2_addr = flx->openBackDoor( 2 );
  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

  bool wide_mode = (bar2->WIDE_MODE == 1);
  int  groups = (wide_mode ? 7 : 5);

  cout << "Firmware  : " << firmware_version( flx ) << endl;

  cout << "Trailer   : "
       << (((bar2->CHUNK_TRAILER_32B & 1) == 1) ? "32-bit" : "16-bit" )
       << endl;

  cout << "Blocksize : " << bar2->BLOCKSIZE << endl;

  cout << "Features (YES/NO): "
       << "FromHost:"
       << ((bar2->CR_GENERICS.FROM_HOST_INCLUDED  == 1) ? "YES" : "NO" );
       //<< endl;

  cout << " DirectMode:"
       << ((bar2->CR_GENERICS.DIRECT_MODE_INCLUDED  == 1) ? "YES" : "NO" );
       //<< endl;

  cout << " Xoff:"
       << ((bar2->CR_GENERICS.XOFF_INCLUDED  == 1) ? "YES" : "NO" );
       //<< endl;

#if REGMAP_VERSION < 0x500
  cout << " TTCemu:"
       << (((bar2->TTC_EMU_CONST_GENERATE_TTC_EMU & 2) == 2) ? "YES" : "NO" )
       << endl;
#else
  cout << " TTCemu:"
       << (((bar2->GENERATE_TTC_EMU & 2) == 2) ? "YES" : "NO" )
       << endl;
#endif

  cout << "E-link configurability:" << endl;
  cout << "Egroup |    ToHost    |  FromHost" << endl;
  cout << "-----------------------------------" << endl;
  flxcard_include_egroup_t *ig;
  for( int g=0; g<groups; ++g )
    {
      ig = &bar2->INCLUDE_EGROUPS[g].INCLUDE_EGROUP;
      cout << "   " << g << "       ";

      int chars = 0;
      ostringstream oss;
      // ToHost
      if( ig->TOHOST_02 == 1 )
        {
          oss << "2";
          ++chars;
        }
      if( ig->TOHOST_04 == 1 )
        {
          if( chars )
            {
              oss << ",";
              ++chars;
            }
          oss << "4";
          ++chars;
        }
      if( ig->TOHOST_08 == 1 )
        {
          if( chars )
            {
              oss << ",";
              ++chars;
            }
          oss << "8";
          ++chars;
        }
      if( ig->TOHOST_16 == 1 )
        {
          if( chars )
            {
              oss << ",";
              ++chars;
            }
          oss << "16";
          chars += 2;
        }
      if( ig->TOHOST_HDLC == 1 )
        {
          if( chars )
            {
              oss << ",";
              ++chars;
            }
          oss << "HDLC";
          chars += 4;
        }

      if( chars == 0 )
        {
          oss << "    ---";
          chars = 7;
        }

      cout << oss.str();

      for( ; chars<14; ++chars )
        cout << " ";

      // FromHost
      chars = 0;
      if( ig->FROMHOST_02 == 1 )
        {
          cout << "2";
          ++chars;
        }
      if( ig->FROMHOST_04 == 1 )
        {
          if( chars ) cout << ",";
          cout << "4";
          ++chars;
        }
      if( ig->FROMHOST_08 == 1 )
        {
          if( chars ) cout << ",";
          cout << "8";
          ++chars;
        }
      if( ig->FROMHOST_HDLC == 1 )
        {
          if( chars ) cout << ",";
          cout << "HDLC";
          ++chars;
        }
      if( chars == 0 )
        {
          cout << "---";
        }
      cout << endl;        
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

string firmware_version( FlxCard *flx )
{
  if( flx == 0 ) return std::string();

  std::ostringstream oss;
  uint64_t ts  = flx->cfg_get_reg( REG_BOARD_ID_TIMESTAMP );
  uint64_t fw  = flx->cfg_get_reg( REG_FIRMWARE_MODE );
  uint64_t chn = flx->cfg_get_reg( REG_NUM_OF_CHANNELS );
  uint64_t typ = flx->cfg_get_reg( REG_CARD_TYPE );

  oss << "FLX" << typ << "-";
  if( fw == 0 )
    oss << "GBT-";
  else if( fw == 1 )
    oss << "FULLMODE-";
  else if( fw == 2 )
    oss << "LTDB-";
  else if( fw == 3 )
    oss << "FEI4-";
  else if( fw == 4 )
    oss << "PIXEL-";
  else if( fw == 5 )
    oss << "STRIP-";
  else if( fw == 6 )
    oss << "FELIG-";
  else if( fw == 7 )
    oss << "FMEMU-";
  else if( fw == 8 )
    oss << "MROD-";
  else
    oss << "?" << "?" << "?-"; // Avoid 'trigraph'
  oss << chn << "chan-" << std::hex << ts;

  uint64_t git = flx->cfg_get_reg( REG_GIT_TAG );
  oss << "-GIT:";
  char ch;
  for( int i=0; i<8; ++i, git>>=8 )
    {
      ch = (char) (git & 0xFF);
      if( ch == 0 ) break;
      oss << ch;
    }
  git = flx->cfg_get_reg( REG_GIT_COMMIT_NUMBER );
  oss << "/" << std::dec << git;
  return oss.str();
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fcap version " << hex << VERSION_ID << dec << endl <<
    "Show FLX-device capabilities, including E-link configurability.\n"
    "(consider this an addition to flx-info information)\n"
    "Usage: fcap [-h|V] [-d <devnr>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n";
}

// ----------------------------------------------------------------------------
