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
const int   VERSION_ID = 0x19112500;
//const int VERSION_ID = 0x19092000;

// Mapping of GPIO bit (byte) number to a byte index
// in the GPIO SCA command or reply frame
const int GPIO_BYTE[] = { 2, 3, 0, 1 };

uint8_t ds24_crc( uint8_t *data );

// ----------------------------------------------------------------------------

static int zeroReplies  = 0;
static int recvReplies  = 0;
static int scaCmdErrors = 0;
static int scaRejErrors = 0;
static int scaCrcErrors = 0;

void getDs24Replies( FlxDataChecker *checker, int elinknr, int io )
{
  uint8_t *chunkdata = 0;
  uint32_t size;
  bool     err_occurred = false;
  uint32_t id_byte = 0;
  uint32_t id_bits = 0;
  int      iobyt = GPIO_BYTE[io/8];
  uint8_t  iobit = 1 << (io & 0x7);
  static uint8_t IdBytes[8];

  cout << hex << setfill( '0' );

  checker->startRecvTime(); // Reset time-out on receiving (none or non-stop)
  while( checker->nextChunkRecvd( &chunkdata, &size, 10000, elinknr ) )
    {
      ++recvReplies;
      if( size == 0 ) ++zeroReplies; // Keep track of these unexpected ones
      if( size < 5 ) continue;       // Ignore these

      // GBT-SCA reply parameters
      uint32_t ctrl     = (uint32_t) chunkdata[1];
      uint32_t trid     = (uint32_t) chunkdata[2];
      //uint32_t chan   = (uint32_t) chunkdata[3];
      uint32_t err      = (uint32_t) chunkdata[4];
      //uint32_t len    = (uint32_t) chunkdata[5];
      //uint32_t crc_lo = (uint32_t) chunkdata[size-1];

      // Check the reply for various errors
      if( err != 0 )
        {
          ++scaCmdErrors;
          err_occurred = true;
        }
      if( ctrl & HDLC_CTRL_SFORMAT )
        {
          uint32_t type = ctrl & HDLC_CTRL_TYPE_MASK;
          if( type == HDLC_CTRL_REJECT || type == HDLC_CTRL_SELREJECT )
            {
              ++scaRejErrors;
              err_occurred = true;
            }
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

      // Extract the single ID bit from the messages
      if( size == 12 && trid == 254 )
        {
          uint8_t *databytes = &chunkdata[6];
          // MSB first
          id_byte >>= 1;
          if( (databytes[iobyt] & iobit) != 0 )
            id_byte |= 0x80;
          ++id_bits;

          // An ID byte completed ?
          if( (id_bits & 0x7) == 0 )
            {
              // Display it
              cout << setw(2) << id_byte << " ";
              // Store it
              IdBytes[id_bits/8-1] = id_byte;
              // Reset for next ID byte
              id_byte = 0;
            }
          if( id_bits == 64 )
            {
              // ID should be complete (64 bits): check its CRC
              if( IdBytes[7] != ds24_crc( IdBytes ) )
                cout << " (###CRC 0x"
                     << (uint32_t) ds24_crc( IdBytes ) << ")";
              //else
              //  cout << " (CRC=OK)";
              cout << endl;
              id_bits = 0;
            }
        }
    }
  cout << dec << setfill( ' ' );

  cout << "Replies received: " << recvReplies;
  //if( largeReplies > 0 )
  //  cout << " (Large " << largeReplies << ")";
  if( zeroReplies > 0 )
    cout << " (Zero " << zeroReplies << ")";
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
  int  io           = -1;
  int  repeat_cnt   = 1;
  int  exp_cnt      = 0;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "Cd:DEe:G:g:hi:p:r:RVXZ")) != -1 )
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
          if( sscanf( optarg, "%d", &io ) != 1 )
            arg_error( 'i' );
          break;
        case 'p':
          // E-path number to upload to
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'r':
          if( sscanf( optarg, "%d", &repeat_cnt ) != 1 )
            arg_error( 'r' );
          if( repeat_cnt < 1 )
            arg_range( 'r', 1, 65536 );
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

  // Check for mandatory parameters
  if( io < 0 )
    {
      cout << "### Provide a GPIO bit number (-i)" << endl;
      return 1;
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
  // FLX-card sender, receiver and FELIX data chunk reader/data checker

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

  //fup.setDmaSize( 8*1024 );

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
    }

  FlxDataChecker checker;
  checker.setReceiver( frecv );

  // -------------------------------------------------------------------------
  // Initialize GBT-SCA

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
  // Configure GBT-SCA

  uint8_t data[4]      = { 0x00, 0x00, 0x00, 0x00 };
  uint8_t dev_mask = 0;
  int     trid = 1, recv_trid;

  if( gpio_enable || gpio_disable )
    {
      // Get the current 'channel enable mask', before adjusting it;
      // do it twice... first one likely results in an SREJ reply...
      recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_B, data );
      //fup.addScaFrame( &recv_trid, SCA_DEV_CONFIG, 0,
      //                 SCA_CONFIG_RD_B, data );
      recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_B, data );
      //fup.addScaFrame( &recv_trid, SCA_DEV_CONFIG, 0,
      //                 SCA_CONFIG_RD_B, data );
      //fup.uploadScaFrames( elinknr );

      exp_cnt += 2;

      uint8_t *chunkdata = 0;
      uint32_t size;
      int      cnt = 0;
      while( checker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
        {
          if( size <= 12 && chunkdata[2] == 254 ) // Transaction ID 254
            dev_mask = chunkdata[7];
          //cout << "sz=" << size << endl; //DEBUG
          ++cnt;
        }

      //cout << "ENA=" << hex << (uint32_t) dev_mask << dec << endl;

      // Received vs expected number of messages
      if( cnt != exp_cnt )
        cout << "Enable GPIO (expected " << exp_cnt
             << ", recvd " << cnt << " msgs)" << endl;
      exp_cnt = 0;
    }

  if( gpio_enable )
    {
      // Enable GPIO, taking the current channel enable mask into account
      data[0] = 0x00;
      data[1] = dev_mask | (1 << (SCA_DEV_GPIO & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_B, data );
      ++exp_cnt;

      //cout << "GPIO enabled" << endl;
    }

  // -------------------------------------------------------------------------
  // Get GPIO I/O status

  uint8_t dir_bits[4]  = { 0x00, 0x00, 0x00, 0x00 }; // All inputs
  //uint8_t dir_bits[4]= { 0xFF, 0xFF, 0xFF, 0xFF }; // All outputs
  uint8_t io_bits[4]   = { 0x00, 0x00, 0x00, 0x00 };

  // Get the current GPIO direction and I/O bits
  recv_trid = 253;
  fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_RD_DIR, data );
  ++exp_cnt;
  recv_trid = 254;
  fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_RD_IN, data );
  ++exp_cnt;

  uint8_t *chunkdata = 0;
  uint32_t size;
  int      cnt = 0;
  while( checker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
    {
      if( size == 12 && chunkdata[2] == 253 ) // Transaction ID 253
        {
          // Data bytes in bytes 6-9:
          // (NB: byte order is 2,3,0,1
          //      (bit 16-23, bit 24-31, bit 0-7, bit 8-15))
          dir_bits[0] = chunkdata[6];
          dir_bits[1] = chunkdata[7];
          dir_bits[2] = chunkdata[8];
          dir_bits[3] = chunkdata[9];
        }
      if( size == 12 && chunkdata[2] == 254 ) // Transaction ID 254
        {
          io_bits[0] = chunkdata[6];
          io_bits[1] = chunkdata[7];
          io_bits[2] = chunkdata[8];
          io_bits[3] = chunkdata[9];
        }
      ++cnt;
    }

  // Received vs expected number of messages
  if( cnt != exp_cnt )
    cout << "Get GPIO Direction+Data (expected " << exp_cnt
         << ", recvd " << cnt << " msgs)" << endl;
  exp_cnt = 0;

  // -------------------------------------------------------------------------
  // -------------------------------------------------------------------------

  int i, j;
  int iobyt = GPIO_BYTE[io/8];
  u8  iobit = 1 << (io & 0x7);

  fup.resetScaFrames();

  // The 1-Wire operation
  // -------------------------------------------------------
  // ds24_init():

  // ds24_to_output()
  dir_bits[iobyt] |= iobit;
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   4, SCA_GPIO_WR_DIR, dir_bits );

  // ds24_clear()
  io_bits[iobyt] &= ~iobit;
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   4, SCA_GPIO_WR_OUT, io_bits );

  // 500 us delay: insert_delay( 500 );
  fup.addScaDelayUs( 500 );

  // ds24_to_input()
  dir_bits[iobyt] &= ~iobit;
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   4, SCA_GPIO_WR_DIR, dir_bits );
  // 70 us delay: insert_delay( 70 );
  fup.addScaDelayUs( 70 );

  // If a DS2411 device is present it will pull the line low
  //recv_trid = 0xFE;
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   2, SCA_GPIO_RD_IN, data );
  // 400 us delay: insert_delay( 400 );
  //fup.addScaDelayUs( 400 ); // ALTERNATIVE DELAY CONFIG
  fup.addScaDelayUs( 400+200 );

  exp_cnt += 4;

  // -------------------------------------------------------
  // ds24_write_byte( DS2401_READROM_CMD ):
  int val = 0x33; // DS24_READROM_CMD
  for( i=0; i<8; ++i, val>>=1 )
    {
      //fup.addScaDelay( 300 ); // >100 // ALTERNATIVE DELAY CONFIG

      // ds24_to_output()
      io_bits[iobyt] |= iobit;
      fup.addScaFrame( &trid, SCA_DEV_GPIO,
                       4, SCA_GPIO_WR_DIR, io_bits );

      // ds24_clear()
      io_bits[iobyt] &= ~iobit;
      fup.addScaFrame( &trid, SCA_DEV_GPIO,
                       4, SCA_GPIO_WR_OUT, io_bits );

      if( val & 1 )
        {
          // ds24_to_input()
          dir_bits[iobyt] &= ~iobit;
          fup.addScaFrame( &trid, SCA_DEV_GPIO, 4,
                           SCA_GPIO_WR_DIR, dir_bits );
        }
      else
        {
          // Delay..
          fup.addScaDelay( 300 ); // >24*12

          // ds24_to_input()
          dir_bits[iobyt] &= ~iobit;
          fup.addScaFrame( &trid, SCA_DEV_GPIO, 4,
                           SCA_GPIO_WR_DIR, dir_bits );
        }
      fup.addScaDelay( 300 ); // >100
    }
  exp_cnt += 8*3;

  // -------------------------------------------------------
  // Read ID bytes (pseudo-code):
  //   for( j=0; j<DS2401_BYTES; ++j )
  //     ds24_data[j] = ds24_read_byte();

  for( j=0; j<8; ++j ) 
    // ds24_read_byte():
    for( i=0; i<8; ++i )
      {
        //fup.addScaDelay( 350 ); // >=310 && <=390 // ALTERNATIVE DELAY CONFIG

        // ds24_to_output()
        dir_bits[iobyt] |= iobit;
        fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, dir_bits );
        // ds24_clear()
        io_bits[iobyt] &= ~iobit;
        fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, io_bits );
        // ds24_to_input()
        dir_bits[iobyt] &= ~iobit;
        fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, dir_bits );

        // Read
        recv_trid = 0xFE;
        fup.addScaFrame( &recv_trid, SCA_DEV_GPIO, 2,
                         SCA_GPIO_RD_IN, data );

        fup.addScaDelay( 350 ); // >=310 && <=390
      }
  exp_cnt += 8*8*4;

  // In order to prevent SREJ replies due to HDLC sequence number jumps
  // when repeatedly uploading the above sequence of commands (repeat_cnt>1)
  // we need to have a number of messages that is a multiple of 8,
  // so append 'dummy' commands if necessary
  while( (exp_cnt & 7) != 0 )
    {
      // Avoid back-to-back GBT-SCA reply issues (until solved): add a delay
      fup.addScaDelay( 30 );
      fup.addScaFrame( &trid, SCA_DEV_CONFIG, 0,
                       SCA_CONFIG_RD_B, data );
      ++exp_cnt;
    }

  // The total number of expected replies
  exp_cnt *= repeat_cnt;

  //fup.dumpScaFrames(); // DEBUG

  // Execute the 1-Wire operation
  for( i=0; i<repeat_cnt; ++i )
    fup.uploadScaFrames( elinknr );

  // Check replies and extract the ID information
  cout << "ID:" << endl;
  getDs24Replies( &checker, elinknr, io );

  if( recvReplies != exp_cnt )
    cout << "### Expected " << exp_cnt << ", got " << recvReplies << endl;

  // -------------------------------------------------------------------------
  // -------------------------------------------------------------------------
  // Disable GPIO?

  if( gpio_disable )
    {
      // Restore 'channel enable mask'
      data[0] = 0x00;
      data[1] = dev_mask & ~(1 << (SCA_DEV_GPIO & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_B, data );
      cout << "GPIO disabled" << endl;
    }

  // -------------------------------------------------------------------------
  // Finish up

  fup.stop();
  if( frecv )
    frecv->stop();
  //cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscads24 version " << hex << VERSION_ID << dec << endl <<
    "Tool to read out the 64-bit ID from a 1-Wire DS24xx chip.\n"
    "Usage:\n"
    " fscads24 [-h|V] [-d <devnr>] [-e <elink>] [-G <gbt>] "
    "[-g <group>] [-p <path>] \n"
    "          [-r <cnt>] [-C] [-D] [-E] [-R] [-X] [-Z] -i <pin>\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -i <pin>   : Use GPIO bit <pin> for the 1-Wire protocol ([0..31]).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -r <cnt>   : Number of times to read the ID (default: 1).\n"
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

/* ------------------------------------------------------------------------ */

uint8_t ds24_crc( uint8_t *data )
{
  // Calculate CRC-8 value of the first 7 bytes of DS24xx data
  // (1 byte family code and 6 bytes serial number);
  // uses The CCITT-8 polynomial, expressed as X^8 + X^5 + X^4 + 1
  uint8_t crc = 0x00;
  for( int index=0; index<7; ++index ) {
    uint8_t byt = data[index];
    for( int b=0; b<8; ++b ) {
      if( (byt^crc) & 0x01 )
        {
          crc ^= 0x18;
          crc >>= 1;
          crc |= 0x80;
        }
      else
        {
          crc >>= 1;
        }
      byt >>= 1;
    }
  }
  return crc;
}

// ----------------------------------------------------------------------------
