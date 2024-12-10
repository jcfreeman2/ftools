#include <iostream>
#include <iomanip>
#include <csignal>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxReceiver.h"
#include "FlxParser.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x21111200; // -A also displays TTC2Host frames
//const int VERSION_ID = 0x21110300; // Add option -B (SCA command frames)
//const int VERSION_ID = 0x20061700; // Add option -O
//const int VERSION_ID = 0x20021800;
//const int VERSION_ID = 0x17091900;
//const int VERSION_ID = 0x17042100;

bool NotInterrupted = true;
void sigint_handler( int signal )
{
  NotInterrupted = false;
}

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    cardnr               = 0;
  int    dma_index            = 0; // fupload uses 1 by default, so use 0 here
  bool   use_intrpt           = false;
  int    elink_filter         = -1;
  int    word_size            = 1;
  bool   display_data_chunks  = false;
  bool   display_chunks       = true;
  bool   display_chunk_sz     = true;
  bool   display_timeout      = true;
  bool   display_timeout_data = true;
  bool   display_sca          = false;
  bool   display_sca_cmd      = false;
  bool   timestamp            = false;

  // Parse the options
  while( (opt = getopt(argc, argv, "O0248ABcDd:e:hi:ITV")) != -1 )
    {
      switch( opt )
        {
        case 'O':
          display_timeout = false;
          break;
        case '0':
          display_timeout_data = false;
          break;
        case '2':
          if( word_size > 1 )
            cout << "### Use only one of -2, -4 or -8 options" << endl;
          word_size = 2;
          break;
        case '4':
          if( word_size > 1 )
            cout << "### Use only one of -2, -4 or -8 options" << endl;
          word_size = 4;
          break;
        case '8':
          if( word_size > 1 )
            cout << "### Use only one of -2, -4 or -8 options" << endl;
          word_size = 8;
          break;
        case 'A':
          display_sca = true;
          break;
        case 'B':
          // Assume we receive GBT-SCA command instead of reply frames
          display_sca = true;
          display_sca_cmd = true;
          break;
        case 'c':
          display_data_chunks = false;
          display_chunks = false;
          break;
        case 'D':
          display_data_chunks = true;
          display_chunks = false;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'e':
          // Elink number to filter out from the data
          unsigned int x;
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elink_filter = x;
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
        case 'I':
          use_intrpt = true;
          break;
        case 'T':
          timestamp = true;
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

  FlxReceiver flxReceiver( cardnr, 64*1024*1024, dma_index );
  if( flxReceiver.hasError() )
    {
      cout << "### " << flxReceiver.errorString() << endl;
      flxReceiver.stop();
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", " << flxReceiver.infoString() << endl;
  cout << "Using DMA #" << dma_index;
  if( use_intrpt )
    cout << " interrupt-driven" << endl;
  else
    cout << " polling" << endl;

  if( flxReceiver.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  flxReceiver.setFanOutForDaq();
  flxReceiver.setUseInterrupt( use_intrpt );

  cout << "Display data from E-link ";
  if( elink_filter >= 0 )
    cout << hex << setw(3) << setfill('0') << elink_filter << endl;
  else
    cout << ": all" << endl;

  if( display_chunks )
    {
      cout << "Chunk types: BOTH=\"<<\", FIRST=\"++\", LAST=\"&&\", "
           << "MIDDLE=\"==\", TIMEOUT=\"]]\", NULL=\"@@\",\n"
           << "OUTOFBAND=\"##\" and \"TEC\" for chunk "
           << "truncation/error/CRCerr.\n";
    }

  std::signal( SIGINT, sigint_handler );

  FlxParser  fparser;
  i64        nbytes, i, elinknr, blocknr = 0;
  uint8_t   *block;
  int        blocksize = flxReceiver.blockSize();
  fparser.setReceiver( &flxReceiver ); // Sets blocksize and trailer format
  while( NotInterrupted )
    {
      if( flxReceiver.hasData() )
        {
          if( timestamp )
            {
              uint64_t t = flxReceiver.timestamp();
              uint64_t s = t/1000000000;
              uint64_t h = s/3600;
              // Hours, minutes, seconds, microseconds
              cout << setfill('0')
                   << " T=" << h << ":" << setw(2) << (s - h*3600)/60
                   << ":" << setw(2) << (s % 60)
                   << "." << setw(6) << (t - s*1000000000 + 500)/1000 << endl;
            }

          nbytes = flxReceiver.bytesAvailable();
          for( i=0; i<nbytes/blocksize; ++i )
            {
              block   = (uint8_t *) flxReceiver.data();
              elinknr = fparser.blockElinkNr( block );
              if( elink_filter < 0 || elinknr == elink_filter )
                {
                  if( display_data_chunks )
                    // Display complete user data chunks only
                    // (temporarily store FIRST and MIDDLE subchunks)
                    fparser.displayDataChunks( block, blocknr,
                                               display_sca, -1,
                                               word_size,
                                               display_sca_cmd );
                  else if( display_chunks )
                    // Display FELIX format (sub)chunks
                    fparser.displayChunks( block, blocknr,
                                           display_chunk_sz,
                                           display_timeout,
                                           display_timeout_data,
                                           display_sca, false, -1,
                                           word_size,
                                           display_sca_cmd );
                  else
                    // Display block's raw bytes,
                    // preceded by interpreted block header (first 4 bytes)
                    fparser.displayBlock( block, blocknr, 0, 0, -1,
                                          word_size );
                }
              flxReceiver.updateBytesConsumed( blocksize );
              ++blocknr;
            }
          //flxReceiver.updateBytesConsumed( nbytes );
        }
      else
        {
          // Doze off briefly
          usleep( 5000 );
          //sleep( 1 );
          //cout << flxReceiver.bytesReceived() << endl;
        }
      cout << flush;
    }

  flxReceiver.stop();
  if( !NotInterrupted )
    cout << " Interrupted..." << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fedump version " << hex << VERSION_ID << dec << endl <<
    "Dump selected E-link chunk data (optionally block-by-block)\n"
    "received from an FLX-device to screen.\n"
    "Chunk types are delimited by:\n"
    "BOTH=\"<<\", FIRST=\"++\", LAST=\"&&\", MIDDLE=\"==\", "
    "TIMEOUT=\"]]\", NULL=\"@@\", OUTOFBAND=\"##\"\n"
    "Usage:\n"
    " fedump [-h|V] [-A|B] [-c|D] [-d <devnr>] [-e <elink>] "
    "[-i <dma>] [-I] [-0|O] [-2|4|8]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -A         : Interpret chunks that could be GBT-SCA (reply) frames or\n"
    "               TTCtoHost frames.\n"
    "  -B         : Interpret chunks that could be GBT-SCA (command) frames.\n"
    "  -c         : Display data 'raw', block-by-block (default: chunk data).\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -D         : Display only whole data chunks, "
    "i.e. the user's data frames.\n"
    "  -e <elink> : E-link number (hex) to filter out for display "
    "(default: no filter).\n"
    "  -i <dma>   : FLX-device DMA controller to use (default: 0).\n"
    "  -I         : Use interrupt to receive data (default: polling).\n"
    "  -0         : Do not display time-out chunkdata (zeroes).\n"
    "  -O         : Do not display time-out chunks at all.\n"
    "  -2         : Display data as 2-byte words (little-endian).\n"
    "  -4         : Display data as 4-byte words (little-endian).\n"
    "  -8         : Display data as 8-byte words (little-endian).\n";
}

// ----------------------------------------------------------------------------
