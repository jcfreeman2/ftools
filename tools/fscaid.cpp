#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x21110900; // Option -E (ToHost E-link number)
//const int VERSION_ID = 0x18111300;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr       = 0;
  int  gbtnr        = -1;
  int  egroupnr     = 7; // EC-link
  int  epathnr      = 7; // EC-link
  int  elinknr      = -1;
  int  elinknr_recv = -1;
  int  dma_write    = -1;// Autoselect FromHost DMA controller index
  int  dma_read     = 0;
  bool receive      = true;
  bool sca_connect  = false;
  bool sca_reset    = false;
  bool version1     = false;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "1Cd:e:E:G:g:hp:RVZ")) != -1 )
    {
      switch( opt )
        {
        case '1':
          version1 = true;
          break;
        case 'C':
          sca_connect = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'e':
          // E-link number
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elinknr = x;
          if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
            arg_range_hex( 'e', 0, FLX_MAX_ELINK_NR );
          break;
        case 'E':
          // E-link number for receiving (if different from 'elinknr')
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'E' );
          elinknr_recv = x;
          if( elinknr_recv < 0 || elinknr_recv > FLX_MAX_ELINK_NR )
            arg_range_hex( 'E', 0, FLX_MAX_ELINK_NR );
          break;
        case 'G':
          // GBT link number to upload to
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'g':
          // E-group number to upload to
          // (NB: Egroup 7 (or 5) + Epath 7 is EC link)
          if( sscanf( optarg, "%d", &egroupnr ) != 1 )
            arg_error( 'g' );
          if( egroupnr < 0 || (egroupnr > FLX_FROMHOST_GROUPS-1 &&
                               egroupnr != 5 && egroupnr != 7) )
            arg_range( 'g', 0, FLX_FROMHOST_GROUPS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'p':
          // E-path number to upload to
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'R':
          sca_reset = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'Z':
          receive = false;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Check for either a valid -e or valid -G/g/p options
  if( (elinknr != -1 && !(gbtnr == -1 || egroupnr == -1 || epathnr == -1)) ||
      (elinknr == -1 && !(gbtnr != -1 && egroupnr != -1 && epathnr != -1)) )
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
  else
    {
      // Derive e-link number from the given GBT, group and epath numbers
      elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                 (egroupnr << BLOCK_EGROUP_SHIFT) |
                 (epathnr  << BLOCK_EPATH_SHIFT));
   }

  // Default: receiving (ToHost) E-link number identical to sending (FromHost)
  if( elinknr_recv == -1 )
    elinknr_recv = elinknr;

  // -------------------------------------------------------------------------
  // FLX-card sender and receiver

  FlxUpload fup( cardnr, 0, dma_write );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", firmw " << fup.firmwareVersion() << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  cout << "Using e-link: " << hex << uppercase << setfill('0')
       << setw(3) << fup.mapElink( elinknr );
  if( elinknr_recv != elinknr )
    cout << " (FromHost), " << setw(3)
         << fup.mapElink( elinknr_recv ) << " (ToHost)";
  cout << dec << setfill(' ') << endl;

  FlxReceiver *frecv = 0;
  if( receive )
    {
      frecv = new FlxReceiver( cardnr, 64*1024*1024, dma_read );
      if( frecv->hasError() )
        {
          cout << "### " << frecv->errorString() << endl;
          frecv->stop();
          return 1;
        }
    }

  // -------------------------------------------------------------------------
  // Initialize

  if( sca_connect )
    {
      if( fup.scaConnect( elinknr ) )
        cout << "GBT-SCA connect" << endl;
      else
        cout << "###GBT-SCA connect: " << fup.errorString() << endl;
    }

  if( sca_reset )
    {
      if( fup.scaReset( elinknr ) )
        cout << "GBT-SCA reset" << endl;
      else
        cout << "###GBT-SCA reset: " << fup.errorString() << endl;
    }

  // -------------------------------------------------------------------------
  // Read GBT-SCA chip ID

  uint8_t data[4], dev_mask = 0;
  int     trid;

  // Requires ADC device to be enabled...(huh!?)
  // Read current device enabled mask:
  // Do it twice... first one possibly results in SREJ reply...
  trid = 254;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 0,
                      SCA_CONFIG_RD_D, data );
  trid = 254;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 0,
                      SCA_CONFIG_RD_D, data );

  FlxDataChecker flxChecker;
  uint8_t       *chunkdata = 0;
  uint32_t       size;
  flxChecker.setReceiver( frecv );
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr_recv ) )
    {
      if( size == 0 ) continue;
      if( size <= 12 && chunkdata[2] == 0xFE ) // Transaction ID 254
        dev_mask = chunkdata[7];
    }

  // Enable ADC if necessary
  if( (dev_mask & (1 << (SCA_DEV_ADC & 0x7))) == 0 )
    {
      data[0] = 0x00;
      data[1] = dev_mask | (1 << (SCA_DEV_ADC & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_D, data );
    }

  // And NOW: read the Chip ID
  trid = 254;
  if( version1 )
    fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 0,
                        SCA_RD_CHIPID_V1, data );
  else
    fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 0,
                        SCA_RD_CHIPID, data );

  // Disable ADC if necessary
  if( (dev_mask & (1 << (SCA_DEV_ADC & 0x7))) == 0 )
    {
      data[0] = 0x00;
      data[1] = dev_mask;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_D, data );
    }

  // -------------------------------------------------------------------------
  // Display

  cout << "GBT-SCA Chip ID";
  if( version1 ) cout << " (V1)";
  cout << ":" << endl;
  cout << hex << setfill( '0' );

  // ###NB: if 'dummy' not defined something goes wrong reading the ID
  // from an E-link that is not EC-link... (MYSTERIOUS!) To be investigated.
  //FlxDataChecker dummy;
  unsigned int dataword;
  //flxChecker.setReceiver( frecv );
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr_recv ) )
    {
      if( size == 0 ) continue;

      // Find chunks with SCA replies containing
      // our dedicated Transaction ID of 254
      if( size == 12 && chunkdata[2] == 0xFE ) // Transaction ID
        {
          dataword = (((uint32_t) chunkdata[8]) << 0) |
            (((uint32_t) chunkdata[9]) << 8) |
            (((uint32_t) chunkdata[6]) << 16) |
            (((uint32_t) chunkdata[7]) << 24);
          cout << "0x" << setw(8) << dataword;
          cout << endl;
        }
    }

  // -------------------------------------------------------------------------
  // Finish up

  fup.stop();
  if( frecv )
    frecv->stop();
  cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscaid version " << hex << VERSION_ID << dec << endl <<
    "Tool to read a GBT-SCA's Chip ID.\n"
    "Usage:\n"
    " fscaid [-h|V] [-d <devnr>] [-e <elink>] [-E <elinkr>] [-G <gbt>] "
    "[-g <group>] [-p <path>]\n"
    "        [-1] [-C] [-R] [-Z]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -E <elinkr>: E-link number (hex) for receiving, if different from\n"
    "               the E-link number for sending.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -C         : Send GBT-SCA connect (HDLC control).\n"
    "  -R         : Send GBT-SCA reset (HDLC control).\n"
    "  -Z         : Do not receive and display the GBT-SCA replies.\n"
    "  -1         : Read ID from a GBT-SCA Version 1 (default: V2).\n";
}

// ----------------------------------------------------------------------------
