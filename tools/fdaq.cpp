#include <iostream>
#include <iomanip>
#include <csignal>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxDaq.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x22020100; // Add ctrl-c handler
//const int VERSION_ID = 0x21120700; // Add option -n (display chunk counts)
//const int VERSION_ID = 0x21031200; // Invert -R logic
//const int VERSION_ID = 0x19120500; // Invert -C logic
//const int VERSION_ID = 0x19091900; // Output in columns (option -o to disable)
//const int VERSION_ID = 0x19050700; // Add option -C (check for chunk CRC err)
//const int VERSION_ID = 0x19041700; // Fix online check null trailer issue
//const int VERSION_ID = 0x18121100; // Change dbg_mode slightly
//const int VERSION_ID = 0x18070501; // File size up to 16GB (default 4GB)
//const int VERSION_ID = 0x18070500; // Additional error counters (while flush)
//const int VERSION_ID = 0x18052400; // Option -R added (no reset/flush)
//const int VERSION_ID = 0x18013100; // Added firmware mode check
//const int VERSION_ID = 0x17103100; // Option -w added (file write chunksize)
//const int VERSION_ID = 0x17070700; // Option -T added (no datetime file names)
//const int VERSION_ID = 0x17062100; // Option -Z added (backpressure)
//const int VERSION_ID = 0x17061600; // Max buffer size increased to 32GB
//const int VERSION_ID = 0x17032900; // Interrupt-driven; some fixes
//const int VERSION_ID = 0x16120200; // Tagged version
//const int VERSION_ID = 0x16103100; // Fix chunk trailer check
//const int VERSION_ID = 0x16100500; // Check chunk trailers in non-save mode
//const int VERSION_ID = 0x16071100; // Lock bits for FO_SEL registers
//const int VERSION_ID = 0x16052600;
//const int VERSION_ID = 0x16051100;
//const int VERSION_ID = 0x16051000;
//const int VERSION_ID = 0x16042200;
//const int VERSION_ID = 0x16032201;

bool NotInterrupted = true;
void sigint_handler( int signal )
{
  NotInterrupted = false;
}

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int      opt;
  bool     dbg_mode     = false;
  int      cardnr       = 0;
  int      blocksize    = -1;
  int      dma_index    = 0;
  bool     use_intrpt   = false;
  uint64_t buffersize   = 0;
  uint64_t filesize     = 0;
  uint64_t filewritesz  = 0;
  bool     per_elink    = false;
  bool     use_emulator = false, emulator_external = false;
  int      secs_of_daq  = 1;
  int      runnr        = 0;
  bool     recording    = true;
  bool     use_datetime = true;
  bool     backpressure = false; // For back-pressure tests
  bool     reset        = false;
  bool     check_crc    = true;
  bool     column_output= true;
  bool     display_chunk_counts = false;
  string   filename;

  // Parse the options
  while( (opt = getopt(argc, argv, "b:Cd:eEDf:hi:Inor:Rt:TVXx:w:Z")) != -1 )
    {
      switch( opt )
        {
        case 'b':
          // Buffer size in MBytes
          if( sscanf( optarg, "%lu", &buffersize ) != 1 )
            arg_error( 'b' );
          buffersize *= 1024ULL*1024ULL;
          if( buffersize > 32768ULL*1024ULL*1024ULL )
            {
              buffersize = 32768ULL*1024ULL*1024ULL;
              cout << "(buffer size capped to 32GB)" << endl;
            }
          break;
        case 'C':
          check_crc = false;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          dbg_mode = true;
          break;
        case 'E':
          emulator_external = true;
        case 'e':
          use_emulator = true;
          break;
        case 'f':
          if( sscanf( optarg, "%lu", &filesize ) != 1 )
            arg_error( 'f' );
          filesize *= 1024ULL*1024ULL;
          if( filesize > 16384ULL*1024ULL*1024ULL )
            {
              filesize = 16384ULL*1024ULL*1024ULL;
              cout << "(file size capped to 16GB)" << endl;
            }
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          // DMA controller to use
          if( sscanf( optarg, "%d", &dma_index ) != 1 )
            arg_error( 'i' );
          break;
        case 'I':
          use_intrpt = true;
          break;
        case 'n':
          display_chunk_counts = true;
          break;
        case 'o':
          column_output = false;
          break;
        case 'r':
          if( sscanf( optarg, "%d", &runnr ) != 1 )
            arg_error( 'r' );
          break;
        case 'R':
          reset = true;
          break;
        case 't':
          if( sscanf( optarg, "%d", &secs_of_daq ) != 1 )
            arg_error( 't' );
          break;
        case 'T':
          use_datetime = false;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'w':
          if( sscanf( optarg, "%lu", &filewritesz ) != 1 )
            arg_error( 'w' );
          filewritesz *= 1024ULL*1024ULL;
          if( filewritesz > 512ULL*1024ULL*1024ULL )
            {
              filewritesz = 512ULL*1024ULL*1024ULL;
              cout << "(file write chunksize capped to 512MB)" << endl;
            }
          break;
        case 'X':
          per_elink = true;
          break;
        case 'x':
          // FLX-device unit block size in KByte
          if( sscanf( optarg, "%d", &blocksize ) != 1 )
            arg_error( 'x' );
          if( blocksize < 1 || blocksize > 16 )
            arg_range( 'x', 1, 16 );
          blocksize *= 1024;
          break;
        case 'Z':
          backpressure = true;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // File name (base name)
  if( optind < argc )
    {
      filename = string( argv[optind] );
      recording = true;
    }
  else
    {
      // Just check received data blocks, and flush 'm..
      recording = false;
      cout << "Consume FLX-device data while checking the data "
           << "(blockheader and trailers)," << endl;
      if( dbg_mode )
        cout << "counts errors including chunk truncation, "
             << "continuing when the memory buffer overflows.";
      else
        //cout << "halts as soon as an error is encountered "
        //       << "or the memory buffer is near overflowing.";
        cout << "counts errors including chunk truncation, "
             << "halts when the memory buffer is near overflowing.";
      cout << endl;
      if( check_crc )
        cout << "Also counts chunk CRC errors." << endl;
    }

  FlxDaq flxdaq( cardnr, buffersize, dma_index, reset );
  if( flxdaq.hasError() )
    {
      cout << "### " << flxdaq.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", " << flxdaq.infoString() << endl;

  flxdaq.setUseInterrupt( use_intrpt );
  flxdaq.enableBackpressure( backpressure );
  flxdaq.setFileNameDateTime( use_datetime );

  if( blocksize > 0 )
    flxdaq.setBlockSize( blocksize );

  if( filesize > 0 )
    flxdaq.setFileMaxSize( filesize );

  if( filewritesz > 0 )
    flxdaq.setFileChunkSize( filewritesz );

  //if( dbg_mode )
  //  cout << "**DEBUG ON**" << endl;

  if( recording )
    {
      if( dbg_mode )
        cout << "Before startRec: " << flxdaq.debugString();

      if( !flxdaq.startRecording( filename, runnr, per_elink ) )
        cout << "### " << flxdaq.errorString() << endl;

      if( dbg_mode )
        cout << "After startRec: " << flxdaq.debugString();
    }

  if( flxdaq.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;

  // Check for firmware mode dependent issues
  int fw_mode = flxdaq.firmwareMode();
  if( use_emulator && emulator_external && fw_mode == 1 )
    {
      cout << "### WARNING: FULLMODE firmware, "
           << "you selected EXTERNAL emu, using INTERNAL" << endl;
      emulator_external = false;
    }
  else if( fw_mode == -1 )
    {
      cout << "### WARNING: unknown firmware mode" << endl;
    }

  cout << "**START";
  if( use_emulator )
    {
      cout << "(emulator";
      if( emulator_external )
        cout << "-ext";
      cout << ")";
      flxdaq.startEmulator( emulator_external );
    }
  else
    {
      flxdaq.setFanOutForDaq();
    }
  cout << "** using DMA #" << dma_index;
  if( use_intrpt )
    cout << " interrupt-driven" << endl;
  else
    cout << " polling" << endl;

  cout << fixed << setprecision( 1 ); // For rates output

  std::signal( SIGINT, sigint_handler );

  int64_t bytes_received, bytes_received_prev = 0;
  int64_t bytes_recorded, bytes_recorded_prev = 0;
  int64_t block_errors = 0, trailer_errors = 0, crc_errors = 0;
  double  mb_from_flx, mb_to_file;
  int     halfsecs = 0;// bufferwrap_count = 0;
  while( halfsecs < 2*secs_of_daq && NotInterrupted )
    {
      usleep( 500000 ); // Half a second

      if( (halfsecs & 1) == 0 )
        if( flxdaq.bufferFullOccurred() )
          {
            //if( dbg_mode && recording )
            //  cout << flxdaq.debugString();

            if( !dbg_mode )
              {
                cout << "## OVERFLOW ##";
                if( flxdaq.bufferFull() )
                  cout << " (buffer full)";
                cout << endl;

                // Finish when this happens, unless in debug mode
                halfsecs = 2*secs_of_daq;
              }
          }

      ++halfsecs;
      // Display status once every full second
      if( (halfsecs & 1) == 0 )
        {
          bytes_received = flxdaq.bytesReceivedCount();
          bytes_recorded = flxdaq.bytesRecordedCount();

          // Calculate data rates
          mb_from_flx = (double) (bytes_received-bytes_received_prev) / 1.0e6;
          mb_to_file  = (double) (bytes_recorded-bytes_recorded_prev) / 1.0e6;

          bytes_received_prev = bytes_received;
          bytes_recorded_prev = bytes_recorded;

          // Display elapsed time, data rates, data totals
          // and data buffer status
          if( column_output )
            {
              // Display column headers ?
              if( ((halfsecs/2) % 20) == 1 )
                {
                  cout << "  Secs | Recvd[MB/s] | File[MB/s] | Total[(M)B] | "
                    "Rec[(M)B] | Buf[%] | Wraps" << endl;
                  cout << "-------|-------------|------------|-------------|-"
                    "----------|--------|-------" << endl;
                }

              cout << setw(6)  << halfsecs/2  << "   "
                   << setw(11) << mb_from_flx << "   "
                   << setw(10) << mb_to_file  << "   ";

              cout << setw(11);
              if( bytes_received < 1000000 )
                cout << bytes_received;
              else
                cout << (double) bytes_received/1000000.0;

              cout << "   " << setw(9);
              if( bytes_recorded < 1000000 )
                cout << bytes_recorded;
              else
                cout << (double) bytes_recorded/1000000.0;

              int wraps = flxdaq.bufferWrapCount();
              cout << "   " << setw(6);
              cout << flxdaq.bufferFullPercentage();
              cout << "   " << setw(5) << wraps;
              cout << endl;
            }
          else
            {
              cout << "->" << setw(2) << halfsecs/2 << " sec, "
                   << "Rates: "
                   << "recv " << setw(5) << mb_from_flx << " MB/s, "
                   << "file " << setw(5) << mb_to_file << " MB/s; ";

              cout << "Total: recvd ";
              if( bytes_received < 1000000 )
                cout << bytes_received << " B, ";
              else
                cout << (double) bytes_received/1000000.0 << " MB, ";
              cout << "file ";
              if( bytes_recorded < 1000000 )
                cout << bytes_recorded << " B; ";
              else
                cout << (double) bytes_recorded/1000000.0 << " MB, ";

              int wraps = flxdaq.bufferWrapCount();
              cout << "Buffer: " << setw(2)
                   << flxdaq.bufferFullPercentage() << "%, "
                   << "wraps " << wraps
                //<< " (bytes available: " << flxdaq.bytesAvailable() << ")"
                   << endl;
            }
          //if( dbg_mode && bufferwrap_count != wraps )
          //  {
          //    if( mb_from_flx == 0.0 )
          //      cout << flxdaq.debugString();
          //    bufferwrap_count = wraps;
          //  }

          if( !recording )
            {
              if( true )//dbg_mode )
                {
                  if( flxdaq.blockErrors() > block_errors ||
                      flxdaq.trailerErrors() > trailer_errors ||
                      (check_crc && flxdaq.crcErrors() > crc_errors) )
                    {
                      cout << "   ### Blocks " << flxdaq.blocksChecked()
                           << " Errors: header=" << flxdaq.blockErrors()
                           << " trailer=" << flxdaq.trailerErrors()
                           << " (trunc=" << flxdaq.truncErrors()
                           << ",err=" << flxdaq.errErrors()
                           << ",length=" << flxdaq.lengthErrors()
                           << ",type=" << flxdaq.typeErrors() << ")";
                      if( check_crc )
                        cout << " crc=" << flxdaq.crcErrors();
                      cout << endl;
                      block_errors   = flxdaq.blockErrors();
                      trailer_errors = flxdaq.trailerErrors();
                      crc_errors     = flxdaq.crcErrors();
                    }
                }
              else
                {
                  if( flxdaq.blockErrors() > 0 || flxdaq.trailerErrors() > 0 )
                    halfsecs = 2*secs_of_daq; // Finish on error encountered...
                }
            }
        }
    }

  if( !NotInterrupted )
    cout << "Interrupted..." << endl;

  if( use_emulator )
    flxdaq.stopEmulator();
  cout << "**STOP**" << endl;

  if( recording )
    {
      if( !flxdaq.stopRecording() )
        cout << "### " << flxdaq.errorString() << endl;

      if( dbg_mode )
        cout << "After stopRec: " << flxdaq.debugString();

      // Display totals
      cout << "-> Totals: "
           << "Recvd " << flxdaq.bytesReceivedCount()/1000000 << " MB, "
           << "File " << flxdaq.bytesRecordedCount()/1000000 << " MB "
           << "(last " << flxdaq.bytesWrittenCount()/1000000 << " MB), "
           << flxdaq.fileCount() << " files" << endl;
      cout << "-> File  : " << flxdaq.fileName() << endl;
    }
  else
    {
      // Display results of 'data block check'
      cout << "-> Data checked: "
           << "Blocks " << flxdaq.blocksChecked() << ", "
           << "Errors: header=" << flxdaq.blockErrors()
           << " trailer=" << flxdaq.trailerErrors();
      if( flxdaq.blockErrors() > 0 ||
          flxdaq.trailerErrors() > 0 )
        {
          cout << setfill('0');
          cout << ", ErrorI " << flxdaq.blockErrorIndex() << endl;
          //cout << "Data blocks around block-with-error at "
          //     << hex << setw(4) << flxdaq.blockErrorDataSize()/2 << ":";

          //cout << hex << setfill('0');
          unsigned char *errdata = flxdaq.blockErrorData();
          FlxDataChecker checker;
          checker.setBlockSize( flxdaq.blockSize() );
          checker.setTrailerIs32Bit( flxdaq.trailerIs32Bit() );
          cout << "\nFirst block with error, raw:" << endl;
          checker.displayBlock( errdata );
          cout << "\nFirst block with error, chunk data:" << endl;
          checker.displayChunks( errdata );
        }
      cout << endl;

      if( display_chunk_counts )
        // Display compiled E-link chunk counts (if any)
        flxdaq.displayChunkCounts();
    }

  cout << "Exiting.." << endl;
  flxdaq.stop();

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fdaq version " << hex << VERSION_ID << dec << endl <<
    "Stream data from FLX-device to file(s). Whenever the set maximum file size\n"
    "is exceeded a new file is created. Every second a status line\n"
    "with data rates, data totals and memory buffer status is displayed.\n"
    "(NB: if no filename is provided all data is consumed while checking "
    "the data blocks,\n"
    "     i.e. blockheader and (sub)chunk trailers; chunk truncation and "
    "error counts are reported.)\n\n"

    "Usage: fdaq [-h|V] [-D] [-d <devnr>] [-b <size>] [-e|E] [-f <size>]\n"
    "            [-i <dma>] [-I] [-r <runnr>] [-t <secs>] [-n] [-C] [-R] [-T] "
    "[-X] [-o] [-x <kbyte>]\n"
    "            [<filename-base>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -C         : Do *not* check for presence of data chunk CRC errors "
    "(when not writing to file).\n"
    "  -D         : Debug mode on, i.e. output some additional info;\n"
    "               continue when memory buffer overflows.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -b <size>  : DMA (cmem_rcc) memory buffer size to use, in MB "
    "(default 1024, max 32768).\n"
    "  -e|E       : Enable FLX-device data generator, internal (e) or\n"
    "               external (E) (default: false).\n"
    "  -f <size>  : Maximum file size, in MB (default 4096, max 16384).\n"
    "  -i <dma>   : FLX-device DMA controller to use (default: 0).\n"
    "  -I         : use interrupt to receive data (default: polling)\n"
    "  -n         : Display chunk count per e-link\n"
    "               (when exiting, when not writing to file).\n"
    "  -o         : Display status output not in columns (but line-by-line).\n"
    "  -r <runnr> : Run number to use in file names (default: none).\n"
    "  -R         : Reset DMA at startup (default: 'soft reset' only).\n"
    "  -t <secs>  : Number of seconds to do acquisition (default: 1).\n"
    "  -T         : Do NOT add datetime as part of file names.\n"
    "  -X         : Stream data from individual e-links to separate files "
    "(default: false).\n"
    "  -x <kbyte> : Set size of FLX-device unit data block, in KByte\n"
    "               (forced; normally read from FLX-device itself).\n"
    " <filename-base>: Name to be combined with datetime+runnumber+counter "
    "of files created\n"
    "                  (unless option -T is given)\n";
}

// ----------------------------------------------------------------------------
