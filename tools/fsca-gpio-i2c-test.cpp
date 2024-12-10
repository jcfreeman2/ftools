#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>
#include <math.h>

#include "arg.h"
#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "crc.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x19082200;

// ----------------------------------------------------------------------------

static int zeroReplies    = 0;
static int largeReplies   = 0;
static int recvReplies    = 0;
static int scaCmdErrors   = 0;
static int scaRejErrors   = 0;
static int scaCrcErrors   = 0;

void getReplies( FlxDataChecker *checker, int elinknr )
{
  uint8_t *chunkdata = 0;
  uint32_t size;
  bool     err_occurred = false;
  checker->startRecvTime(); // Reset time-out on receiving data non-stop
  //while( checker->nextChunkRecvd( &chunkdata, &size, 100000, elinknr ) )
  while( checker->nextChunkRecvd( &chunkdata, &size, 200000, elinknr ) )
    {
      if( size == 0 )
        {
          ++zeroReplies;
          continue;
        }
      ++recvReplies;
      if( size > 12 )
        {
          ++largeReplies;
          continue;
        }

      // GBT-SCA reply parameters
      uint32_t ctrl = (uint32_t) chunkdata[1];
      //uint32_t trid = (uint32_t) chunkdata[2];
      //uint32_t chan = (uint32_t) chunkdata[3];
      uint32_t err  = (uint32_t) chunkdata[4];
      //uint32_t len  = (uint32_t) chunkdata[5];
      //uint32_t crc_lo = (uint32_t) chunkdata[size-1];
      //chunkdata[size-1] &= ~1; // ###DEBUG: test CRC error

      // Check the reply for various errors
      if( ctrl & HDLC_CTRL_SFORMAT )
        {
          uint32_t type = ctrl & HDLC_CTRL_TYPE_MASK;
          if( type == HDLC_CTRL_REJECT || type == HDLC_CTRL_SELREJECT )
            {
              ++scaRejErrors;
              err_occurred = true;
            }
        }
      else if( err != 0 )
        {
          ++scaCmdErrors;
          err_occurred = true;
        }

      if( crc16( chunkdata, size ) != (uint16_t) 0 )
        {
          //cout << "CRC-ERR, size=" << size << ", " << hex;
          //for( uint32_t i=0; i<size; ++i )
          //  cout << " " << setw(2) << (uint32_t) chunkdata[i];
          //cout << dec << endl;
          ++scaCrcErrors;
          err_occurred = true;
        }
      //if( (crc_lo & 0x1F) == 0x1F )
      //  cout << recvReplies << ": crc_lo==" << hex << crc_lo << dec << endl;
    }

  cout << "Replies " << recvReplies;
  if( largeReplies > 0 )
    cout << " (Large " << largeReplies << ")";
  if( zeroReplies > 0 )
    cout << " Zero " << zeroReplies;
  if( err_occurred )
    cout << " (Errors: CRC=" << scaCrcErrors
         << " CMD=" << scaCmdErrors
         << " (S)REJ=" << scaRejErrors << ")";
  cout << endl;
}

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr       = 0;
  int  gbtnr        = -1;
  int  egroupnr     = 7; // EC-link
  int  epathnr      = 7; // EC-link
  int  elinknr      = -1;
  int  dma_write    = -1;// Autoselect FromHost DMA controller index
  int  dma_read     = 0;
  bool dma_circular = false;
  bool receive      = true;
  bool sca_connect  = false;
  bool sca_reset    = false;
  bool gpio_enable  = true;
  bool gpio_disable = false;
  int  sca_delay    = 0; // Minimum ca. 120-140, to prevent BUSY replies below
  int  group_sz     = 1000;
  int  repeat_cnt   = 1000;
  int  exp_cnt      = 0;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "Cd:DEe:G:g:hp:P:r:s:RVXZ")) != -1 )
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
        case 'p':
          // E-path number to upload to
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'P':
          // A pause between certain commands (see below)
          if( sscanf( optarg, "%d", &sca_delay ) != 1 )
            arg_error( 'P' );
          if( sca_delay < 0 )
            arg_range( 'P', 0, 99999 );
          break;
        case 'r':
          if( sscanf( optarg, "%d", &repeat_cnt ) != 1 )
            arg_error( 'r' );
          if( repeat_cnt < 1 )
            arg_range( 'r', 1, 0x7FFFFFFF );
          break;
        case 's':
          if( sscanf( optarg, "%d", &group_sz ) != 1 )
            arg_error( 's' );
          if( group_sz < 1 || group_sz > 1000000 )
            arg_range( 's', 1, 1000000 );
          break;
        case 'R':
          sca_reset = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'X':
          dma_circular = true;
          break;
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

  // -------------------------------------------------------------------------
  // FLX-card sender and receiver

  FlxUpload fup( cardnr, 256*1024*1024, dma_write );
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

  fup.setDmaSize( 8*1024 );

  fup.setDmaCircular( dma_circular );

  FlxReceiver *frecv = 0;
  if( receive )
    {
      frecv = new FlxReceiver( cardnr, 256*1024*1024, dma_read );
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

  FlxDataChecker checker;
  checker.setReceiver( frecv );

  // -------------------------------------------------------------------------
  // Initialize

  if( sca_connect )
    {
      if( fup.scaConnect( elinknr ) )
        cout << "GBT-SCA connect" << endl;
      else
        cout << "###GBT-SCA connect: " << fup.errorString() << endl;
      ++exp_cnt;
    }

  if( sca_reset )
    {
      if( fup.scaReset( elinknr ) )
        cout << "GBT-SCA reset" << endl;
      else
        cout << "###GBT-SCA reset: " << fup.errorString() << endl;
      ++exp_cnt;
    }

  // -------------------------------------------------------------------------
  // Configure

  uint8_t data[4], dev_mask = 0;
  int     trid = 1;

  if( gpio_enable || gpio_disable )
    {
      // Do it twice... first one likely results in an SREJ reply...
      //int recv_trid = 254;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_B, data );
      //fup.addScaFrame( &trid, SCA_DEV_CONFIG, 0,
      //                 SCA_CONFIG_RD_B, data );
      //recv_trid = 254;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_B, data );
      //fup.addScaFrame( &trid, SCA_DEV_CONFIG, 0,
      //                 SCA_CONFIG_RD_B, data );
      //recv_trid = 254;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_B, data );
      //fup.addScaFrame( &trid, SCA_DEV_CONFIG, 0,
      //                 SCA_CONFIG_RD_B, data );
      //fup.uploadScaFrames( elinknr );
      exp_cnt += 3;

      //FlxDataChecker flxChecker;
      uint8_t       *chunkdata = 0;
      uint32_t       size;
      int            cnt = 0;
      //flxChecker.setReceiver( frecv );
      while( checker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
        {
          if( size == 0 ) continue;
          if( size <= 12 && chunkdata[2] == 254 ) // Transaction ID 254
            dev_mask = chunkdata[7];
          ++cnt;
        }

      //cout << "ENA=" << hex << (uint32_t) dev_mask << dec << endl;

      // Received vs expected number of messages
      if( cnt != exp_cnt )
        cout << "(Expected " << exp_cnt
             << ", recvd " << cnt << " msgs)" << endl;
      exp_cnt = 0;
    }

  //fup.stop();
  //if( frecv )
  //  frecv->stop();
  //cout << endl;
  //return 0;

  if( gpio_enable )
    {
      // Enable GPIO
      data[0] = 0x00;
      data[1] = dev_mask | (1 << (SCA_DEV_GPIO & 0x7));
      //data[1] = (1 << (SCA_DEV_ADC & 0x7)) | (1 << (SCA_DEV_DAC & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_B, data );
      ++exp_cnt;

      // Set GPIO direction to all output
      data[0] = 0xFF; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                          SCA_GPIO_WR_DIR, data );
      ++exp_cnt;

      cout << "GPIO+output enabled" << endl;
    }

  // I2C (*EXTRA*)
  int i2c_chan = 2;
  int i2c_dev  = SCA_DEV_I2C + i2c_chan;
  int cmd;
  if( i2c_chan >= 0 && i2c_chan < 5 )
    cmd = SCA_CONFIG_WR_B;
  else if( i2c_chan < 0xD )
    cmd = SCA_CONFIG_WR_C;
  else if( i2c_chan < 0x10 )
    cmd = SCA_CONFIG_WR_D;
  else
    {
      i2c_chan = 0;
      cmd = SCA_CONFIG_WR_B;
    }

  bool i2c_enable = true;
  if( i2c_enable )
    {
      // Enable the selected I2C channel
      data[0] = 0x00;
      data[1] = dev_mask | (1 << (i2c_dev & 0x7));
      if( cmd == SCA_CONFIG_WR_B )
        data[1] |=  (1 << (SCA_DEV_GPIO & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2, cmd, data );
      ++exp_cnt;
      cout << "I2C enabled" << endl;
    }

  int i2c_rate = 4;
  if( i2c_rate == 2 )
    i2c_rate = SCA_I2C_CTRL_200K;
  else if( i2c_rate == 4 )
    i2c_rate = SCA_I2C_CTRL_400K;
  else if( i2c_rate == 10 )
    i2c_rate = SCA_I2C_CTRL_1M;
  else
    i2c_rate = SCA_I2C_CTRL_100K;

  // Number of bytes to write in the I2C write part:
  // size of register address plus number of bytes to write (if any)
  int i2c_addr     = 0x01;
  int i2c_raddr    = 0x23; // register address
  int i2c_rval;
  int i2c_raddr_sz = 1;    // 1-byte address
  int i2c_rval_sz  = 2;    // 2-byte register
  int i2c_nbytes = i2c_raddr_sz + i2c_rval_sz;

  // Write I2C channel control: I2C speed and payload number of bytes
  // (i.e. the 1- or 2-byte GBTx register address
  //  plus -optionally- the bytes to write)
  data[0] = 0;
  data[1] = i2c_rate | (i2c_nbytes << 2);
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_WR_CTRL, data );
  ++exp_cnt;

  // DAC (*EXTRA*)
  //const int DAC_WR_CMD[] = {
  //  SCA_DAC_A_WR, SCA_DAC_B_WR, SCA_DAC_C_WR, SCA_DAC_D_WR
  //};
  const int DAC_RD_CMD[] = {
    SCA_DAC_A_RD, SCA_DAC_B_RD, SCA_DAC_C_RD, SCA_DAC_D_RD
  };

  bool dac_enable = true;
  if( dac_enable )
    {
      // Enable DAC
      data[0] = 0x00;
      data[1] = dev_mask | (1 << (SCA_DEV_DAC & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_D, data );
      ++exp_cnt;
      cout << "DAC enabled" << endl;
    }

  // -------------------------------------------------------------------------
  // Operate GPIO: operate the GPIO-bits as if it were a 32-bits counter,
  // running it up to value 'repeat_cnt'

  unsigned int val = 0;
  for( int i=0; i<repeat_cnt/group_sz; ++i )
    {
      fup.resetScaFrames();
      for( int j=0; j<group_sz; ++j, ++val )
        {
          data[0] = (uint8_t) ((val >> 16) & 0xFF);
          data[1] = (uint8_t) ((val >> 24) & 0xFF);
          data[2] = (uint8_t) ((val >>  0) & 0xFF);
          data[3] = (uint8_t) ((val >>  8) & 0xFF);

          // Set GPIO direction
          // ###NB: *removing* this message causes issues with replies to
          // subsequent I2C commands below (due to bug in the GBT-SCA chip
          // which requires a work-around in FELIX firmware)
          fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, data );
          ++exp_cnt;

          fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, data );
          ++exp_cnt;
          if( sca_delay > 0 )
            fup.addScaDelay( sca_delay );

          {
            // I2C (*EXTRA*, copied and adapted from fscai2c.cpp)
            // Fill I2C data transmit buffer,
            // i.e. the register address plus additional byte(s) to write
            i2c_rval = val;
            if( i2c_raddr_sz == 2 )
              {
                // ...
              }
            else
              {
                data[1] = (u8) (i2c_raddr & 0xFF);
                if( i2c_rval_sz >= 1 )
                  data[0] = (u8) (i2c_rval & 0xFF);
                else
                  data[0] = 0;
                if( i2c_rval_sz >= 2 )
                  data[3] = (u8) ((i2c_rval & 0xFF00) >> 8);
                else
                  data[3] = 0;
                if( i2c_rval_sz >= 3 )
                  data[2] = (u8) ((i2c_rval & 0xFF0000) >> 16);
                else
                  data[2] = 0;
                fup.addScaFrame( &trid, i2c_dev, 4,
                                 SCA_I2C_WR_DATA0, data );
                ++exp_cnt;
              }
            // Write the bytes
            data[1] = i2c_addr;
            data[0] = 0; data[2] = 0; data[3] = 0;
            fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_MULTI_7B_WR, data );
            ++exp_cnt;
            if( sca_delay > 0 )
              fup.addScaDelay( sca_delay );
          }

          {
            // DAC (*EXTRA*, copied and adapted from fscadac.cpp)
            //for( int dac=0; dac<1; ++dac )
            for( int dac=0; dac<4; ++dac )
              {
                // Read
                data[1] = 0;
                fup.addScaFrame( &trid, SCA_DEV_DAC,
                                 1, DAC_RD_CMD[dac], data );
              }
            exp_cnt += 4;
          }
        }

      fup.uploadScaFrames( elinknr );

      getReplies( &checker, elinknr );
    }

  // Final batch, less than 'group_sz' messages
  fup.resetScaFrames();
  int rest = repeat_cnt-(repeat_cnt/group_sz)*group_sz;
  for( int i=0; i<rest; ++i ) {

      data[0] = (uint8_t) ((val >> 16) & 0xFF);
      data[1] = (uint8_t) ((val >> 24) & 0xFF);
      data[0] = (uint8_t) ((val >> 16) & 0xFF);
      data[0] = (uint8_t) ((val >> 16) & 0xFF);
      fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, data );
  }
  exp_cnt += rest;
  fup.uploadScaFrames( elinknr );

  getReplies( &checker, elinknr );

  if( recvReplies != exp_cnt )
    cout << "### Expected " << exp_cnt << ", got " << recvReplies << endl;

  cout << "Totals: Replies " << recvReplies;
  cout << " Large " << largeReplies ;
  cout << " Zero " << zeroReplies;
  cout << " (Errors: CRC=" << scaCrcErrors
       << " CMD=" << scaCmdErrors
       << " (S)REJ=" << scaRejErrors << ")";
  cout << endl;

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
  cout << "fsca-gpio-i2c-test version " << hex << VERSION_ID << dec << endl <<
    "Tool to send a mix of multiple GBT-SCA GPIO and I2C commands and receive "
    "and check the replies.\n"
    "Usage:\n"
    " fsca-gpio-i2c-test [-h|V] [-d <devnr>] [-e <elink>] [-G <gbt>] "
    "[-g <group>] [-p <path>] \n"
    "                    [-r <cnt>] [-s <size>] [-P <us>] [-C] [-D] [-E] "
    "[-R] [-X] [-Z]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -P <us>    : Add pause after some commands (in us, default:0).\n"
    "  -r <cnt>   : Number of write-output messages to GBT-SCA GPIO "
    "(default: 1000).\n"
    "  -s <size>  : GBT-SCA message group size used (default: 1000).\n"
    "  -C         : Send GBT-SCA connect (HDLC control).\n"
    "  -R         : Send GBT-SCA reset (HDLC control).\n"
    "  -D         : Disable GBT-SCA GPIO after operation "
    "(default: leave enabled)\n"
    "  -E         : Do *not* enable GBT-GPIO at start of operation, "
    "assume it already is.\n"
    "  -X         : Use continuous-mode DMA for upload "
    "(default: single-shot).\n"
    "  -Z         : Do not receive and display the GBT-SCA replies.\n";
}

// ----------------------------------------------------------------------------
