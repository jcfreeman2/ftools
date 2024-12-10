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
const int   VERSION_ID = 0x22011700; // Fix bug in single lpGBT E-link request
//const int VERSION_ID = 0x21120200;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr    = 0;
  int  gbtnr     = -1;
  int  egroupnr  = -1;
  int  epathnr   = -1;
  int  elinknr   = -1;
  int  dma_index = -1;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:e:g:G:hp:V")) != -1 )
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

  // DMA index number to set, if any
  if( optind < argc )
    {
      if( sscanf( argv[optind], "%d", &dma_index ) == 1 )
        {
          if( dma_index < 0 || dma_index > 7 )
            {
              cout << "### DMA index outside max range [0..7]" << endl;
              return 1;
            }
        }
      else
        {
          cout << "### Invalid DMA index provided" << endl;
          return 1;
        }
    }

#if REGMAP_VERSION < 0x500
  cout << "### fedma tool is for RM5 firmware only" << endl;
  return 1;
#endif // REGMAP_VERSION

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

  // Check if given DMA index is within range as set by the firmware
  if( dma_index != -1 )
    {
      uint64_t descriptors = bar2->GENERIC_CONSTANTS.DESCRIPTORS;
      if( dma_index >= (int) descriptors-1 )
        {
          cout << "### DMA index given not in range [0.." << descriptors-2
               << "] for this FLX-device" << endl;
          flx->card_close();
          return 1;
        }
      cout << "Set DMA to: " << dma_index << endl;
    }

  uint64_t fw_mode = bar2->FIRMWARE_MODE;
  uint64_t chans = bar2->NUM_OF_CHANNELS;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  // Single E-link number given ?
  if( elinknr != -1 )
    {
      // Derive GBT, e-group and e-path numbers from the given e-link number
      if( fw_mode == FIRMW_PIXEL ||
          fw_mode == FIRMW_STRIP ||
          fw_mode == FIRMW_LPGBT )
        {
          gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
          egroupnr = (elinknr & (BLOCK_EGROUP_MASK>>1))>>(BLOCK_EGROUP_SHIFT-1);
          epathnr  = (elinknr & (BLOCK_EPATH_MASK>>1)) >> BLOCK_EPATH_SHIFT;
        }
      else
        {
          gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
          egroupnr = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
          epathnr  = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
        }
    }

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
      if( fw_mode == FIRMW_PIXEL ||
          fw_mode == FIRMW_STRIP ||
          fw_mode == FIRMW_LPGBT )
        max_egroupnr = 7;
      else
        max_egroupnr = 5;
    }
  else
    {
      max_egroupnr = egroupnr + 1;
    }

  int max_epathnr;
  if( epathnr == -1 )
    {
      epathnr = 0;
      if( fw_mode == FIRMW_PIXEL ||
          fw_mode == FIRMW_STRIP ||
          fw_mode == FIRMW_LPGBT )
        max_epathnr = 4;
      else
        max_epathnr = 8;
    }
  else
    {
      max_epathnr = epathnr + 1;
    }

  cout << "E-link G-g-p DMA" << endl << setfill('0');

  uint64_t dma_i = 0;
  for( int gbt=gbtnr; gbt<max_gbtnr; ++gbt )
    for( int grp=egroupnr; grp<max_egroupnr; ++grp )
      for( int path=epathnr; path<max_epathnr; ++path )
        {
          if( fw_mode == FIRMW_PIXEL ||
              fw_mode == FIRMW_STRIP ||
              fw_mode == FIRMW_LPGBT )
            elinknr = ((gbt  << BLOCK_GBT_SHIFT) |
                       (grp  << (BLOCK_EGROUP_SHIFT-1)) | // Epath: 1 bit less 
                       (path << BLOCK_EPATH_SHIFT));
          else
            elinknr = ((gbt  << BLOCK_GBT_SHIFT) |
                       (grp  << BLOCK_EGROUP_SHIFT) |
                       (path << BLOCK_EPATH_SHIFT));

#if REGMAP_VERSION >= 0x500
          // Set 'address' (and read current DMA index setting)
          bar2->CRTOHOST_DMA_DESCRIPTOR_2.AXIS_ID = elinknr;

          // Write new DMA index setting?
          if( dma_index != -1 )
            {
              bar2->CRTOHOST_DMA_DESCRIPTOR_1.DESCR = dma_index;
              // Re-read
              bar2->CRTOHOST_DMA_DESCRIPTOR_2.AXIS_ID = elinknr;
            }

          // Read DMA index
          dma_i = bar2->CRTOHOST_DMA_DESCRIPTOR_2.DESCR_READ;
#endif // REGMAP_VERSION

          cout << "0x" << setfill('0') << setw(3) << hex << elinknr << " "
               << dec << setfill(' ') << setw(2) << gbt << "-"
               << grp << "-" << path << "  " << dma_i << endl;
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
  cout << "fedma version " << hex << VERSION_ID << dec << endl <<
    "NOTE: for RM5 firmware only.\n"
    "Set and/or read the DMA controller index for (ToHost) e-links,\n"
    "a setting per e-path (i.e. e-link).\n"
    "Without a DMA index provided the current setting(s) of the selected "
    "e-link(s)\nis (are) displayed.\n\n"
    "Usage: fedma [-h|V] [-d <devnr>] [-e <elink>]\n"
    "             [-G <gbt> [-g <group>] [-p <path>]] [<dma>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: all links).\n"
    "  -g <group> : Group number (default: all groups).\n"
    "  -p <path>  : E-path number (default: all paths).\n"
    "  <dma>      : DMA index to set for the selected E-link(s).\n";
}

// ----------------------------------------------------------------------------
