#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#if REGMAP_VERSION < 0x500
#include "GbtConfig.h"
#else
#include "GbtConfig5.h"
#endif // REGMAP_VERSION
#include "arg.h"

void listElinks( int gbtnr, int egroupnr, int epathnr );
void readLinkConfig( FlxCard *flx, GbtConfig *gbtConfig );

// Version identifier: year, month, day, release number
// ### More changes needed for RM5: EC, IC and lpGBT link numbers
#if REGMAP_VERSION >= 0x500
const int   VERSION_ID = 0x21021100; // GbtConfig5.h
#else
const int   VERSION_ID = 0x19101400; // Allow E-link nr with offset of n*2048
#endif // REGMAP_VERSION
//const int VERSION_ID = 0x18080700; // Added IC-link 
//const int VERSION_ID = 0x16092800;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int opt;
  int cardnr   = -1;
  int elinknr  = -1;
  int devnr    = -1;
  int gbtnr    = -1;
  int egroupnr = -1;
  int epathnr  = -1;
  int eindex   = -1;
  int ewidth   = -1;
  bool list    = false;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:e:g:G:hI:lp:Vw:")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          if( elinknr != -1 )
            {
              if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
                arg_range( 'e', 0, FLX_MAX_ELINK_NR );
              break;
            }
          break;
        case 'e':
          {
            // E-link number (may have an offset of a multiple of
            // FLX_MAX_ELINK_NR, to indicate the FLX-device number
            // the E-link is assigned to, unless a device number,
            // i.e. 'cardnr', is provided using option -d)
            unsigned int x;
            if( sscanf( optarg, "%x", &x ) != 1 )
              arg_error( 'e' );
            elinknr = x;
            int elinknr_max;
            if( cardnr != -1 )
              elinknr_max = FLX_MAX_ELINK_NR;
            else
              elinknr_max = FLX_MAX_ELINK_NR*4; // Let's allow up to 4 devs
            if( elinknr < 0 || elinknr > elinknr_max )
              arg_range( 'e', 0, elinknr_max );
            // The 'device number' in the E-link number given
            devnr = elinknr / FLX_MAX_ELINK_NR;
            break;
          }
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
          if( egroupnr < 0 || egroupnr >= FLX_TOHOST_GROUPS )
            arg_range( 'g', 0, FLX_TOHOST_GROUPS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'I':
          // Index of e-link in GBT frame
          if( sscanf( optarg, "%d", &eindex ) != 1 )
            arg_error( 'I' );
          if( eindex < 0 || eindex > 5*16-1 )
            arg_range( 'I', 0, 5*16-1 );
          break;
        case 'l':
          list = true;
          break;
        case 'p':
          // E-path number
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'w':
          // E-link width in bits
          if( sscanf( optarg, "%d", &ewidth ) != 1 )
            arg_error( 'w' );
          if( !(ewidth == 2 || ewidth == 4 || ewidth == 8 || ewidth == 16) )
            {
              cout << "### -w: argument not one of [2,4,8,16]" << endl;
              return 1;
            }
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  if( list )
    {
      if( elinknr != -1 )
        {
          // Derive GBT, e-group and e-path numbers from the given e-link number
          gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
          egroupnr = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
          epathnr  = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
        }
      listElinks( gbtnr, egroupnr, epathnr );
      return 0;
    }

  // Check for either a valid -e option or valid -G/g/p or -G/I/w options
  if( (elinknr != -1 && (gbtnr != -1 || egroupnr != -1 || epathnr != -1 ||
                         eindex != -1 || ewidth != -1)) ||
      (elinknr == -1 && gbtnr == -1) ||
      (elinknr == -1 &&
       !(egroupnr != -1 && epathnr != -1) && !(eindex != -1 && ewidth != -1)) )
    {
      cout << "### Provide either -e, -G/g/p or -G/I/w options" << endl;
      return 1;
    }

  if( elinknr != -1 )
    {
      // Derive GBT, e-group and e-path numbers from the given e-link number
      gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
      egroupnr = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
      epathnr  = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
    }
  else if( eindex != -1 )
    {
      egroupnr = eindex / 16;
      epathnr  = GbtConfig::toEpath( eindex, ewidth );
      if( epathnr == -1 )
        {
          cout << "### Invalid -I and/or -w option(s)" << endl;
          return 1;
        }
    }
  elinknr = ((gbtnr << BLOCK_GBT_SHIFT) |
             (egroupnr << BLOCK_EGROUP_SHIFT) |
             (epathnr << BLOCK_EPATH_SHIFT));

  if( !(egroupnr == 7 && (epathnr == 7 || epathnr == 6 ||
                          epathnr == 5 || epathnr == 3)) &&
      (gbtnr >= FLX_LINKS || egroupnr >= 7) )
    {
      cout << "### E-link " << hex << setw(3) << setfill('0') << uppercase
           << elinknr << dec << " is INVALID (";
      if( gbtnr >= FLX_LINKS )
        cout << "GBT=" << gbtnr << ", max=" << FLX_LINKS-1;
      if( egroupnr > 7 )
        cout << " Egroup=" << egroupnr << ", max=" << 6;
      else if( egroupnr == 7 )
        cout << " Egroup=7: path=7,6,5 or 3 only";
      cout << ")" << endl;
      return 1;
    }

  if( devnr > 0 )
    cout << "= ";
  cout << "E-link " << hex << setw(3) << setfill('0') << uppercase
       << elinknr << dec;
  if( devnr > 0 )
    cout << " (FLX-device #" <<devnr << ")";
  cout << " = GBT #" << gbtnr << " group #" << egroupnr
       << " path #" << epathnr;
  switch( epathnr )
    {
    case 0:
      cout << ", bit#" << egroupnr*16 << " width=2|4";
      break;
    case 1:
      cout << ", bit#"  << egroupnr*16+2 << " width=2";
      cout << " OR bit#" << egroupnr*16   << " width=8";
      break;
    case 2:
      cout << ", bit#"  << egroupnr*16+4 << " width=2|4";
      break;
    case 3:
      if( egroupnr == 7 )
        {
          cout << ", TTC-to-Host link";
        }
      else
        {
          cout << ", bit#"  << egroupnr*16+6 << " width=2";
          cout << " OR bit#" << egroupnr*16   << " width=16";
        }
      break;
    case 4:
      cout << ", bit#"  << egroupnr*16+8 << " width=2|4";
      break;
    case 5:
      if( egroupnr == 7 )
        {
          cout << ", AUX-link width=2";
        }
      else
        {
          cout << ", bit#"  << egroupnr*16+10 << " width=2";
          cout << " OR bit#" << egroupnr*16+ 8 << " width=8";
        }
      break;
    case 6:
      if( egroupnr == 7 )
        cout << ", IC-link width=2";
      else
        cout << ", bit#" << egroupnr*16+12 << " width=2|4";
      break;
    case 7:
      if( egroupnr == 7 )
        cout << ", EC-link width=2";
      else
        cout << ", bit#" << egroupnr*16+14 << " width=2";
      break;
    }
  cout << endl;

  // Check for elink on installed FLX-card ?
  if( cardnr < 0 ) return 0;

  if( gbtnr < 0 || gbtnr >= FLX_LINKS )
    {
      cout << "### Invalid GBT number [0.." << FLX_LINKS
           << "] for FLX-device" << endl;
      return 1;
    }

  // Check if the given elink is configured on the given FLX-card (if any)
  cout << "Checking e-link on FLX-device #" << cardnr << "..." << endl;

  // Open FELIX FLX-card
  FlxCard *flx = new FlxCard;
  try {
    flx->card_open( cardnr, LOCK_NONE );
  }
  catch( FlxException &ex ) {
    cout << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }

  // Space to store the card's current E-link configuration
  GbtConfig gbtConfig[FLX_LINKS];

  // Read the card's e-link configuration
  readLinkConfig( flx, gbtConfig );

  int bits;
  cout << "From-GBT/To-Host FLX #" << cardnr;
  if( gbtConfig[gbtnr].isEnabled( egroupnr, epathnr, &bits, false ) )
    cout << ": link enabled, width=" << bits
         << ", mode=" << gbtConfig[gbtnr].mode( egroupnr, epathnr, false )
         << endl;
  else
    cout << ": NO link" << endl;

  cout << "To-GBT/From-Host FLX #" << cardnr;
  if( egroupnr == 7 ) egroupnr = 5; // Hack.. (FromHost has only 5+1 groups)
  if( gbtConfig[gbtnr].isEnabled( egroupnr, epathnr, &bits, true ) )
    cout << ": link enabled, width=" << bits
         << ", mode=" << gbtConfig[gbtnr].mode( egroupnr, epathnr, true )
         << endl;
  else
    cout << ": NO link" << endl;

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

void listElinks( int gbtnr, int egroupnr, int epathnr )
{
  int gbtstart   = 0, gbtend   = FLX_LINKS;
  int groupstart = 0, groupend = FLX_TOHOST_GROUPS;
  int pathstart  = 0, pathend  = FLX_ELINK_PATHS;

  cout << "==> E-link list <==" << endl;

  // List elinks from all or a single GBT
  if( gbtnr >= 0 )
    {
      gbtstart = gbtnr;
      gbtend   = gbtnr + 1;
    }
  // List elinks from all or a single egroup
  if( egroupnr >= 0 )
    {
      groupstart = egroupnr;
      groupend   = egroupnr + 1;
    }
  // List elinks from all or a single epath
  if( epathnr >= 0 )
    {
      pathstart = epathnr;
      pathend   = epathnr + 1;
    }

  //cout << "GBT " << gbtstart << "-" << gbtend << ", "
  //     << "Egroup " << groupstart << "-" << groupend
  //     << "Epath " << pathstart << "-" << pathend << endl;

  cout << setfill('0') << uppercase;

  int elinknr, gbt, egroup, epath;
  for( gbt=gbtstart; gbt<gbtend; ++gbt )
    {
      for( egroup=groupstart; egroup<groupend; ++egroup )
        {
          cout << "GBT-" << gbt
               << " Egroup " << egroup << ":" << endl;
          for( epath=pathstart; epath<pathend; ++epath )
            {
              elinknr = ((gbt << BLOCK_GBT_SHIFT) |
                         (egroup << BLOCK_EGROUP_SHIFT) |
                         (epath << BLOCK_EPATH_SHIFT));
              if( egroup == FLX_TOHOST_GROUPS-1 )
                {
                  if( epath == 7 || epath == 6 || epath == 5 || epath == 3 )
                    cout << " " << hex << setw(3) << elinknr << " = "
                         << dec << gbt << "-" << egroup << "-" << epath;

                  if( epath == 7 )
                    cout << " to/from-host:EC" << endl;
                  else if( epath == 6 )
                    cout << " to/from-host:IC" << endl;
                  else if( epath == 5 )
                    cout << " to/from-host:AUX" << endl;
                  else if( epath == 3 )
                    cout << " to-host:TTC" << endl;
                  //else
                  //cout << " ----" << endl;
                }
              else if( egroup >= FLX_FROMHOST_GROUPS-1 )
                {
                  cout << " " << hex << setw(3) << elinknr << " = "
                       << dec << gbt << "-" << egroup << "-" << epath;

                  cout << " to-host:wide";

                  if( egroup == FLX_FROMHOST_GROUPS-1 && epath == 7 )
                    cout << " from-host:EC";
                  else if( egroup == FLX_FROMHOST_GROUPS-1 && epath == 6 )
                    cout << " from-host:IC";
                  else if( egroup == FLX_FROMHOST_GROUPS-1 && epath == 5 )
                    cout << " from-host:AUX";
                  else
                    cout << " from-host:---";

                  cout << endl;
                }
              else
                {
                  cout << " " << hex << setw(3) << elinknr << " = "
                       << dec << gbt << "-" << egroup << "-" << epath << endl;
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------

void readLinkConfig( FlxCard *flx, GbtConfig *gbtConfig )
{
  if( !flx ) return;

  uint64_t bar2_addr = flx->bar2Address();
  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

  uint64_t chans = bar2->NUM_OF_CHANNELS;
  //if( bar2->CARD_TYPE == 711 || bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS )
    chans = FLX_LINKS;

  uint64_t gbt, egroup;
  uint32_t enables, chunksz;
  uint64_t modes;
  // To Host:
  //for( gbt=0; gbt<chans; ++gbt )
  for( gbt=0; gbt<FLX_LINKS; ++gbt ) // Read all configuration registers
    {
#if REGMAP_VERSION >= 0x500
      if( gbt < 12 )
#endif
      for( egroup=0; egroup<FLX_TOHOST_GROUPS-1; ++egroup )
        {
#if REGMAP_VERSION < 0x500
          flxcard_cr_tohost_egroup_ctrl_t config =
                     bar2->CR_GBT_CTRL[gbt].EGROUP_TOHOST[egroup].TOHOST;
          uint64_t modebits;
          enables  = (uint32_t) config.EPROC_ENA;
          modebits = config.PATH_ENCODING; // 2 bits per E-path!
          chunksz  = (uint32_t) config.MAX_CHUNK_LEN;

          // Translate 'mode' word from register to local format 'modes' word,
          // i.e. 4 bits per E-PROC
          //modes = GbtConfig::epath2BitModeWordToModes( enables, modebits );
          modes = GbtConfig::epath2BitModeWordToModes( modebits );
#else
          flxcard_decoding_egroup_ctrl_t config =
            bar2->DECODING_EGROUP_CTRL_GEN[gbt].DECODING_EGROUP[egroup].EGROUP;
          uint32_t width;
          // There are only 8 enable bits: the width setting determines
          // what the E-links width is (they are the same within an E-group)
          enables = (uint32_t) config.EPATH_ENA;
          width   = (uint32_t) config.EPATH_WIDTH;
          modes   = config.PATH_ENCODING; // 4 bits per E-path!
          chunksz = 0;

          gbtConfig[gbt].setWidthToHost( egroup, width );
#endif // REGMAP_VERSION

          // Store in our local configuration structure
          gbtConfig[gbt].setEnablesToHost( egroup, enables );
          gbtConfig[gbt].setModesToHost( egroup, modes );
          if( egroup == 0 ) gbtConfig[gbt].setMaxChunkWord( chunksz );
        }

      // and the 'EC+IC+AUX+TTC' group
      enables = 0x0000;

#if REGMAP_VERSION < 0x500
      flxcard_ec_tohost_t *ec_tohost =
        &bar2->MINI_EGROUP_CTRL[gbt].EC_TOHOST;

      // To-host EC enabled/disabled
      if( ec_tohost->ENABLE )
        enables = 0x4000;

      // To-host EC encoding
      // translate 'mode' word from register to local format 'modes' word,
      // i.e. 4 bits per E-PROC
      uint64_t modebits;
      modebits = ec_tohost->ENCODING << (7*2);
      modes = GbtConfig::epath2BitModeWordToModes( modebits );

      // To-host IC enabled/disabled
      if( ec_tohost->IC_ENABLE )
        enables |= 0x2000;

      // To-host AUX enabled/disabled
      if( ec_tohost->SCA_AUX_ENABLE )
        enables |= 0x1000;

      // TTC-to-Host enabled/disabled
      if( gbt == 0 && bar2->CR_TTC_TOHOST.ENABLE )
        enables |= 0x0400;
#else
      flxcard_mini_egroup_tohost_t *mini_egroup_tohost =
        &bar2->MINI_EGROUP_TOHOST_GEN[gbt].MINI_EGROUP_TOHOST;

      // To-host EC enabled/disabled
      if( mini_egroup_tohost->EC_ENABLE )
        enables = 0x4000;

      // To-host EC encoding: 4 bits
      modes = (uint64_t) mini_egroup_tohost->EC_ENCODING << (14*4);

      // To-host IC enabled/disabled
      if( mini_egroup_tohost->IC_ENABLE )
        enables |= 0x2000;

      // To-host AUX enabled/disabled
      if( mini_egroup_tohost->AUX_ENABLE )
        enables |= 0x1000;

      // TTC-to-Host enabled/disabled
      if( gbt == 0 && bar2->TTC_TOHOST_ENABLE )
        enables |= 0x0400;
#endif

      // Store in our local configuration structure
      gbtConfig[gbt].setEnablesToHost( FLX_TOHOST_GROUPS-1, enables );
      gbtConfig[gbt].setModesToHost( FLX_TOHOST_GROUPS-1, modes );
    }

  // From Host:
  //for( gbt=0; gbt<chans; ++gbt )
  for( gbt=0; gbt<FLX_LINKS; ++gbt ) // Read all configuration registers
    {
#if REGMAP_VERSION >= 0x500
      if( gbt < 12 )
#endif
      for( egroup=0; egroup<FLX_FROMHOST_GROUPS-1; ++egroup )
        {
#if REGMAP_VERSION < 0x500
          flxcard_cr_fromhost_egroup_ctrl_t config =
                     bar2->CR_GBT_CTRL[gbt].EGROUP_FROMHOST[egroup].FROMHOST;
          enables  = (uint32_t) config.EPROC_ENA;
          uint64_t modebits;
          modebits = config.PATH_ENCODING; // 4 bits per E-path!

          // Translate 'mode' word from register to local format
          // 'modes' word i.e. 4 bits per E-PROC
          //modes = GbtConfig::epath4BitModeWordToModes( enables, modewrd );
          modes = GbtConfig::epath4BitModeWordToModes( modebits );
#else
          flxcard_encoding_egroup_ctrl_t config =
            bar2->ENCODING_EGROUP_CTRL_GEN[gbt].ENCODING_EGROUP[egroup].ENCODING_EGROUP_CTRL;
          uint32_t width;
          enables = (uint32_t) config.EPATH_ENA;
          width   = (uint32_t) config.EPATH_WIDTH;
          modes   = config.PATH_ENCODING; // 4 bits per E-path!

          gbtConfig[gbt].setWidthFromHost( egroup, width );
#endif // REGMAP_VERSION

          // Store in our local configuration structure
          gbtConfig[gbt].setEnablesFromHost( egroup, enables );
          gbtConfig[gbt].setModesFromHost( egroup, modes );
        }

      // and the 'EC+IC+AUX' group
      enables = 0x0000;

#if REGMAP_VERSION < 0x500
      flxcard_ec_fromhost_t *ec_fromhost =
        &bar2->MINI_EGROUP_CTRL[gbt].EC_FROMHOST;

      // From-host EC enabled/disabled
      if( ec_fromhost->ENABLE )
        enables = 0x4000;

      // From-host EC encoding
      uint64_t modebits;
      modebits = ec_fromhost->ENCODING << (7*4);
      modes = GbtConfig::epath4BitModeWordToModes( modebits );

      // From-host IC enabled/disabled
      if( ec_fromhost->IC_ENABLE )
        enables |= 0x2000;

      // From-host AUX enabled/disabled
      if( ec_fromhost->SCA_AUX_ENABLE )
        enables |= 0x1000;
#else
      flxcard_mini_egroup_fromhost_t *mini_egroup_fromhost =
        &bar2->MINI_EGROUP_FROMHOST_GEN[gbt].MINI_EGROUP_FROMHOST;

      // From-host EC enabled/disabled
      if( mini_egroup_fromhost->EC_ENABLE )
        enables = 0x4000;

      // From-host EC encoding: 4 bits
      modes = (uint64_t) mini_egroup_fromhost->EC_ENCODING << (14*4);

      // From-host IC enabled/disabled
      if( mini_egroup_fromhost->IC_ENABLE )
        enables |= 0x2000;

      // From-host AUX enabled/disabled
      if( mini_egroup_fromhost->AUX_ENABLE )
        enables |= 0x1000;
#endif // REGMAP_VERSION

      // Store in our local configuration structure
      gbtConfig[gbt].setEnablesFromHost( FLX_FROMHOST_GROUPS-1, enables );
      gbtConfig[gbt].setModesFromHost( FLX_FROMHOST_GROUPS-1, modes );
    }
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "felink version " << hex << VERSION_ID << dec << endl <<
    "Convert a given E-link number into GBT, egroup and epath numbers\n"
    "as well as GBT and bit-index and width, or viceversa.\n"
    "The E-link number is provided as a (hex) number directly (-e option),\n"
    "as a set of -G/g/p options, or as a set of -G/I/w options.\n"
    "Optionally checks if this E-link is valid and configured "
    "on a given FLX-device (option -d),\n"
    "in either to-host or from-host direction.\n"
    "Use option -l to display a list of valid E-link numbers,\n"
    "optionally in combination with -G or -g options to restrict the output\n"
    "to a particular GBT-link and/or egroup.\n"
    "(Note that E-link numbers are also shown in the elinkconfig GUI).\n\n"

    "Usage: felink [-h|V] [-d <devnr>] (-e <elink>\n"
    "               | (-G <gbt> (-g <group> -p <path>) | "
    "(-I <index> -w <width>))\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p or -G/I/w options.\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number.\n"
    "  -l         : Show a list of valid E-link numbers "
    "(use options -G, -g, -p to restrict the list).\n"
    "  -p <path>  : E-path number.\n"
    "  -I <index> : Index of first bit of e-link in GBT frame.\n"
    "  -w <width> : E-link width in bits (2, 4, 8 or 16).\n";
}

// ----------------------------------------------------------------------------
