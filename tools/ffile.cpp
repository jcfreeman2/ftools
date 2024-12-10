/* Read and process FELIX user data chunks read from a file
   as produced by fdaq (framework code).
*/
#include <iostream>
#include <iomanip>
using namespace std;

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "version.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x17092800;

// Read a file in chunks of 8 MBytes
const int BUFSIZE = 8*1024*1024;

bool process_dataframe( uint32_t framesize, uint8_t *framedata, int errbits );
void display_dataframe( uint32_t framesize, uint8_t *framedata, int errbits );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    frames_to_process = 1;
  int    frames_to_skip    = 0;
  string filename;
  FILE  *fp;
  unsigned char *buffer = new unsigned char[BUFSIZE];

  // Parse options
  while( (opt = getopt(argc, argv, "F:S:hV")) != -1 )
    {
      switch( opt )
        {
        case 'F':
          // Number of frames to process
          if( sscanf( optarg, "%d", &frames_to_process ) != 1 )
            arg_error( 'F' );
          break;
        case 'S':
          // Number of frames to skip before starting processing
          if( sscanf( optarg, "%d", &frames_to_skip ) != 1 )
            arg_error( 'S' );
          break;
        case 'h':
          usage();
          return 0;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Get the file name
  if( optind < argc )
    {
      filename = string( argv[optind] );
    }
  else
    {
      cout << "### Filename missing.." << endl;
      return 1;
    }

  // Open the file
  if( (fp = fopen( filename.c_str(), "r" )) == NULL )
    {
      cout << "### Failed to open file " << filename << endl;
      return 1;
    }

  FlxDataChecker flxChecker;
  uint8_t       *block, *framedata;
  uint32_t       framesize;
  int            errbits;
  int            elinknr;
  int            framecnt     = 0;
  uint64_t       total_bytes  = 0;
  uint64_t       total_blocks = 0;
  size_t         nbytes, blocknr;
  int            blocksize    = 1024;
  flxChecker.setBlockSize( blocksize );
  //flxChecker.setTrailerIs32Bit( true/false );
  while( (nbytes = fread( buffer, 1, BUFSIZE, fp )) > 0 )
    {
      block = buffer;
      for( blocknr=0; blocknr<nbytes/blocksize; ++blocknr, block+=blocksize )
        {
          // Going to go through the chunks in this block
          if( !flxChecker.setBlockToHandle( block ) )
            {
              cout << "### Invalid block "
                   << total_blocks + blocknr << endl;
            }

          // Get the chunks/frames from this block one-by-one;
          // optional parameter 'errbits' returns the error bits set
          // for this chunk;
          // optional parameter 'elinknr' returns the E-link number
          // the chunk was received from
          while( flxChecker.nextChunkFromBlock( &framedata, &framesize,
                                                &errbits, &elinknr ) )
            {
              if( framesize == 0 ) continue;
              ++framecnt;

              if( frames_to_skip > 0 )
                {
                  --frames_to_skip;
                  continue;
                }

              if( frames_to_process > 0 )
                {
                  // ==> USER CODE: PROCESS THIS DATA FRAME
                  // ('framesize' bytes pointed to by 'framedata')
                  if( !process_dataframe( framesize, framedata, errbits ) )
                    {
                      display_dataframe( framesize, framedata, errbits );
                      return 1;
                    }
                  --frames_to_process;
                }
            }
        }
      total_bytes  += nbytes;
      total_blocks += nbytes / blocksize;
    }

  cout << "File " << filename << " contains "
       << total_bytes << " bytes ("
       << total_blocks << " FLX blocks +"
       << (total_bytes % blocksize) << ")" << endl
       << "  and " << framecnt << " frames" << endl;
  return 0;
}

// ----------------------------------------------------------------------------

bool process_dataframe( uint32_t framesize, uint8_t *framedata, int errbits )
{
  if( errbits != 0 )
    return false;

  // Check size
  // ...
  // Check data content
  // ...

  return true;
}

// ----------------------------------------------------------------------------

void display_dataframe( uint32_t framesize, uint8_t *framedata, int errbits )
{
  // Display frame data bytes
  cout << "Frame size=" << framesize;
  if( errbits != 0 )
    {
      cout << " ERR=";
      if( errbits & 1 ) cout << " TRUNC";
      if( errbits & 2 ) cout << " ERR";
      if( errbits & 4 ) cout << " CRCERR";
    }
  cout << ": " << endl << hex << setfill('0');
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
  cout << "ffile version " << hex << VERSION_ID << dec << endl <<
    "Read and process FELIX user data chunks read from a file\n"
    "as produced by fdaq (framework code).\n"
    "Usage: ffile [-h|V] [-F <frames>] [-S <frames>] <filename>\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -F <frames>: Dump <frames> data frames to display.\n"
    "  -S <frames>: Skip <frames> of data frames before starting display.\n"
    " <filename>  : Name of file containing data to process.\n";
}

// ----------------------------------------------------------------------------
