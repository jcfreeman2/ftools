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
const int VERSION_ID = 0x21011800;

uint32_t chunk2val( uint8_t *chunk );
void val2bytes( uint32_t val, uint8_t *bytes );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int   opt;
  int   cardnr       = 0;
  int   gbtnr        = -1;
  int   egroupnr     = 7; // EC-link
  int   epathnr      = 7; // EC-link
  int   elinknr      = -1;
  int   dma_write    = -1; // Autoselect FromHost DMA controller index
  int   dma_read     = 0;
  bool  receive      = true;
  bool  sca_connect  = false;
  bool  sca_reset    = false;
  bool  gpio_enable  = true;
  bool  gpio_disable = false;
  int   gpio_bit     = -1;
  int64_t gpio_val   = -1;
  int64_t gpio_setdir= -1;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "Cd:DEe:G:g:hi:o:p:RVZ")) != -1 )
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
          gpio_disable = true;
          break;
        case 'E':
          gpio_enable = false;
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
          // GPIO bit index
          if( sscanf( optarg, "%d", &gpio_bit ) != 1 )
            arg_error( 'i' );
          if( gpio_bit < 0 || gpio_bit > 31 )
            arg_range( 'i', 0, 31 );
          break;
        case 'o':
          // GPIO DIR register value
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'o' );
          gpio_setdir = x;
          if( gpio_setdir < 0 || gpio_setdir > 0xFFFFFFFF )
            arg_range( 'o', 0, 0xFFFFFFFF );
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

  // GPIO value to write
  if( optind < argc )
    {
      // Expect a value to write
      if( sscanf( argv[optind], "%x", &x ) == 1 )
        {
          gpio_val = x;
          if( gpio_bit != -1 )
            {
              // Single bit value
              if( gpio_val != 0 && gpio_val != 1 )
                {
                  cout << "### GPIO value out-of-range 0..1: "
                       << gpio_val << endl;
                }
            }
          else
            {
              // 32-bit value
              if( gpio_val < 0 || gpio_val > 0xFFFFFFFF )
                {
                  cout << "### GPIO value out-of-range 0..0xFFFFFFFF: "
                       << gpio_val << endl;
                  return 1;
                }
            }
        }
      else
        {
          cout << "### Invalid GPIO value provided" << endl;
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

  FlxDataChecker flxChecker;
  uint8_t       *chunkdata = 0;
  uint32_t       size;
  flxChecker.setReceiver( frecv );

  // -------------------------------------------------------------------------
  // Initialize GBT-SCA

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
  // Enable GPIO channel

  uint8_t data[4], dev_mask = 0;
  int     trid = 1;

  if( gpio_enable || gpio_disable )
    {
      // Read twice... first one possibly results in SREJ reply...
      int recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_B, data );
      recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_B, data );

      // Receive reply
      while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
        {
          if( size == 0 ) continue;
          if( size <= 12 && chunkdata[2] == 0xFE ) // Transaction ID 254
            dev_mask = chunkdata[7];
        }
      //cout << "ENA=" << hex << (uint32_t) dev_mask << dec << endl;
    }

  if( gpio_enable )
    {
      // Enable GPIO
      data[0] = 0x00;
      data[1] = 1 << SCA_DEV_GPIO;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_B, data );
      cout << "GPIO enabled" << endl;
    }

  // -------------------------------------------------------------------------
  // Read/write GPIO direction

  // Write DIR register
  if( gpio_setdir != -1 )
    {
      uint32_t tmp = (uint64_t) (gpio_setdir & 0xFFFFFFFF);
      val2bytes( tmp, data );
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                          SCA_GPIO_WR_DIR, data );
    }

  // Read DIR register
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_RD_DIR, data );
  // Receive reply
  int64_t gpio_dir = -1;
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
    {
      if( size == 12 && chunkdata[2] == trid-1 )
        gpio_dir = (int64_t) chunk2val( chunkdata );
    }
  if( gpio_dir == -1 )
    {
      cout << "### Failed to receive GBT-SCA reply (read DIR)" << endl;
      return 1;
    }

  if( gpio_bit != -1 && gpio_val != -1 )
    {
      // Set the *selected* GPIO bit to output, if not already
      if( (gpio_dir & (1 << gpio_bit)) == 0 )
        {
          // Write DIR
          gpio_dir |= (1 << gpio_bit);
          uint32_t tmp = (uint64_t) (gpio_dir & 0xFFFFFFFF);
          val2bytes( tmp, data );
          fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                              SCA_GPIO_WR_DIR, data );
        }
    }

  cout << "GPIO dir = 0x" << hex << setfill('0')
       << setw(8) << gpio_dir << endl;

  // -------------------------------------------------------------------------
  // Read current DATAOUT

  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_RD_OUT, data );
  // Receive reply
  int64_t gpio_out = -1;
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
    {
      if( size == 12 && chunkdata[2] == trid-1 )
        gpio_out = (int64_t) chunk2val( chunkdata );
    }
  if( gpio_out == -1 )
    {
      cout << "### Failed to receive GBT-SCA reply (read OUT)" << endl;
      return 1;
    }

  // -------------------------------------------------------------------------
  // Write selected GPIO output or all

  if( gpio_val != -1 )
    {
      if( gpio_bit != -1 )
        {
          // Single bit
          if( gpio_val == 1 )
            gpio_out |= (1 << gpio_bit);
          else
            gpio_out &= ~(1 << gpio_bit);
        }
      else
        {
          // 32 bits
          gpio_out = gpio_val;
        }
      uint32_t tmp = (uint64_t) (gpio_out & 0xFFFFFFFF);
      val2bytes( tmp, data );
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                          SCA_GPIO_WR_OUT, data );

      // Re-read DATAOUT
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                          SCA_GPIO_RD_OUT, data );
      // Receive reply
      gpio_out = -1;
      while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
        {
          if( size == 12 && chunkdata[2] == trid-1 )
            gpio_out = (int64_t) chunk2val( chunkdata );
        }
      if( gpio_out == -1 )
        {
          cout << "### Failed to receive GBT-SCA reply (read OUT)" << endl;
          return 1;
        }
    }

  // -------------------------------------------------------------------------
  // Read current DATAIN

  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_RD_IN, data );
  // Receive reply
  int64_t gpio_in = -1;
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
    {
      if( size == 12 && chunkdata[2] == trid-1 )
        gpio_in = (int64_t) chunk2val( chunkdata );
    }
  if( gpio_in == -1 )
    {
      cout << "### Failed to receive GBT-SCA reply (read IN)" << endl;
      return 1;
    }

  // -------------------------------------------------------------------------
  // Display

  cout << hex << setfill('0')
       << "GPIO out = 0x" << setw(8) << gpio_out
       << " in = 0x" << setw(8) << gpio_in << endl;

  if( gpio_bit != -1 )
    {
      cout << "Bit " << gpio_bit << ": ";
      if( (gpio_dir & (1 << gpio_bit)) != 0 )
        {
          // It's an output
          cout << ((gpio_out & (1 << gpio_bit)) ? "1" : "0");
          cout << " (OUT)";
        }
      else
        {
          // It's an input
          cout << ((gpio_in & (1 << gpio_bit)) ? "1" : "0");
          cout << " (IN)";
        }
      cout << endl;
    }

  // -------------------------------------------------------------------------
  // Disable

  if( gpio_disable )
    {
      data[0] = 0x00;
      data[1] = dev_mask;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_B, data );
      cout << "GPIO disabled" << endl;
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
  cout << "fscaio version " << hex << VERSION_ID << dec << endl <<
    "Tool to write and/or read the GBT-SCA GPIO bits and direction register.\n"
    "Usage:\n"
    " fscaio [-h|V] [-d <devnr>] [-e <elink>] [-G <gbt>] [-g <group>] "
    "[-p <path>]\n"
    "        [-i <bit>] [-o <dir>] [-C] [-R] [-D] [-E] [-Z] [<value>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -i <bit>   : Read or write GPIO bit number <bit> (default: all).\n"
    "               NB: if a single I/O pin is written to, its direction bit\n"
    "                   is set to output (independent of option -o).\n"
    "  -o <dir>   : Set GPIO direction register to value <dir> (hex).\n"
    "  -C         : Send GBT-SCA connect (HDLC control).\n"
    "  -R         : Send GBT-SCA reset (HDLC control).\n"
    "  -Z         : Do not receive and display the GBT-SCA replies.\n"
    "  -D         : Disable GBT-SCA GPIO channel after operation "
    "(default: leave enabled)\n"
    "  -E         : Do *not* enable GBT-SCA GPIO channel at start of operation,"
    " assume it already is.\n"
    " <value>     : Value to write (0 or 1 for a single GPIO bit, or up to "
    "0xFFFFFFFF otherwise, hexadecimal);\n"
    "               if no value is provided a read operation is performed.\n";
}

// ----------------------------------------------------------------------------

uint32_t chunk2val( uint8_t *chunk )
{
  uint32_t val = ((((uint32_t) chunk[8]) << 0) |
                  (((uint32_t) chunk[9]) << 8) |
                  (((uint32_t) chunk[6]) << 16) |
                  (((uint32_t) chunk[7]) << 24));
  return val;
}

// ----------------------------------------------------------------------------

void val2bytes( uint32_t val, uint8_t *bytes )
{
  bytes[0] = (uint8_t) ((val & 0x00FF0000) >> 16);
  bytes[1] = (uint8_t) ((val & 0xFF000000) >> 24);
  bytes[2] = (uint8_t) ((val & 0x000000FF) >>  0);
  bytes[3] = (uint8_t) ((val & 0x0000FF00) >>  8);
}

// ----------------------------------------------------------------------------
