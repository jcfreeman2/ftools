#include <fstream>
#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "I2cGpio.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x19101600; // Optionally display sequence to be sent
//const int VERSION_ID = 0x18040900;

#define sleep_ms(x) usleep((x)*1000)

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr      = 0;
  int  dma_write   = -1;// Autoselect FromHost DMA controller index
  int  dma_read    = 0;
  int  gbtnr       = -1;
  int  egroupnr    = 7; // EC-link
  int  epathnr     = 7; // EC-link
  int  elinknr     = -1;
  bool use_intrpt  = false;
  bool debug       = false;
  int  i2c_rate    = 1; // In 100KHz
  int  scl         = -1;
  int  sda         = -1;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "a:c:d:De:G:g:hp:R:V")) != -1 )
    {
      switch( opt )
        {
        case 'a':
          if( sscanf( optarg, "%d", &sda ) != 1 )
            arg_error( 'a' );
          if( sda < 0 || sda > 31 )
            arg_range( 'a', 0, 31 );
          if( scl == sda )
            {
              cout << "### -a: SCL and SDA must differ" << endl;
              return 1;
            }
          break;
        case 'c':
          if( sscanf( optarg, "%d", &scl ) != 1 )
            arg_error( 'c' );
          if( scl < 0 || scl > 31 )
            arg_range( 'c', 0, 31 );
          if( scl == sda )
            {
              cout << "### -c: SCL and SDA must differ" << endl;
              return 1;
            }
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          debug = true;
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
          // GBT link number
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'g':
          // E-group number
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
          // E-path number
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'R':
          // I2C data transfer rate (in multiples of 100KHz)
          if( sscanf( optarg, "%d", &i2c_rate ) != 1 )
            arg_error( 'R' );
          if( !(i2c_rate == 1 || i2c_rate == 2) )
            {
              cout << "### -R: argument not one of [1,2]*100KHz" << endl;
              return 1;
            }
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

  // Uploads should not get split across DMAs: causes unwanted delays
  fup.setDmaSize( 8192 );

  FlxReceiver frecv( cardnr, 512*1024*1024, dma_read );
  if( frecv.hasError() )
    {
      cout << "### " << frecv.errorString() << endl;
      frecv.stop();
      return 1;
    }
  frecv.setUseInterrupt( use_intrpt );

  // -------------------------------------------------------------------------
  // Enable and configure an I2C channel on GBT-SCA GPIO

  I2cGpio iic( fup, frecv, elinknr, scl, sda, i2c_rate*100 );
  iic.init();
  if( debug )
    iic.displayReplies( "Init" );

  // -------------------------------------------------------------------------
  // Perform I2C operations...

  // E.g. reading register 0x00 from device address (0xF0>>1)
  iic.i2cStart();
  iic.i2cWrite( 0xF0 | I2C_WRITE );
  iic.i2cWrite( 0x00 );
  iic.i2cStop();
  iic.i2cStart();
  iic.i2cWrite( 0xF0 | I2C_READ );
  iic.i2cRead( true );
  iic.i2cStop();

  // Send the sequence of GBT-SCA commands
  int sz = iic.execute( debug );
  cout << "(DMA of " << sz << " bytes)" << endl;

  if( !debug )
    {
      // Receive GBT-SCA replies,
      // while extracting (I2C) data and acknowledge bits
      u8 *ack, *data;
      int nack, nbits;
      iic.getBits( &data, &nbits, &ack, &nack );

      cout << "nack=" << nack << endl;
      for( int i=0; i<nack; ++i )
        if( (ack[i/8] & (1<<(i&7))) != 0 )
          cout << " 1";
        else
          cout << " 0";
      cout << endl;

      cout << "nbits=" << nbits << endl;
      cout << hex << setfill( '0' );
      for( int i=0; i<(nbits+7)/8; ++i )
        cout << " " << (uint32_t) data[i];
      cout << endl;
      cout << dec << setfill( ' ' );

      iic.clearBits();
    }
  else
    {
      // Just display what we receive from the GBT-SCA
      // without trying to match this to the I2C operation
      iic.displayReplies( "Operation" );
    }

  // -------------------------------------------------------------------------
  // Finish up

  fup.stop();
  frecv.stop();
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscai2c-io version " << hex << VERSION_ID << dec << endl <<
    "*** DEMO ONLY (a single I2C read operation) ***\n"
    "Tool to operate an I2C bus by bit-banging two GPIO lines of a GBT-SCA,\n"
    "connected to any FLX-device GBT E-link (2-bit HDLC).\n"
    "Usage:\n"
    " fscai2c-io [-h|V] [-D] [-d <devnr>] [-e <elink>] "
    "[-G <gbt> [-g <group> -p <path>]]\n"
    "           [-b] [-R <rate>] -a <sda> -c <scl>\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -a <sda>   : GPIO line used for SDA.\n"
    "  -c <scl>   : GPIO line used for SCL.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -D         : Enable debug mode.\n"
    "  -e <elink> : E-link number (hex), or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number (default: 7=EC-channel).\n"
    "  -p <path>  : E-path number (default: 7=EC-channel).\n"
    "  -R <rate>  : I2C speed in [100KHz]: 1 or 2 (default: 1).\n";
}

// ----------------------------------------------------------------------------
