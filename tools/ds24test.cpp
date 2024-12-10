// Test program to try out 1-wire DS24xx ID readout
// using FELIX and a GBT-SCA (on a VLDB board)
// Henk B, 30 Dec 2016

#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x17012300;

// To map GPIO bit number to a byte index in the GPIO SCA command
const int GPIO_BYTE_MAP[] = { 2, 3, 0, 1 };

// ----------------------------------------------------------------------------

void insert_delay( long us )
{
  // Using usleep()
  //usleep( us );

  // Using nanosleep()
  struct timespec ts;
  //ts.tv_sec  = 0;
  //ts.tv_nsec = us * 1000; // in nanoseconds
  //nanosleep( &ts, 0 );

  // Using busy-wait
  time_t sec;
  long   nsec;
  clock_gettime( CLOCK_REALTIME, &ts );
  sec  = ts.tv_sec;
  nsec = ts.tv_nsec + us*1000;
  if( nsec >= 1000000000 )
    {
      nsec -= 1000000000;
      ++sec;
    }
  while( true )
    {
      clock_gettime( CLOCK_REALTIME, &ts );
      if( ts.tv_sec == sec )
        {
          if( ts.tv_nsec >= nsec )
            break;
        }
      else if( ts.tv_sec > sec )
        {
          break;
        }
    }
}

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int   opt;
  int   cardnr      = 0;
  int   gbtnr       = 0;
  //int egroupnr    = 5; // EC
  int   egroupnr    = 7; // EC
  int   epathnr     = 7; // EC
  int   dma_read    = 0;
  int   dma_write   = -1; // Autoselect FromHost DMA controller index
  int   par         = 0;
  bool  receive     = true;

  // Parse the options
  while( (opt = getopt(argc, argv, "G:hrvx:")) != -1 )
    {
      switch( opt )
        {
        case 'G':
          // GBT link number to upload to
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'r':
          receive = false;
          break;
        case 'v':
          cout << "Version " << hex << VERSION_ID << dec << endl;
          return 0;
        case 'x':
          if( sscanf( optarg, "%d", &par ) != 1 )
            arg_error( 'x' );
          break;
        default: // '?'
          usage();
          return 0;
        }
    }

  FlxUpload fup( cardnr, 0, dma_write );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 0;
    }
  cout << "Opened FLX-card " << cardnr
       << ", firmw " << fup.firmwareVersion()
       << " (cmem buffersize="
       << fup.bufferSize()/(1024*1024) << "MB, upload)" << endl;


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
          return 0;
        }
      cout << "Opened FLX-card " << cardnr
           << ", firmw " << frecv->firmwareVersion()
           << " (cmem buffersize="
           << frecv->bufferSize()/(1024*1024) << "MB, receive)" << endl;
    }

  cout << "=> 1-Wire on GPIO #" << par << endl;

  u8   data[4];
  int  i, j;
  int  trid = 1;
  int  recv_trid; // Specific TRID to indicate a reply containing data for us
  int  elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                  (egroupnr << BLOCK_EGROUP_SHIFT) |
                  (epathnr  << BLOCK_EPATH_SHIFT));
  int  iobyt = GPIO_BYTE_MAP[par/8];
  u8   iobit = 1 << (par & 0x7);
  memset( static_cast<void *> (data), 0, sizeof(data) );

  // -------------------------------------------------------

  fup.scaReset( elinknr );

  // Enable GPIO
  data[0] = 0x00;
  data[1] = 1 << SCA_DEV_GPIO;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG,
                      2, SCA_CONFIG_WR_B, data );

  // Set GPIO direction: all inputs
  data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO,
                      4, SCA_GPIO_WR_DIR, data );

  // -------------------------------------------------------
  // ds24_init():

  fup.resetScaFrames();

  // ds24_to_output()
  data[iobyt] |= iobit;
  //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO,
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   4, SCA_GPIO_WR_DIR, data );

  // ds24_clear()
  data[iobyt] &= ~iobit;
  //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO,
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   4, SCA_GPIO_WR_OUT, data );

  // 500 us delay
  //insert_delay( 500 );
  fup.addScaDelayUs( 500 );

  // ds24_to_input()
  data[iobyt] &= ~iobit;
  //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO,
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   4, SCA_GPIO_WR_DIR, data );
  // 70 us delay
  //insert_delay( 70 );
  fup.addScaDelayUs( 70 );

  // If a DS2411 device is present it will pull the line low
  //recv_trid = 0xFE;
  //fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_GPIO,
  //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO,
  fup.addScaFrame( &trid, SCA_DEV_GPIO,
                   2, SCA_GPIO_RD_IN, data );  
  // 400 us delay
  //insert_delay( 400 );
  fup.addScaDelayUs( 400 );

  // #### TEST
  /*{
    fup.resetScaFrames();

    // ds24_to_output()
    data[iobyt] |= iobit;
    fup.addScaFrame( &trid, SCA_DEV_GPIO,
    4, SCA_GPIO_WR_DIR, data );
    for( int k=0; k<20; ++k )
    {
    if( k & 1 )
    data[iobyt] |= iobit;
    else
    data[iobyt] &= ~iobit;
    fup.addScaFrame( &trid, SCA_DEV_GPIO,
    4, SCA_GPIO_WR_OUT, data );
    fup.addScaFrame( &trid, SCA_DEV_CONFIG, 2,
    SCA_CONFIG_RD_A, data );
    fup.addScaFrame( &trid, SCA_DEV_CONFIG, 2,
    SCA_CONFIG_RD_A, data );
    fup.addScaFrame( &trid, SCA_DEV_CONFIG, 2,
    SCA_CONFIG_RD_A, data );
    fup.addScaFrame( &trid, SCA_DEV_CONFIG, 2,
    SCA_CONFIG_RD_A, data );
    //fup.addScaFrame( &trid, SCA_DEV_GPIO,
    //                 4, SCA_GPIO_WR_OUT, data );
    }

    fup.uploadScaFrames( elinknr );
    }*/
  //return 0;

  // -------------------------------------------------------
  // ds24_write_byte( DS2401_READROM_CMD ):

  //fup.resetScaFrames();
  int val = 0x33; // DS24_READROM_CMD
  for( i=0; i<8; ++i, val>>=1 )
    {
      fup.addScaDelay( 300 ); // >100

      // ds24_to_output()
      data[iobyt] |= iobit;
      fup.addScaFrame( &trid, SCA_DEV_GPIO,
                       4, SCA_GPIO_WR_DIR, data );

      // ds24_clear()
      data[iobyt] &= ~iobit;
      fup.addScaFrame( &trid, SCA_DEV_GPIO,
                       4, SCA_GPIO_WR_OUT, data );

      if( val & 1 )
        {
          // ds24_to_input()
          data[iobyt] &= ~iobit;
          fup.addScaFrame( &trid, SCA_DEV_GPIO, 4,
                           SCA_GPIO_WR_DIR, data );
        }
      else
        {
          // Delay..
          fup.addScaDelay( 300 ); // >24*12
          //for( int k=0; k<21; ++k ) // 20,21,22 works, <20 and >22 doesn't
          //  fup.addScaFrame( &trid, SCA_DEV_CONFIG, 2,
          //                       SCA_CONFIG_RD_A, data );
          // ds24_to_input()
          data[iobyt] &= ~iobit;
          fup.addScaFrame( &trid, SCA_DEV_GPIO, 4,
                           SCA_GPIO_WR_DIR, data );
        }
    }
  //fup.uploadScaFrames( elinknr );

  // -------------------------------------------------------
  // Read ID bytes:
  //for( j=0; j<DS2401_BYTES; ++j )
  //  ds24_data[j] = ds24_read_byte();
  //fup.resetScaFrames();
  for( j=0; j<8; ++j ) 
    // ds24_read_byte():
    for( i=0; i<8; ++i )
      {
        fup.addScaDelay( 350 ); // >=310 && <=390

        // ds24_to_output()
        data[iobyt] |= iobit;
        fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, data );
        // ds24_clear()
        data[iobyt] &= ~iobit;
        fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, data );
        // ds24_to_input()
        data[iobyt] &= ~iobit;
        fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, data );
        // Read
        recv_trid = 0xFE;
        fup.addScaFrame( &recv_trid, SCA_DEV_GPIO, 2,
                         SCA_GPIO_RD_IN, data );
      }
  //fup.dumpScaFrames();
  fup.uploadScaFrames( elinknr );

  // -------------------------------------------------------
  // Disable GPIO
  data[0] = 0x00; data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG,
                      2, SCA_CONFIG_WR_B, data );

  // -------------------------------------------------------
  // Extract the GBT-SCA replies from the datablock(s) received
  // and extract the DS2411 bits from those replies;
  // NB: replies with TRID=0xFE are the ones we're interested in.
  // Total number of expected SCA replies:
  // 2 + 4 + (8*(2+1)) + (8*8*4) + 1 = 6 + 24 + 256 + 1 = 287 replies
  // (so more than 287*(4+4)=2296 bytes),
  // of which (1+)8*8=64 or 65 carry one bit of significant information
  // i.e. (presence pulse and) 64 bits of the ID

  // Do not receive replies ?
  if( frecv == 0 )
    return 0;

  cout << hex << setfill('0');

  egroupnr = 7;
  epathnr  = 7;
  elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
             (egroupnr << BLOCK_EGROUP_SHIFT) |
             (epathnr  << BLOCK_EPATH_SHIFT));
  FlxDataChecker flxChecker;
  i64      nbytes, blocknr = 0;
  uint8_t *block, *chunkdata = 0;
  uint32_t size, id_byte = 0, id_bits = 0;
  flxChecker.setReceiver( frecv ); 
  while( frecv->hasData() )
    {
      nbytes = frecv->bytesAvailable();
      for( i=0; i<nbytes/BLOCK_BYTES; ++i )
        {
          block = (uint8_t *) frecv->data();

          //flxChecker.displayChunks( block, blocknr ); // DEBUG

          // Going to go through the chunks in this block
          flxChecker.setBlockToHandle( block, elinknr );

          // Get the chunks from this block one-by-one
          // (skipping any TIMEOUT/NULL/OOB chunks)
          while( flxChecker.nextChunkFromBlock( &chunkdata, &size ) )
            {
              if( size == 0 ) continue;

              // Find chunks with SCA replies containing
              // our 'special' Transaction ID
              if( size == 12 &&
                  chunkdata[2] == 0xFE ) // Transaction ID
                {
                  // Display chunkdata
                  //for( uint32_t i=0; i<size; ++i ) // DEBUG
                  //  cout << " " << setw(2) << (uint32_t) chunkdata[i];
                  //cout << endl;

                  uint32_t gpio = 0;
                  for( int j=0; j<4; ++j )
                    gpio |= ((uint32_t) chunkdata[6+j]) << (GPIO_BYTE_MAP[j]*8);
                  //cout << gpio << endl; // DEBUG

                  // Extract the GPIO bit we're interested in
                  // and add it to the ID byte we're compiling
                  id_byte |= ((gpio >> par) & 1) << (id_bits & 0x7);
                  ++id_bits;
                  // ID byte complete? -> display it
                  if( (id_bits & 0x7) == 0 )
                    {
                      cout << setw(2) << id_byte << " ";
                      id_byte = 0; // Reset for next ID byte
                    }
                }
            }

          frecv->updateBytesConsumed( BLOCK_BYTES );
          ++blocknr;
        }
      usleep( 10000 ); // Make sure all data has arrived
    }
  frecv->stop();

  cout << endl << dec << "Processed " << blocknr << " blocks" << endl;

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "ds24test, version " << hex << VERSION_ID << dec << endl <<
    "*** UNDER DEVELOPMENT ***\n"
    "Test program to try out 1-wire DS24xx ID readout\n"
    "using FELIX and a GBT-SCA (on a VLDB board).\n"
    "Usage:\n"
    " ds24test [-h|v] [-G <gbt>] [-x <par>] [-r]\n"
    "  -h         : Show this help text.\n"
    "  -v         : Show version.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -r         : Do not receive replies (use e.g. 'fedump' for that).\n"
    "  -x <par>   : GPIO number the DS2411 is connected to.\n";
}

// ----------------------------------------------------------------------------
