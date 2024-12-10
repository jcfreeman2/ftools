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
const int VERSION_ID = 0x21011300;   // Options -E/-I added
//const int VERSION_ID = 0x18110700; // Default all GBT links
//const int VERSION_ID = 0x17121100; // Fix bug
//const int VERSION_ID = 0x17060500;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr   = 0;
  int  gbtnr    = -1;
  int  egroupnr = -1;
  int  epathnr  = -1;
  int  elinknr  = -1;
  int  set      = -1;
  bool do_th    = true;
  bool do_fh    = true;
  bool do_ic    = false;
  bool do_ec    = false;
  string operation;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:e:Efg:G:hIp:tV")) != -1 )
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
        case 'E':
          do_ec = true;
          break;
        case 'f':
          do_th = false;
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
        case 'I':
          do_ic = true;
          break;
        case 'p':
          // E-path number
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 't':
          do_fh = false;
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
  if( (elinknr != -1 && (gbtnr != -1 || egroupnr != -1 || epathnr != -1)) )
    {
      cout << "### Provide either -e or -G/g/p options" << endl;
      return 1;
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

  uint64_t chans = bar2->NUM_OF_CHANNELS;
  //if( bar2->CARD_TYPE == 711 || bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  /*if( gbtnr == -1 )
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
  */
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

  int max_egroupnr;
  if( egroupnr == -1 )
    {
      egroupnr = 0;
      //max_egroupnr = FLX_TOHOST_GROUPS-1;
      max_egroupnr = FLX_FROMHOST_GROUPS-1;
    }
  else
    {
      max_egroupnr = egroupnr + 1;
    }

  int max_epathnr;
  if( epathnr == -1 )
    {
      epathnr = 0;
      max_epathnr = FLX_ELINK_PATHS;
    }
  else
    {
      max_epathnr = epathnr + 1;
    }

  uint64_t config;
  int display_cnt = 0;
  if( do_th )
    {
      if( do_ic || do_ec )
        {
          for( int gbt=gbtnr; gbt<max_gbtnr; ++gbt )
            {
#if REGMAP_VERSION < 0x500
              flxcard_ec_tohost_t *ec_tohost =
                &bar2->MINI_EGROUP_CTRL[gbt].EC_TOHOST;
              if( do_ec )
                {
                  if( set == 1 )
                    ec_tohost->BIT_SWAPPING = 1;
                  else if( set == 0 )
                    ec_tohost->BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " EC TH swapped: "
                       << ec_tohost->BIT_SWAPPING << endl;
                }
              if( do_ic )
                {
                  if( set == 1 )
                    ec_tohost->IC_BIT_SWAPPING = 1;
                  else if( set == 0 )
                    ec_tohost->IC_BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " IC TH swapped: "
                       << ec_tohost->IC_BIT_SWAPPING << endl;
                }
#else
              flxcard_mini_egroup_tohost_t *mini_egroup_tohost =
                &bar2->MINI_EGROUP_TOHOST_GEN[gbt].MINI_EGROUP_TOHOST;
              if( do_ec )
                {
                  if( set == 1 )
                    mini_egroup_tohost->EC_BIT_SWAPPING = 1;
                  else if( set == 0 )
                    mini_egroup_tohost->EC_BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " EC TH swapped: "
                       << mini_egroup_tohost->EC_BIT_SWAPPING << endl;
                }
              if( do_ic )
                {
                  if( set == 1 )
                    mini_egroup_tohost->IC_BIT_SWAPPING = 1;
                  else if( set == 0 )
                    mini_egroup_tohost->IC_BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " IC TH swapped: "
                       << mini_egroup_tohost->IC_BIT_SWAPPING << endl;
                }
#endif // REGMAP_VERSION
              ++display_cnt;
            }
        }
      else
        {
          for( int gbt=gbtnr; gbt<max_gbtnr; ++gbt )
            for( int grp=egroupnr; grp<max_egroupnr; ++grp )
              for( int p=epathnr; p<max_epathnr; ++p )
                {
#if REGMAP_VERSION < 0x500
                  // To-Host
                  config = bar2->CR_GBT_CTRL[gbt].EGROUP_TOHOST[grp].
                    TOHOST.REVERSE_ELINKS;
                  // Bit 43 + epath (starting from MSB of 11th nibble)
                  if( set == 1 )
                    {
                      config |= ((uint64_t) 1 << p);
                      bar2->CR_GBT_CTRL[gbt].EGROUP_TOHOST[grp].
                        TOHOST.REVERSE_ELINKS = config;
                    }
                  else if( set == 0 )
                    {
                      config &= ~((uint64_t) 1 << p);
                      bar2->CR_GBT_CTRL[gbt].EGROUP_TOHOST[grp].
                        TOHOST.REVERSE_ELINKS = config;
                    }
#else
                  // To-Host
                  config = bar2->DECODING_EGROUP_CTRL_GEN[gbt].DECODING_EGROUP[grp].
                    EGROUP.REVERSE_ELINKS;
                  // Bit 43 + epath (starting from MSB of 11th nibble)
                  if( set == 1 )
                    {
                      config |= ((uint64_t) 1 << p);
                      bar2->DECODING_EGROUP_CTRL_GEN[gbt].DECODING_EGROUP[grp].
                        EGROUP.REVERSE_ELINKS = config;
                    }
                  else if( set == 0 )
                    {
                      config &= ~((uint64_t) 1 << p);
                      bar2->DECODING_EGROUP_CTRL_GEN[gbt].DECODING_EGROUP[grp].
                        EGROUP.REVERSE_ELINKS = config;
                    }
#endif // REGMAP_VERSION

                  cout << "GBT " << gbt << " egroup " << grp
                       << " epath " << p << " TH: ";
                  if( config & ((uint64_t) 1 << p) )
                    cout << "ENABLED";
                  else
                    cout << "disabled";
                  cout << endl;
                  ++display_cnt;
                }
        }
    }

  if( do_fh )
    {
      if( display_cnt > 1 ) cout << endl;

      if( do_ic || do_ec )
        {
          for( int gbt=gbtnr; gbt<max_gbtnr; ++gbt )
            {
#if REGMAP_VERSION < 0x500
              flxcard_ec_fromhost_t *ec_fromhost =
                &bar2->MINI_EGROUP_CTRL[gbt].EC_FROMHOST;
              if( do_ec )
                {
                  if( set == 1 )
                    ec_fromhost->BIT_SWAPPING = 1;
                  else if( set == 0 )
                    ec_fromhost->BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " EC FH swapped: "
                       << ec_fromhost->BIT_SWAPPING << endl;
                }
              if( do_ic )
                {
                  if( set == 1 )
                    ec_fromhost->IC_BIT_SWAPPING = 1;
                  else if( set == 0 )
                    ec_fromhost->IC_BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " IC FH swapped: "
                       << ec_fromhost->IC_BIT_SWAPPING << endl;
                }
#else
              flxcard_mini_egroup_fromhost_t *mini_egroup_fromhost =
                &bar2->MINI_EGROUP_FROMHOST_GEN[gbt].MINI_EGROUP_FROMHOST;
              if( do_ec )
                {
                  if( set == 1 )
                    mini_egroup_fromhost->EC_BIT_SWAPPING = 1;
                  else if( set == 0 )
                    mini_egroup_fromhost->EC_BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " EC FH swapped: "
                       << mini_egroup_fromhost->EC_BIT_SWAPPING << endl;
                }
              if( do_ic )
                {
                  if( set == 1 )
                    mini_egroup_fromhost->IC_BIT_SWAPPING = 1;
                  else if( set == 0 )
                    mini_egroup_fromhost->IC_BIT_SWAPPING = 0;
                  cout << "GBT " << gbt << " IC FH swapped: "
                       << mini_egroup_fromhost->IC_BIT_SWAPPING << endl;
                }
#endif // REGMAP_VERSION
            }
        }
      else
        {
          for( int gbt=gbtnr; gbt<max_gbtnr; ++gbt )
            for( int grp=egroupnr; grp<max_egroupnr; ++grp )
              for( int p=epathnr; p<max_epathnr; ++p )
                {
#if REGMAP_VERSION < 0x500
                  // From-Host
                  config = bar2->CR_GBT_CTRL[gbt].EGROUP_FROMHOST[grp].
                    FROMHOST.REVERSE_ELINKS;
                  // Bit 47 (MSB of 12th nibble)
                  if( set == 1 )
                    {
                      config |= ((uint64_t) 1 << p);
                      bar2->CR_GBT_CTRL[gbt].EGROUP_FROMHOST[grp].
                        FROMHOST.REVERSE_ELINKS = config;
                    }
                  else if( set == 0 )
                    {
                      config &= ~((uint64_t) 1 << p);
                      bar2->CR_GBT_CTRL[gbt].EGROUP_FROMHOST[grp].
                        FROMHOST.REVERSE_ELINKS = config;
                    }
#else
                  // From-Host
                  config = bar2->ENCODING_EGROUP_CTRL_GEN[gbt].ENCODING_EGROUP[grp].
                    ENCODING_EGROUP_CTRL.REVERSE_ELINKS;
                  // Bit 47 (MSB of 12th nibble)
                  if( set == 1 )
                    {
                      config |= ((uint64_t) 1 << p);
                      bar2->ENCODING_EGROUP_CTRL_GEN[gbt].ENCODING_EGROUP[grp].
                        ENCODING_EGROUP_CTRL.REVERSE_ELINKS = config;
                    }
                  else if( set == 0 )
                    {
                      config &= ~((uint64_t) 1 << p);
                      bar2->ENCODING_EGROUP_CTRL_GEN[gbt].ENCODING_EGROUP[grp].
                        ENCODING_EGROUP_CTRL.REVERSE_ELINKS = config;
                    }
#endif // REGMAP_VERSION

                  cout << "GBT " << gbt << " egroup " << grp
                       << " epath " << p << " FH: ";
                  if( config & ((uint64_t) 1 << p) )
                    cout << "ENABLED";
                  else
                    cout << "disabled";
                  cout << endl;
                }
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
  cout << "fereverse version " << hex << VERSION_ID << dec << endl <<
    "Enable, disable or display the bit-order reversal feature for e-links,\n"
    "a setting per e-path (e-link).\n"
    "Without keyword '(re)set' the current setting is displayed.\n\n"
    "Usage: fereverse [-h|V] [-d <devnr>] [-e <elink>]\n"
    "                 [-G <gbt> [-g <group>] [-p <path>]] [-E] [-I] [-f] [-t] "
    "[set|reset]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: all links).\n"
    "  -g <group> : Group number (default: all groups).\n"
    "  -p <path>  : E-path number (default: all paths).\n"
    "  -E         : Display or enable/disable the EC channel 'bit swap'.\n"
    "  -I         : Display or enable/disable the IC channel 'bit swap'.\n"
    "(for options -E/-I use option -G, not -e; options -g/-p are ignored)\n"
    "  -f         : Configure FromHost (FH) only.\n"
    "  -t         : Configure ToHost (TH) only.\n"
    "  set        : Enable e-link bit-reversal.\n"
    "  reset      : Disable e-link bit-reversal.\n";
}

// ----------------------------------------------------------------------------
