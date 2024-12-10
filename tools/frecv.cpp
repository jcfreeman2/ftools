/* Receive and process FELIX user data (framework code).
*/
#include <iostream>
#include <iomanip>
using namespace std;
#include <csignal>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "version.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x19020600;

bool process_dataframe( uint32_t framesize, uint8_t *framedata,
                        int elinknr, int errbits );
void display_dataframe( uint32_t framesize, uint8_t *framedata,
                        int elinknr, int errbits );

// Histogramming chunk sizes

const int MAX_SZ   = 8192;
uint64_t ChunkCntr = 0;
uint64_t ErrCntr   = 0;
uint32_t Hmin      = MAX_SZ;
uint32_t Hmax      = 0;
uint64_t Histo[MAX_SZ];

void display_chunksize_histo( int );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int   opt;
  int   cardnr = 0;
  // Display something every 'loginterval' fragments received
  uint64_t loginterval = 1000000;
  // Display frame data or not, in case of error
  bool  display_frame = false;

  // At <Ctrl-C> dump the gathered chunksize histogram entries
  signal( SIGINT, display_chunksize_histo );

  // Parse options
  while( (opt = getopt(argc, argv, "Dd:hl:V")) != -1 )
    {
      switch( opt )
        {
        case 'D':
          display_frame = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'h':
          usage();
          return 0;
        case 'l':
          if( sscanf( optarg, "%lu", &loginterval ) != 1 )
            arg_error( 'l' );
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

  FlxReceiver *flxReceiver = new FlxReceiver( cardnr );
  if( flxReceiver->hasError() )
    {
      cout << "### " << flxReceiver->errorString() << endl;
      flxReceiver->stop();
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", firmw " << flxReceiver->firmwareVersion()
       <<  " (cmem bufsize=" << flxReceiver->bufferSize() << ")" << endl;

  if( flxReceiver->fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  flxReceiver->setFanOutForDaq();

  uint8_t       *framedata = 0;
  uint32_t       framesize;
  int            timeout_us;
  int            elinknr_filter;
  int            elinknr;
  int            errbits;
  uint64_t       burstcount;
  uint64_t       framecount;
  uint64_t       logcount;
  bool           run;
  FlxDataChecker flxChecker;
  flxChecker.setReceiver( flxReceiver );
  timeout_us     = 10000; // Time-out after 10 ms
  elinknr_filter = -1;    // No filter on E-link number: receive from all
  framecount     = 0;     // Total number of frames (chunks) received
  logcount       = loginterval;
  run            = true;
  while( run )
    {
      burstcount = 0;
      flxChecker.startRecvTime(); // Reset time-out on receiving data non-stop
      while( flxChecker.nextChunkRecvd( &framedata, &framesize, timeout_us,
                                        elinknr_filter, &elinknr, &errbits ) )
        {
          if( framesize == 0 ) continue;
          ++burstcount;
          ++framecount;
          ++ChunkCntr;

          // ==> USER CODE: PROCESS THIS DATA FRAME (or: chunk)
          // ('framesize' bytes pointed to by 'framedata',
          //  orginating from E-link number 'elinknr')
          if( !process_dataframe( framesize, framedata, elinknr, errbits ) )
            {
              cout << "###ERROR @Frame=" << framecount
                   << " error-bits=0x" << hex << errbits << dec
                   << " (buf=" << flxReceiver->bufferFullPercentage()
                   << "%)" << endl;

              if( display_frame )
                display_dataframe( framesize, framedata, elinknr, errbits );

              //flxReceiver->stop();
              //run = false;
            }

          if( framecount >= logcount )
            {
              cout << "@Frame " << framecount
                   << " (buf=" << flxReceiver->bufferFullPercentage()
                   << "%)" << endl;

              logcount += loginterval; // Next framecount to display something
            }
        }

      if( burstcount != 0 )
        {
          // Display total number of frames (chunks) processed so far
          cout << "Burst of " << burstcount
               << ": Frames=" << framecount << endl;
        }
    }

  flxReceiver->stop();
  return 0;
}

// ----------------------------------------------------------------------------

bool process_dataframe( uint32_t framesize, uint8_t *framedata,
                        int elinknr, int errbits )
{
  if( errbits != 0 )
    {
      ++ErrCntr;
      return false;
    }

  // Check size: here, histogram it
  if( framesize < MAX_SZ )
    {
      ++Histo[framesize];
      if( framesize < Hmin )
        Hmin = framesize;
      if( framesize > Hmax )
        Hmax = framesize;
    }
  else
    {
      ++Histo[0];
    }

  // Check data content
  // ...

  return true;
}

// ----------------------------------------------------------------------------

void display_dataframe( uint32_t framesize, uint8_t *framedata,
                        int elinknr, int errbits )
{
  // Display frame data bytes
  cout << "Elink=" << setw(3) << hex << setfill('0') << elinknr << dec
       << " Frame size=" << framesize;
  if( errbits != 0 )
    {
      cout << " ERR=";
      if( errbits & 1 ) cout << " TRUNC";
      if( errbits & 2 ) cout << " ERR";
      if( errbits & 4 ) cout << " CRCERR";
    }
  cout << ": " << endl << hex;
  uint32_t i;
  for( i=0; i<framesize; ++i )
    {
      cout << setw(2) << (uint32_t) framedata[i] << " ";
      if( (i & 0x1F) == 0x1F ) cout << endl;
    }
  if( (i & 0x1F) != 0 ) cout << endl;
  cout << dec;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "frecv version " << hex << VERSION_ID << dec << endl <<
    "Receive and process FELIX user data (framework code).\n"
    "Usage: frecv [-h|V] [-d <devnr>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device number to receive data from (default: 0).\n"
    "  -l <cntr>  : Display a message every 'cntr' fragments received "
    "(default: 100000).\n";
}

// ----------------------------------------------------------------------------

void display_chunksize_histo( int )
{
  cout << "\nRESULTS:" << dec << endl;
  if( Hmin <= Hmax )
    {
      cout << "Total chunks: " << ChunkCntr << endl;
      cout << "Chunk sizes : " << Hmin << " <= Size <= " << Hmax << endl;
      if( Histo[0] > 0 )
        cout << "Chunks>" << MAX_SZ << " :" << Histo[0] << endl;
      cout << "Chunk errors: " << ErrCntr << endl;
      cout << "Histogram:" << endl
           << "Size  Count" << endl;
      for( uint32_t i=Hmin; i<=Hmax; ++i )
        cout << setw(4) << i << "  " << Histo[i] << endl;
    }
  else
    {
      cout << "<NONE>" << endl;
    }
  exit( 0 );
}

// ----------------------------------------------------------------------------
