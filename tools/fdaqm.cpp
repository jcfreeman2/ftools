#include <iostream>
#include <iomanip>
#include <vector>
#include <csignal>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxDaq.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x22021600; // Invert -R logic
//const int VERSION_ID = 0x22020100; // Add ctrl-c handler
//const int VERSION_ID = 0x21120700; // Add option -n (display chunk counts)
//const int VERSION_ID = 0x19120500; // Invert -C logic
//const int VERSION_ID = 0x19091900; // Output in columns (option -o to disable)
//const int VERSION_ID = 0x18083000;

bool NotInterrupted = true;
void sigint_handler( int signal )
{
  NotInterrupted = false;
}

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int      opt;
  int      cardnr       = 0;
  int      dma_index    = 0;
  int      dma_cnt      = 1;
  int      card_cnt     = 1;
  int      blocksize    = -1;
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
  while( (opt = getopt(argc, argv, "b:c:C:d:eEf:hi:Inor:Rt:TVXx:w:Z")) != -1 )
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
        case 'c':
          if( sscanf( optarg, "%d", &dma_cnt ) != 1 )
            arg_error( 'c' );
          if( dma_cnt < 1 )
            arg_range( 'c', 1, 8 );
          break;
        case 'C':
          if( sscanf( optarg, "%d", &card_cnt ) != 1 )
            arg_error( 'C' );
          if( card_cnt < 1 )
            arg_range( 'C', 1, 8 );
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
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
          if( blocksize < 1 || blocksize > (int) buffersize/1024 )
            arg_range( 'x', 1, buffersize/1024 );
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
           << "(blockheader and trailers)" << endl;
      if( check_crc )
        cout << "Also counts chunk CRC errors." << endl;
    }

  // Instantiate 'dma_cnt' FlxDaq objects on each of 'card_cnt' FLX-devices
  vector<FlxDaq *> flxdaq;
  for( int c=0; c<card_cnt; ++c )
    {
      bool reset_it = reset;
      for( int d=0; d<dma_cnt; ++d )
        {
          FlxDaq *f = new FlxDaq( cardnr+c, buffersize, dma_index+d, reset_it );

          // Apply reset once per card at most..
          reset_it = false;

          if( f->hasError() )
            {
              cout << "### Dev" << cardnr+c << "-DMA" << dma_index+d
                   << ": " << f->errorString() << endl;
              return 1;
            }

          f->setUseInterrupt( use_intrpt );
          f->enableBackpressure( backpressure );
          f->setFileNameDateTime( use_datetime );

          if( filesize > 0 )
            f->setFileMaxSize( filesize );

          if( filewritesz > 0 )
            f->setFileChunkSize( filewritesz );

          if( blocksize > 0 )
            f->setBlockSize( blocksize );

          cout << "Opened FLX-device " << cardnr+c
               << ", " << f->infoString() << endl;

          flxdaq.push_back( f );
        }
    }

  if( recording )
    {
      for( int c=0; c<card_cnt; ++c )
        for( int d=0; d<dma_cnt; ++d )
          {
            string fname = (filename + (char) ('0'+cardnr+c) +
                            (char) ('0'+dma_index+d));
            if( !flxdaq[c*dma_cnt+d]->startRecording( fname, runnr,
                                                      per_elink ) )
              cout << "### " << flxdaq[c*dma_cnt+d]->errorString() << endl;
          }
    }

  for( int c=0; c<card_cnt; ++c )
    if( flxdaq[c*dma_cnt]->fanOutLocked() )
      cout <<  "**NB**: Device" << c
           <<  "FanOut-Select registers are locked!" << endl;

  // Check for firmware mode dependent issues
  int fw_mode = flxdaq[0]->firmwareMode();
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
      for( int c=0; c<card_cnt; ++c )
        flxdaq[c*dma_cnt]->startEmulator( emulator_external );
    }
  else
    {
      for( int c=0; c<card_cnt; ++c )
        flxdaq[c*dma_cnt]->setFanOutForDaq();
    }
  cout << "**" << endl;
  for( int i=0; i<dma_cnt; ++i )
    {
      cout << "** using DMA #" << dma_index+i;
      if( use_intrpt )
        cout << " interrupt-driven" << endl;
      else
        cout << " polling" << endl;
    }

  cout << fixed << setprecision( 1 ); // For rates output

  std::signal( SIGINT, sigint_handler );

  int64_t bytes_received;
  int64_t bytes_received_prev[8] = {0,0,0,0,0,0,0,0};
  int64_t bytes_recorded;
  int64_t bytes_recorded_prev[8] = {0,0,0,0,0,0,0,0};
  int64_t block_errors[8]        = {0,0,0,0,0,0,0,0};
  int64_t trailer_errors[8]      = {0,0,0,0,0,0,0,0};
  int64_t crc_errors[8]          = {0,0,0,0,0,0,0,0};
  double  mb_from_flx, mb_to_file;
  int     halfsecs = 0;
  while( halfsecs < 2*secs_of_daq && NotInterrupted )
    {
      usleep( 500000 ); // Half a second

      if( (halfsecs & 1) == 0 )
        for( FlxDaq *f : flxdaq )
          if( f->bufferFullOccurred() )
            {
              cout << "## OVERFLOW ##";
              if( f->bufferFull() )
                cout << " (buffer full)";
              cout << endl;
            }

      ++halfsecs;
      // Display status once every full second
      if( (halfsecs & 1) == 0 )
        for( int i=0; i<card_cnt*dma_cnt; ++i )
          {
            int dev = i/dma_cnt;
            int dma = i-dev*dma_cnt;

            bytes_received = flxdaq[i]->bytesReceivedCount();
            bytes_recorded = flxdaq[i]->bytesRecordedCount();

            // Calculate data rates
            mb_from_flx = (double)(bytes_received-bytes_received_prev[i])/1.0e6;
            mb_to_file  = (double)(bytes_recorded-bytes_recorded_prev[i])/1.0e6;

            bytes_received_prev[i] = bytes_received;
            bytes_recorded_prev[i] = bytes_recorded;

            // Display elapsed time, data rates, data totals
            // and data buffer status
            if( column_output )
              {
                // Display column headers ?
                if( ((halfsecs/2) % 20) == 1 && i == 0 )
                  {
                    cout << "  Secs | d-D | Recvd[MB/s] | File[MB/s] | "
                      "Total[(M)B] | Rec[(M)B] | Buf[%] | Wraps" << endl;
                    cout << "-------|-----|-------------|------------|-"
                      "------------|-----------|--------|-------" << endl;
                  }
                cout << setw(6)  << halfsecs/2  << " "
                     << setw(3) << dev << "-" << dma << "   "
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

                int wraps = flxdaq[i]->bufferWrapCount();
                cout << "   " << setw(6);
                cout << flxdaq[i]->bufferFullPercentage();
                cout << "   " << setw(5) << wraps;
                cout << endl;
              }
            else
              {
                cout << "->" << setw(2) << halfsecs/2 << " sec, "
                     << "Rates #" << dev << "-" << dma << ": "
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

                int wraps = flxdaq[i]->bufferWrapCount();
                cout << "Buffer: " << setw(2)
                     << flxdaq[i]->bufferFullPercentage() << "%, "
                     << "wraps " << wraps
                     << endl;
              }

            if( !recording )
              {
                if( flxdaq[i]->blockErrors() > block_errors[i] ||
                    flxdaq[i]->trailerErrors() > trailer_errors[i] ||
                    (check_crc && flxdaq[i]->crcErrors() > crc_errors[i]) )
                  {
                    cout << "   ### @Dev-DMA=" << dev << "-" << dma
                         << " Blocks " << flxdaq[i]->blocksChecked()
                         << " Errors: header=" << flxdaq[i]->blockErrors()
                         << " trailer=" << flxdaq[i]->trailerErrors()
                         << " (trunc=" << flxdaq[i]->truncErrors()
                         << ",err=" << flxdaq[i]->errErrors()
                         << ",length=" << flxdaq[i]->lengthErrors()
                         << ",type=" << flxdaq[i]->typeErrors() << ")";
                    if( check_crc )
                      cout << " crc=" << flxdaq[i]->crcErrors();
                    cout << endl;
                    block_errors[i]   = flxdaq[i]->blockErrors();
                    trailer_errors[i] = flxdaq[i]->trailerErrors();
                    crc_errors[i]     = flxdaq[i]->crcErrors();
                  }
              }
          }
    }

  if( !NotInterrupted )
    cout << "Interrupted..." << endl;

  if( use_emulator )
    for( int c=0; c<card_cnt; ++c )
      flxdaq[c*dma_cnt]->stopEmulator();
  cout << "**STOP**" << endl;

  for( int i=0; i<card_cnt*dma_cnt; ++i )
    {
      int dev = i/dma_cnt;
      int dma = i-dev*dma_cnt;
      if( recording )
        {
          if( !flxdaq[i]->stopRecording() )
            cout << "### " << flxdaq[i]->errorString() << endl;
          
          // Display totals
          cout << "-> Totals @Dev-DMA=" << dev << "-" << dma << ": "
               << "Recvd " << flxdaq[i]->bytesReceivedCount()/1000000 << " MB, "
               << "File " << flxdaq[i]->bytesRecordedCount()/1000000 << " MB "
               << "(last " << flxdaq[i]->bytesWrittenCount()/1000000 << " MB), "
               << flxdaq[i]->fileCount() << " files" << endl;
          cout << "-> File  : " << flxdaq[i]->fileName() << endl;
        }
      else
        {
          // Display results of 'data block check'
          cout << "-> Data checked @Dev-DMA=" << dev << "-" << dma << ": "
               << "Blocks " << flxdaq[i]->blocksChecked() << ", "
               << "Errors: header=" << flxdaq[i]->blockErrors()
               << " trailer=" << flxdaq[i]->trailerErrors();
          if( flxdaq[i]->blockErrors() > 0 ||
              flxdaq[i]->trailerErrors() > 0 )
            {
              cout << setfill('0');
              cout << ", ErrorI " << flxdaq[i]->blockErrorIndex() << endl;
              //cout << "Data blocks around block-with-error at "
              //     << hex << setw(4) << flxdaq.blockErrorDataSize()/2 << ":";

              //cout << hex << setfill('0');
              unsigned char *errdata = flxdaq[i]->blockErrorData();
              FlxDataChecker checker;
              checker.setBlockSize( flxdaq[i]->blockSize() );
              checker.setTrailerIs32Bit( flxdaq[i]->trailerIs32Bit() );
              cout << "\nFirst block with error, raw:" << endl;
              checker.displayBlock( errdata );
              cout << "\nFirst block with error, chunk data:" << endl;
              checker.displayChunks( errdata );
            }
          cout << endl;

          if( display_chunk_counts )
            {
              cout << "-> Elink chunks @Dev-DMA="
                   << dev << "-" << dma << ":" << endl;
              // Display compiled E-link chunk counts (if any)
              flxdaq[i]->displayChunkCounts();
            }
        }
    }

  cout << "Exiting.." << endl;
  for( FlxDaq *f : flxdaq )
    f->stop();

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fdaqm version " << hex << VERSION_ID << dec << endl <<
    "Stream data from (multiple) FLX-device DMA controller(s) to file(s).\n"
    "Whenever the set maximum file size is exceeded a new file is created.\n"
    "Every second a status line with data rates, data totals and\n"
    "memory buffer status is displayed for each active DMA controller.\n"
    "(NB: if no filename is provided data is consumed while checking "
    "the data blocks,\n"
    "     i.e. blockheader and (sub)chunk trailers.)\n\n"

    "Usage: fdaqm [-h|V] [-D] [-d <devnr>] [-c <dcnt>] [-C <ccnt>] [-b <size>]"
    " [-e|E] [-f <size>]\n"
    "             [-i <dma>] [-I] [-r <runnr>] [-t <secs>] [-n] [-R] [-X] "
    "[-x <kbyte>]\n"
    "             [<filename-base>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -c <dcnt>  : Number of 'DAQ' instances to start up on consecutive DMA "
    "controllers (default:1).\n"
    "  -C <ccnt>  : Number of FLX-devices on which to start up 'DAQ' instances "
    "as set by option 'c' (default:1).\n"
    "  -d <devnr> : First FLX-device to use (default: 0).\n"
    "  -b <size>  : DMA (cmem_rcc) memory buffer size to use, in MB "
    "(default 1024, max 32768).\n"
    "  -e|E       : Enable FLX-device data generator, internal (e) or\n"
    "               external (E) (default: false).\n"
    "  -f <size>  : Maximum file size, in MB (default 1024, max 16384).\n"
    "  -i <dma>   : First FLX-device DMA controller to use (default: 0).\n"
    "  -I         : Use interrupt to receive data (default: polling)\n"
    "  -n         : Display chunk count per e-link\n"
    "               (when exiting, when not writing to file).\n"
    "  -r <runnr> : Run number to use in file names (default: none).\n"
    "  -R         : Reset DMA at startup (default: 'soft reset' only).\n"
    "  -t <secs>  : Number of seconds to do acquisition (default: 1).\n"
    "  -T         : Do NOT add datetime+counter as part of file names.\n"
    "  -X         : Stream data from individual e-links to separate files "
    "(default: false).\n"
    "  -x <kbyte> : Set size of FLX-device unit data block, in KByte\n"
    "               (forced; normally read from FLX-device itself).\n"
    " <filename-base>: Name to be combined with datetime+runnumber+counter "
    "of files created\n"
    "                  (unless option -T is given)\n";
}

// ----------------------------------------------------------------------------
