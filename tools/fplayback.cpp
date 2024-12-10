#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x19082200; // Add -R option: random data playback
//const int VERSION_ID = 0x19061800; // Add -x and -X options
//const int VERSION_ID = 0x17100900; // Fix chunklength bug in FlxUpload
//const int VERSION_ID = 0x17091900;
//const int VERSION_ID = 0x16101100;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int      opt;
  bool     dbg_mode         = false;
  int      cardnr           = 0;
  int      blocksize        = -1;
  int      elink_filter     = -1;
  int      dma_index        = 1; // fdaq uses 0 by default, so use 1 here
  int      blocks_to_upload = 0;
  int      speed_factor     = 1;
  uint32_t dma_size         = 2;
  bool     dma_circular     = false;
  bool     random_data      = false;
  string   filename;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:De:f:F:hi:VRx:Xy:")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          dbg_mode = true;
          break;
        case 'e':
          // Single E-link number to upload to and extract from file
          unsigned int x;
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elink_filter = x;
          if( elink_filter < 0 || elink_filter > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
          break;
        case 'f':
          // A factor to increase upload speed with (at your own risk)
          if( sscanf( optarg, "%d", &speed_factor ) != 1 )
            arg_error( 'f' );
          if( speed_factor < 0 || speed_factor > 999 )
            arg_range( 'f', 0, 999 );
          break;
        case 'F':
          // Number of blocks to upload (default: all in file)
          if( sscanf( optarg, "%d", &blocks_to_upload ) != 1 )
            arg_error( 'F' );
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          // DMA controller to use
          if( sscanf( optarg, "%d", &dma_index ) != 1 )
            arg_error( 'i' );
          if( dma_index < 0 || dma_index > 7 )
            arg_range( 'i', 0, 7 );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'R':
          random_data = true;
          break;
        case 'x':
          // DMA size in KByte
          if( sscanf( optarg, "%u", &dma_size ) != 1 )
            arg_error( 'x' );
          if( dma_size < 1 || dma_size > 8 )
            arg_range( 'x', 1, 8 );
          break;
        case 'X':
          // Use circular (endless) DMA
          dma_circular = true;
        case 'y':
          // FLX-device block size in KByte
          if( sscanf( optarg, "%d", &blocksize ) != 1 )
            arg_error( 'x' );
          if( blocksize < 1 || blocksize > 1024 )
            arg_range( 'x', 1, 1024 );
          blocksize *= 1024;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Name of file with data to upload
  if( optind < argc )
    filename = string( argv[optind] );
  if( filename.empty() )
    {
      cout << "### Missing data file name" << endl;
      return 1;
    }

  FlxUpload fup( cardnr, 0, dma_index );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-card " << cardnr
       << ", firmw " << fup.firmwareVersion()
       << " (cmem buffersize="
       << fup.bufferSize()/(1024*1024) << "MB)" << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  fup.setDmaSize( dma_size*1024 );

  fup.setDmaCircular( dma_circular );

  fup.setPlaybackRandom( random_data );

  if( !fup.startPlayback( filename, blocksize ) )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }

  cout << "(Each '.' represents 10K FLX-blocks uploaded)\n";
  int block_cnt = 0;
  while( fup.playback( elink_filter, speed_factor ) )
    {
      if( dbg_mode )
        {
          // Display something after each block upload
          if( fup.playbackBlockCounter() - block_cnt > 0 )
            {
              block_cnt = fup.playbackBlockCounter();
              cout << "Blocks: " << block_cnt
                   << ", Chunks: " << fup.playbackChunkCounter() << endl;
            }
        }
      else
        {
          if( fup.playbackBlockCounter() - block_cnt >= 10000 )
            {
              block_cnt = fup.playbackBlockCounter();
              cout << "."; cout.flush();
              if( (block_cnt % 500000) == 0 )
                cout << endl;
            }
        }

      if( blocks_to_upload > 0 &&
          fup.playbackBlockCounter() >= blocks_to_upload )
        break;
    }
  if( block_cnt > 10000 ) cout << endl;

  fup.stop();

  cout << "Uploaded " << fup.playbackBlockCounter()
       << " 1K-blocks, " << fup.playbackChunkCounter()
       << " chunks" << endl;

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fplayback version " << hex << VERSION_ID << dec << endl <<
    "Upload data chunks from an 'fdaq'-generated file to an FLX-device.\n"
    "Usage:\n"
    " fplayback [-h|V] [-d <devnr>] [-F <blocks>] [-i <dma>] [-e <elink>]\n"
    "           [-R] [-x <size>] [-X] [-y <size>] [<filename>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : Upload only the data blocks for E-link number <elink>.\n"
    "  -F <blocks>: Number of blocks to upload (default: all in file).\n"
    "  -f <speed> : Speed up default upload rate of about 8MB/s by "
    "factor <speed> (default:1)\n"
    "  -i <dma>   : FLX-device DMA controller to use (default: 1).\n"
    "  -R         : Replace chunk data by pseudo-random data.\n"
    "  -x <size>  : Size of single-shot DMA transfers, in KByte "
    "(default: 2).\n"
    "  -X         : Use continuous-mode DMA for upload "
    "(default: single-shot).\n"
    "  -y <size>  : Set size of FLX-device unit data block, in KB.\n"
    "               (forced; normally read from FLX-device itself).\n\n"
    " <filename>: Name of file with data blocks to upload.\n";
}

// ----------------------------------------------------------------------------
