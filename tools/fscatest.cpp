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
const int   VERSION_ID = 0x18101200; // Use FlxDataChecker::displayDataRecvd()
//const int VERSION_ID = 0x17092400;

#define sleep_ms(x) usleep((x)*1000)

void display_replies( FlxReceiver *recvr,
                      uint64_t     timestamp_sent,
                      int          elinknr,
                      string       title = string(),
                      int          chunks_expected = -1,
                      int          first_trid = -1 );

// To map GPIO bit number to a byte index in the GPIO SCA command
const int GPIO_BYTE_MAP[] = { 2, 3, 0, 1 };

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int   opt;
  int   cardnr      = 0;
  int   gbtnr       = 0;
  int   egroupnr    = 7; // EC-link
  int   epathnr     = 7; // EC-link
  int   dma_write   = -1; // Autoselect FromHost DMA controller index
  int   dma_read    = 0;
  bool  receive     = false;
  bool  use_intrpt  = false;
  int   repeat_cnt  = 0;
  int   par         = 0;
  bool  do_jtag     = false;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:G:g:hi:Ijp:r:Vx:Z")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
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
          // DMA controller to use
          if( sscanf( optarg, "%d", &dma_write ) != 1 )
            arg_error( 'i' );
          if( dma_write < 0 || dma_write > 7 )
            arg_range( 'i', 0, 7 );
          break;
        case 'I':
          use_intrpt = true;
          break;
        case 'j':
          do_jtag = true;
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
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'x':
          if( sscanf( optarg, "%d", &par ) != 1 )
            arg_error( 'x' );
          break;
        case 'Z':
          receive = true;
          break;
        default: // '?'
          usage();
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

  FlxReceiver *flxReceiver = 0;
  if( receive )
    {
      flxReceiver = new FlxReceiver( cardnr, 64*1024*1024, dma_read );
      if( flxReceiver->hasError() )
        {
          cout << "### " << flxReceiver->errorString() << endl;
          flxReceiver->stop();
          return 1;
        }
      cout << "Opened FLX-device " << cardnr
           << ", firmw " << flxReceiver->firmwareVersion()
           << ", " << flxReceiver->numberOfChans()
           <<  " chans (cmem bufsize="
           << flxReceiver->bufferSize() << ", receive: "
           << "DMA #" << dma_read;
      if( use_intrpt )
        cout << " interrupt-driven)" << endl;
      else
        cout << " polling)" << endl;

      flxReceiver->setUseInterrupt( use_intrpt );
    }

  int elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                 (egroupnr << BLOCK_EGROUP_SHIFT) |
                 (epathnr  << BLOCK_EPATH_SHIFT));

  // -------------------------------------------------------------------------
  // Connect

  if( fup.scaConnect( elinknr ) )
    cout << "GBT-SCA connect" << endl;
  else
    cout << "###GBT-SCA connect: " << fup.errorString() << endl;
  display_replies( flxReceiver, fup.timestamp(), elinknr, "connect" );

  u8   data[4];
  int  byte_i;
  u8   bit_i;
  int  trid = 1, first_trid;
  int  repeat;
  if( repeat_cnt <= 0 )
    repeat_cnt = 1;
  struct timespec ts1, ts2;

  if( do_jtag )
    goto jtag_ops;

  // -------------------------------------------------------------------------
  // Enable GPIO
  data[0] = 0x00;
  data[1] = 1 << SCA_DEV_GPIO;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_B, data );

  // Set GPIO direction to all output
  //data[0] = 0xFF; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF; //DEBUG
  // Set only GPIO bit 'par' to output:
  // order is bit 16-23, bit 24-31, bit 0-7, bit 8-15
  data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
  byte_i = GPIO_BYTE_MAP[par/8];
  bit_i  = 1 << (par & 0x7);
  data[byte_i] |= bit_i;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_WR_DIR, data );

  // Read GPIO direction bits
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 0,
                      SCA_GPIO_RD_DIR, data );
  sleep_ms( 1 );

  display_replies( flxReceiver, fup.timestamp(), elinknr, "GPIO init" );

  first_trid = trid;
  fup.resetScaFrames();
  data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
  repeat = 0;
  clock_gettime( CLOCK_REALTIME, &ts1 );
  while( repeat < repeat_cnt )
    {
      // VLDB LEDs are on GPIO #18 and #21
      data[byte_i] |= bit_i;
      fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, data );
      fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_RD_OUT, data );
      data[byte_i] &= ~bit_i;
      fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, data );
      if( !fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_RD_OUT, data ) )
        break;
      // Playing with added delay:
      if( !fup.addScaDelayUs( 100 ) )
        break;
      ++repeat;
    }
  if( repeat == repeat_cnt )
    {
      //fup.dumpScaFrames();
      fup.uploadScaFrames( elinknr );
      clock_gettime( CLOCK_REALTIME, &ts2 );
      display_replies( flxReceiver, fup.timestamp(), elinknr,
                       "GPIO out", 4*repeat, first_trid );

      // Display the time the upload took
      int64_t ns = ((ts2.tv_sec*1000000000 + ts2.tv_nsec)-
                    (ts1.tv_sec*1000000000 + ts1.tv_nsec));
      cout << "Tupload=" << ns/1000 << "." << setfill('0') << setw(3)
           << ns - (ns/1000)*1000 << "us" << endl;
    }
  else
    {
      cout << "### No space in SCA frames buffer (r=" << repeat << ")\n";
    }

  sleep_ms( 1 );
  // Disable GBT-SCA devices (channels)
  data[0] = 0x00; data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_B, data );

  // Check if disabled: (try to) set GPIO direction to all output
  data[0] = 0xFF; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_WR_DIR, data );

  display_replies( flxReceiver, fup.timestamp(), elinknr, "GPIO disable" );

  return 0;

  // -------------------------------------------------------------------------
 jtag_ops:
  // Enable JTAG channel
  data[0] = 0x00;
  data[1] = 1 << (SCA_DEV_JTAG & 7);
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_D, data );

  // Configure JTAG channel (standard; LEN=128 (represented by value 0)
  data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_JTAG, 4,
                      SCA_JTAG_WR_CTRL, data );
  // Frequency of 20MHz: f=2*10000000/(DIV+1) --> DIV=0
  // (NB: LSByte is in data[2] and MSByte in data[3])
  data[0] = 0xFF; data[1] = 0xFF; data[2] = 0x01; data[3] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_JTAG, 4,
                      SCA_JTAG_WR_FREQ, data );
  sleep_ms( 1 );

  display_replies( flxReceiver, fup.timestamp(), elinknr, "JTAG init" );

  u8  p[256];
  int i;
  for( i=0; i<256; ++i )
    p[i] = (u8) (i & 0xFF);
  int global_repeat_cnt = 1, global_repeat = 0;
  first_trid = trid;
  i = 0;
  clock_gettime( CLOCK_REALTIME, &ts1 );
  while( global_repeat < global_repeat_cnt )
    {
      fup.resetScaFrames();
      repeat = 0;
      while( repeat < repeat_cnt )
        {
          // Write TDO bits (4x32=128bits=16bytes)
          data[2] = p[i++]; data[3] = p[i++];
          data[0] = p[i++]; data[1] = p[i++];
          fup.addScaFrame( &trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO0, data );
          data[2] = p[i++]; data[3] = p[i++];
          data[0] = p[i++]; data[1] = p[i++];
          fup.addScaFrame( &trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO1, data );
          data[2] = p[i++]; data[3] = p[i++];
          data[0] = p[i++]; data[1] = p[i++];
          fup.addScaFrame( &trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO2, data );
          data[2] = p[i++]; data[3] = p[i++];
          data[0] = p[i++]; data[1] = p[i++];
          if( !fup.addScaFrame( &trid, SCA_DEV_JTAG, 4,
                                SCA_JTAG_WR_TDO3, data ) )
            break;
          if( i == 256 ) i = 0;

          // Optional: read back TDO bits
          /**/
            fup.addScaFrame( &trid, SCA_DEV_JTAG, 0, SCA_JTAG_RD_TDI0, data );
            fup.addScaDelayUs( 1 ); // Required!
            fup.addScaFrame( &trid, SCA_DEV_JTAG, 0, SCA_JTAG_RD_TDI1, data );
            fup.addScaDelayUs( 1 ); // Required!
            fup.addScaFrame( &trid, SCA_DEV_JTAG, 0, SCA_JTAG_RD_TDI2, data );
            fup.addScaDelayUs( 1 ); // Required!
            if( !fup.addScaFrame( &trid, SCA_DEV_JTAG, 0,
                                  SCA_JTAG_RD_TDI3, data ) )
            break;
            /**/

          // Go!
          data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
          fup.addScaFrame( &trid, SCA_DEV_JTAG, 2, SCA_JTAG_GO, data );
          fup.addScaDelayUs( 2*8 ); // 8 really seems the minimum!
          ++repeat;
        }
      ++global_repeat;
      if( repeat != repeat_cnt )
        break;
      fup.uploadScaFrames( elinknr );
    }

  if( repeat == repeat_cnt )
    {
      //fup.dumpScaFrames();
      //fup.uploadScaFrames( elinknr );
      clock_gettime( CLOCK_REALTIME, &ts2 );
      display_replies( flxReceiver, fup.timestamp(), elinknr,
                       //"JTAG ops", 9*repeat, first_trid );
                       "JTAG ops", 5*repeat*global_repeat, first_trid );

      // Display the time the upload took
      int64_t ns = ((ts2.tv_sec*1000000000 + ts2.tv_nsec)-
                    (ts1.tv_sec*1000000000 + ts1.tv_nsec));
      cout << "Tupload=" << ns/1000 << "." << setfill('0') << setw(3)
           << ns - (ns/1000)*1000 << "us" << endl;
    }
  else
    {
      cout << "### No space in SCA frames buffer (r=" << repeat << ")\n";
    }

  sleep_ms( 1 );
  // Disable GBT-SCA devices (channels)
  data[0] = 0x00; data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_D, data );

  // -------------------------------------------------------------------------
  // Finish up

  fup.stop();
  if( flxReceiver )
    flxReceiver->stop();
  cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscatest version " << hex << VERSION_ID << dec << endl <<
    "*** UNDER DEVELOPMENT ***\n"
    "Test upload of multiple commands in one upload to a GBT-SCA chip\n"
    "via any 2-bit wide HDLC encoded e-link or EC-link.\n"
    "Usage:\n"
    " fscatest [-h|V] [-d <devnr>] [-i <dma>] [-I] [-G <gbt>] [-g <group>] "
    "[-p <path>] [-x <par>] [-j]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -i <dma>   : FLX-device DMA controller to use (default: 1).\n"
    "  -I         : Use interrupt to receive data (default: polling)\n"
    "  -j         : Do JTAG operations.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -r <repeat>: Number of GPIO/ADC/DAC operations to perform "
    "(default: 1).\n"
    "  -x <par>   : Parameter to use in operations, e.g. GPIO number, "
    "ADC or DAC channel (default: 0).\n"
    "  -Z         : Receive, check (TRID, CRC) and display "
    "the GBT-SCA replies.\n";
}

// ----------------------------------------------------------------------------

void display_replies( FlxReceiver *recvr,
                      uint64_t     timestamp_sent,
                      int          elinknr,
                      string       title,
                      int          chunks_expected,
                      int          first_trid )
{
  if( recvr == 0 ) return;

  // Optionally have a title for the GBT-SCA replies displayed
  cout << endl;
  if( title.length() )
    cout << ">> " << title << " <<" << endl;

  // Go through the chunks of each block (skipping any TIMEOUT/NULL/OOB chunks)
  // displaying the data bytes, interpreting chunks as GBT-SCA replies,
  // counting the number of chunks
  FlxDataChecker flxChecker;
  int  timeout_us  = 4000;
  bool display_sca = true;
  int  chunks      = 0;
  chunks += flxChecker.displayDataRecvd( recvr, timeout_us, elinknr,
                                         display_sca, first_trid );

  uint64_t ts = recvr->timestamp();
  if( ts > timestamp_sent )
    cout << "T = " << setw(3) << setfill(' ') << ((ts-timestamp_sent)+500)/1000
      //<<"." << setfill('0') << setw(3) << (ts-timestamp_sent)%1000
      //<< " (" << ts << " " << timestamp_sent << ")"
         << endl;
  else
    cout << "T = ??? " << ts << ", " << timestamp_sent << endl;

  if( chunks_expected > 0 && chunks != chunks_expected )
    cout << "### Received " << chunks
         << ", expected " << chunks_expected << endl;
}

// ----------------------------------------------------------------------------
