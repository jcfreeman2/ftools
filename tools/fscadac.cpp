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
const int VERSION_ID = 0x18111300;

#define sleep_ms(x) usleep((x)*1000)

void display_replies( FlxReceiver *recvr,
                      int          elinknr,
                      string       title = string() );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr       = 0;
  int  gbtnr        = -1;
  int  egroupnr     = 7; // EC-link
  int  epathnr      = 7; // EC-link
  int  elinknr      = -1;
  int  dma_write    = -1; // Autoselect FromHost DMA controller index
  int  dma_read     = 0;
  bool receive      = true;
  bool sca_connect  = false;
  bool sca_reset    = false;
  bool dac_enable   = true;
  bool dac_disable  = false;
  bool dac_sweep    = false;
  int  dac_index    = -1;
  int  dac_val      = -1;
  int  dac_sweep_ms = 1000;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "Cd:DEe:G:g:hi:p:Rst:VZ")) != -1 )
    {
      switch( opt )
        {
        case 'C':
          sca_connect = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          dac_disable = true;
          break;
        case 'E':
          dac_enable = false;
          break;
        case 'e':
          // E-link number
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elinknr = x;
          if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
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
        case 'i':
          // DAC index
          if( sscanf( optarg, "%d", &dac_index ) != 1 )
            arg_error( 'i' );
          if( dac_index < 0 || dac_index > 3 )
            arg_range( 'i', 0, 3 );
          break;
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
        case 's':
          dac_sweep = true;
          break;
        case 't':
          // DAC sweep time in milliseconds
          if( sscanf( optarg, "%d", &dac_sweep_ms ) != 1 )
            arg_error( 't' );
          if( dac_sweep_ms < 1 || dac_sweep_ms > 10000 )
            arg_range( 't', 1, 10000 );
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

  // DAC value to write
  if( optind < argc )
    {
      // Expect a byte value to write
      if( sscanf( argv[optind], "%d", &dac_val ) == 1 )
        {
          if( dac_val < 0 || dac_val > 255 )
            {
              cout << "### DAC byte value out-of-range 0..255: "
                   << dac_val << endl;
              return 1;
            }
        }
      else
        {
          cout << "### Invalid DAC value provided" << endl;
          return 1;
        }
    }

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
      /*
      cout << "Opened FLX-device " << cardnr
           << ", firmw " << frecv->firmwareVersion()
           << ", " << frecv->numberOfChans()
           <<  " chans (cmem bufsize="
           << frecv->bufferSize() << ", receive: "
           << "DMA #" << dma_read << ")" << endl;
      */
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
  // Configure

  uint8_t data[4], dev_mask = 0;
  int     trid = 1;
  int     dac_min, dac_max;
  const int DAC_WR_CMD[] = {
    SCA_DAC_A_WR, SCA_DAC_B_WR, SCA_DAC_C_WR, SCA_DAC_D_WR
  };
  const int DAC_RD_CMD[] = {
    SCA_DAC_A_RD, SCA_DAC_B_RD, SCA_DAC_C_RD, SCA_DAC_D_RD
  };

  if( dac_enable || dac_disable )
    {
      // Do it twice... first one possibly results in SREJ reply...
      int recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_D, data );
      recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_D, data );

      FlxDataChecker flxChecker;
      uint8_t       *chunkdata = 0;
      uint32_t       size;
      flxChecker.setReceiver( frecv );
      while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
        {
          if( size == 0 ) continue;
          if( size <= 12 && chunkdata[2] == 0xFE ) // Transaction ID 254
            dev_mask = chunkdata[7];
        }

      //cout << "ENA=" << hex << (uint32_t) dev_mask << dec << endl;
    }

  if( dac_enable )
    {
      // Enable DAC
      data[0] = 0x00;
      data[1] = dev_mask | (1 << (SCA_DEV_DAC & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_D, data );
      cout << "DAC enabled" << endl;
    }

  // -------------------------------------------------------------------------
  // Write/Read

  if( dac_index == -1 )
    {
      dac_min = 0;
      dac_max = 4;
    }
  else
    {
      dac_min = dac_index;
      dac_max = dac_index + 1;
    }

  int dac, recv_trid;
  data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0;
  if( !dac_sweep )
    {
      for( dac=dac_min; dac<dac_max; ++dac )
        {
          // Write?
          if( dac_val >= 0 )
            {
              data[1] = dac_val;
              fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC,
                                  4, DAC_WR_CMD[dac], data );
            }
        }
      for( dac=0; dac<4; ++dac )
        {
          // Read
          data[1] = 0;
          recv_trid = 254;
          fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_DAC,
                              1, DAC_RD_CMD[dac], data );
        }
    }
  else
    {
      cout << "DAC:     "; 
      // Sweep DAC value from 0 to 255 then finally back to 0
      int pause = (dac_sweep_ms * 1000) / 255;
      for( dac_val=0; dac_val<256; ++dac_val )
        {
          for( dac=dac_min; dac<dac_max; ++dac )
            {
              // Write
              data[1] = dac_val;
              fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC,
                                  4, DAC_WR_CMD[dac], data );
              // Read
              //data[1] = 0;
              //recv_trid = 254;
              //fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_DAC,
              //                    1, DAC_RD_CMD[dac], data );
            }
          usleep( pause );
          cout << "\b\b\b\b   \b\b\b" << setw(3) << dac_val << " ";
          cout.flush();
        }
      cout << endl;
      // Read final DAC value(s)
      data[1] = 0;
      for( dac=0; dac<4; ++dac )
        {
          recv_trid = 254;
          fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_DAC,
                              1, DAC_RD_CMD[dac], data );
        }
      // Write DAC val 0
      for( dac=dac_min; dac<dac_max; ++dac )
        {
          data[1] = 0;
          fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC,
                              4, DAC_WR_CMD[dac], data );
        }
    }

  // -------------------------------------------------------------------------
  // Display

  // DEBUG
  //display_replies( frecv, elinknr, "GBT-SCA DAC readings" );

  cout << "GBT-SCA DAC readings:" << endl;

  FlxDataChecker flxChecker;
  uint8_t       *chunkdata = 0;
  uint32_t       size;
  int            cnt = 0;
  unsigned int   dataword;
  flxChecker.setReceiver( frecv );
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
    {
      if( size == 0 ) continue;

      // Find chunks with SCA replies containing
      // our dedicated Transaction ID of 254
      if( size == 12 && chunkdata[2] == 0xFE ) // Transaction ID
        {
          //dataword = (((uint32_t) chunkdata[8]) << 0) |
          //  (((uint32_t) chunkdata[9]) << 8) |
          //  (((uint32_t) chunkdata[6]) << 16) |
          //  (((uint32_t) chunkdata[7]) << 24);
          // For DAC
          dataword = (uint32_t) chunkdata[7];
          cout << setw(3) << cnt << ": "
               << setw(3) << dataword;
          cout << endl;
          ++cnt;
        }
    }

  // -------------------------------------------------------------------------
  // Disable

  if( dac_disable )
    {
      data[0] = 0x00;
      data[1] = dev_mask;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_D, data );
      cout << "DAC disabled" << endl;
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
  cout << "fscadac version " << hex << VERSION_ID << dec << endl <<
    "Tool to set and/or read back GBT-SCA DAC outputs.\n"
    "In addition it allows a sweep through the DAC range for one or all DAC "
    "outputs\n"
    "within a configurable time period.\n"
    "Usage:\n"
    " fscadac [-h|V] [-d <devnr>] [-e <elink>] [-G <gbt>] [-g <group>] "
    "[-p <path>]\n"
    "         [-i <index>] [-s] [-t <ms>] [-C] [-R] [-D] [-E] [-Z]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -i <index> : DAC index (0=DAC_A,1=DAC_B,2=DAC_C,3=DAC_D) to use "
      "(default: all).\n"
    "  -s         : Sweep DAC value for the given DAC output(s).\n"
    "  -t <ms>    : Sweep time from DAC value 0 to 255, in milliseconds,\n"
    "               when option -s given (default: 1000).\n"
    "  -C         : Send GBT-SCA connect (HDLC control).\n"
    "  -R         : Send GBT-SCA reset (HDLC control).\n"
    "  -Z         : Do not receive and display the GBT-SCA replies.\n"
    "  -D         : Disable GBT-SCA DAC after operation "
    "(default: leave enabled)\n"
    "  -E         : Do *not* enable GBT-SCA DAC at start of operation, "
    "assume it already is.\n";
}

// ----------------------------------------------------------------------------

void display_replies( FlxReceiver *recvr,
                      int          elinknr,
                      string       title )
{
  if( recvr == 0 ) return;

  // Optionally have a title for the GBT-SCA replies displayed
  cout << endl;
  if( title.length() )
    cout << ">> " << title << " <<" << endl;

  // Go through the chunks of each block (skipping any TIMEOUT/NULL/OOB chunks)
  // displaying the data bytes, interpreting chunks as GBT-SCA replies
  FlxDataChecker flxChecker;
  int  timeout_us  = 5000;
  bool display_sca = true;
  flxChecker.displayDataRecvd( recvr, timeout_us, elinknr, display_sca );
}

// ----------------------------------------------------------------------------
