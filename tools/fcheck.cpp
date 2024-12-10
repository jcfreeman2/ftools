#include <iostream>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x22022200; // Can process multiple files sequentially
//const int VERSION_ID = 0x20121600; // Small bug fix in FlxDataChecker.cpp
//const int VERSION_ID = 0x20061700; // Add option -O
//const int VERSION_ID = 0x20050100; // TTCtoHost message can be 26 or 20 bytes
//const int VERSION_ID = 0x20021700; // Options -2|4|8: display multi-byte words
//const int VERSION_ID = 0x19081600; // Classify E-links in width
//const int VERSION_ID = 0x19041700; // Show link-group-path in error messages
//const int VERSION_ID = 0x18100100;
//const int VERSION_ID = 0x18092000;
//const int VERSION_ID = 0x17102000;
//const int VERSION_ID = 0x17100900;
//const int VERSION_ID = 0x17091900;
//const int VERSION_ID = 0x17060800;
//const int VERSION_ID = 0x17051800;
//const int VERSION_ID = 0x17042100;
//const int VERSION_ID = 0x17031500;
//const int VERSION_ID = 0x17022700;
//const int VERSION_ID = 0x17013100;
//const int VERSION_ID = 0x16120200;

// Read a file in chunks of 8 MBytes
const int BUFSIZE = 8*1024*1024;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    check_level         = 2;
  int    blocks_to_display   = 0;
  int    blocks_displayed    = 0;
  int    blocks_to_skip      = 0;
  int    blocksize           = 1024;
  bool   blocksize_forced    = false;
  int    elink_filter        = -1;
  int    word_size           = 1;
  bool   trailer_is_32bit    = false;
  bool   display_data_chunks = false;
  bool   write_data_chunks   = false;
  bool   display_chunks      = true;
  bool   display_chunk_sz    = true;
  bool   display_timeout     = true;
  bool   display_timeout_data= true;
  bool   display_sca         = false;
  bool   chunkdata_only      = false;
  bool   report_trunc_err    = true;
  bool   report_crc_err      = true;
  string filename;
  FILE  *fp;
  unsigned char *buffer = new unsigned char[BUFSIZE];

  // Parse options
  while( (opt = getopt(argc, argv, "O0248AB:cCDe:F:S:tThVwx:X")) != -1 )
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
          // Check level
          if( sscanf( optarg, "%d", &check_level ) != 1 )
            arg_error( 'B' );
          break;
        case 'C':
          chunkdata_only = true; // SPECIAL: data values only, nothing else
          break;
        case 'c':
          display_data_chunks = false;
          display_chunks = false;
          break;
        case 'D':
          display_data_chunks = true;
          display_chunks = false;
          break;
        case 'e':
          // Elink number to filter out from the data
          unsigned int x;
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elink_filter = x;
          if( elink_filter < 0 || elink_filter > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
          break;
        case 'F':
          // Number of blocks to dump to display
          if( sscanf( optarg, "%d", &blocks_to_display ) != 1 )
            arg_error( 'F' );
          break;
        case 'S':
          // Number of blocks to skip before starting check
          if( sscanf( optarg, "%d", &blocks_to_skip ) != 1 )
            arg_error( 'S' );
          break;
        case 't':
          report_trunc_err = false;
          report_crc_err = false;
          break;
        case 'T':
          report_crc_err = false;
          break;
        case 'w':
          write_data_chunks = true;
          //{
          //  char name[256];
          //  if( sscanf( optarg, "%s", name ) != 1 )
          //    arg_error( 'w' );
          //  printf( "name=%s\n", name );
          //}
          break;
        case 'h':
          usage();
          return 0;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'x':
          // FLX-device block size in KByte
          if( sscanf( optarg, "%d", &blocksize ) != 1 )
            arg_error( 'x' );
          if( blocksize < 1 || blocksize > 1024 )
            arg_range( 'x', 1, 1024 );
          blocksize *= 1024;
          blocksize_forced = true;
          break;
        case 'X':
          trailer_is_32bit = true;
          break;
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

  // Apply elink filter?
  if( elink_filter >= 0 )
    cout << "*ELINK FILTER* = "
         << hex << uppercase << elink_filter << dec << endl;

  FlxChecker flxChecker;
  if( write_data_chunks )
    {
      // Open the output file
      flxChecker.openOutputFile( "./dataout.dat" );
    }
  else
    {
      if( blocks_to_display > 0 && display_chunks && !chunkdata_only )
        {
          cout << "Chunk types: BOTH=\"<<\", FIRST=\"++\", LAST=\"&&\", "
               << "MIDDLE=\"==\", TIMEOUT=\"]]\", NULL=\"@@\",\n"
               << "OUTOFBAND=\"##\" and \"TEC\" for chunk "
               << "truncation/error/CRCerr.\n";
        }
    }

  int            blocknr_offs;
  int            blocks_skipped;
  unsigned long  total_bytes;
  size_t         nbytes;
  bool           check_blockid;

 next_file:
  blocknr_offs   = 0;
  blocks_skipped = 0;
  total_bytes    = 0;
  check_blockid  = true;
  flxChecker.reset();

  cout << "=> File: " << filename << endl;
  // Open the (input) file
  if( (fp = fopen( filename.c_str(), "r" )) == NULL )
    {
      cout << "### Failed to open file " << filename << endl;
      return 1;
    }

  flxChecker.setBlockSize( blocksize );
  flxChecker.setTrailerIs32Bit( trailer_is_32bit );
  while( (nbytes = fread( buffer, 1, BUFSIZE, fp )) > 0 )
    {
      if( check_blockid )
        {
          // Look at the very first block ID to see if we're dealing with
          // 32-bit trailers or 16-bit trailers and
          // check if the blocksize is encoded in the block ID
          uint16_t *block_s = (uint16_t *) buffer;
          if( block_s[1] == BLOCK_ID_32B )
            {
              flxChecker.setTrailerIs32Bit( true );
            }
          else if( (block_s[1] & 0xC0FF) == BLOCK_ID_PHASE2 )
            {
              if( !blocksize_forced )
                {
                  // Derive blocksize from the header
                  blocksize = (((block_s[1] & 0x3F00) >> 8) + 1) * 1024;
                  flxChecker.setBlockSize( blocksize );
                }
              flxChecker.setTrailerIs32Bit( true );
            }
          cout << "Blocksize: " << blocksize << endl;
          check_blockid = false;
        }
      if( blocks_to_display > 0 )
        {
          unsigned char *block = buffer;
          for( unsigned int blocknr=0; blocknr<nbytes/blocksize;
               ++blocknr, block+=blocksize )
            {
              if( blocks_skipped < blocks_to_skip )
                {
                  ++blocks_skipped;
                  continue;
                }
              // Display up to the requested number of blocks
              if( blocks_displayed != blocks_to_display )
                {
                  if( write_data_chunks )
                    // Write complete user data chunks to file
                    // (and temporarily store FIRST and MIDDLE subchunks
                    //  until LAST subchunk found)
                    blocks_displayed +=
                      flxChecker.writeDataChunks( block,
                                                  blocknr + blocknr_offs,
                                                  elink_filter );
                  else if( display_data_chunks )
                    // Display complete user data chunks only
                    // (temporarily store FIRST and MIDDLE subchunks)
                    blocks_displayed +=
                      flxChecker.displayDataChunks( block,
                                                    blocknr + blocknr_offs,
                                                    display_sca,
                                                    elink_filter,
                                                    word_size );
                  else if( display_chunks )
                    // Display FELIX format (sub)chunks
                    blocks_displayed +=
                      flxChecker.displayChunks( block,
                                                blocknr + blocknr_offs,
                                                display_chunk_sz,
                                                display_timeout,
                                                display_timeout_data,
                                                display_sca,
                                                chunkdata_only,
                                                elink_filter,
                                                word_size );
                  else
                    // Display block's raw bytes,
                    // preceded by interpreted block header (first 4 bytes)
                    blocks_displayed +=
                      flxChecker.displayBlock( block,
                                               blocknr + blocknr_offs,
                                               blocksize, 0,
                                               elink_filter,
                                               word_size );
                }
            }
        }
      else
        {
          flxChecker.checkBlocks( (unsigned long) buffer,
                                  nbytes, check_level, false, elink_filter,
                                  blocknr_offs, blocks_to_skip,
                                  report_trunc_err, report_crc_err );
          blocks_to_skip -= nbytes/blocksize;
        }
      total_bytes  += nbytes;
      blocknr_offs += nbytes/blocksize;
    }

  if( !chunkdata_only )
    {
      cout << "File " << filename << " contains " << total_bytes << " bytes ("
           << (total_bytes / blocksize) << " FLX blocks";
      if( (total_bytes % blocksize) > 0 )
        cout << " +" << (total_bytes % blocksize);
      cout << ")" << endl;
    }

  // Display a summary of Elink number, chunk sizes, etc.
  if( total_bytes > 0 && blocks_to_display == 0 )
    flxChecker.displaySummary();

  // Next file name ?
  ++optind;
  if( optind < argc )
    {
      filename = string( argv[optind] );
      cout << endl;
      goto next_file;
    }

  if( write_data_chunks )
    {
      // Close the output file
      flxChecker.closeOutputFile();
      printf( "Wrote (selected) data from %d blocks to file\n",
              blocks_displayed );
    }

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fcheck version " << hex << VERSION_ID << dec << endl <<
    "Usage: fcheck [-h|V] [-A] [-B <id>] [-c|C|D] [-e <elink>] [-F <blocks>] "
    "[-S <blocks>]\n"
    "              [-x <kbyte>] [-X] [-t|T] [-w] [-0|O] [-2|4|8] <filename>\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -A         : Interpret chunks that could be GBT-SCA frames.\n"
    "  -B <lvl>   : Do a check on (emulator) data blocks according to <lvl>,\n"
    "               and display a data summary (default: 2):\n"
    "               0: Check for proper block headers at block boundaries,\n"
    "                  for each block 1 line of output is produced.\n"
    "               1: Same as 0, but only when an error is found a line "
    "is output.\n"
    "               2: Full integrity checking of blocks, starting from\n"
    "                  the block trailer going through all chunks.\n"
    "               3: Same as 2, including a check on expected emulator "
    "data payload,\n"
    "                  which must constitute an incrementing byte.\n"
    "               4: Same as 3, but inconsistent maximum values of L1ID are "
    "not reported.\n"
    "  -c         : Display data 'raw' datablocks (default: chunk data)"
    " (with option -F)\n"
    "  -C         : Display chunk data bytes only, nothing else.\n"
    "  -D         : Display only whole data chunks, "
    "i.e. the user's data frames.\n"
    "  -e <elink> : E-link number (hex) to filter for block check "
    "or block display.\n"
    "  -F <blocks>: Dump <blocks> FLX data blocks to display "
    "(overrules data check option -B).\n"
    "               Chunk types: BOTH=\"<<\", FIRST=\"++\", LAST=\"&&\", "
    "MIDDLE=\"==\", TIMEOUT=\"]]\", NULL=\"@@\",\n"
    "                OUTOFBAND=\"##\" and \"TE\" for chunk truncation/error.\n"
    "  -S <blocks>: Skip <blocks> of data blocks before starting check "
    "or display.\n"
    "  -t         : Do NOT report chunk truncation/error/CRCerror.\n"
    "  -T         : Do NOT report chunk CRCerror.\n"
    "  -w         : Instead of displaying, write (binary) chunkdata to file "
    "(dataout.dat).\n"
    "  -0         : Do NOT display time-out chunkdata bytes (zeroes).\n"
    "  -O         : Do not display time-out chunks at all.\n"
    "  -x <kbyte> : Set size of FLX-device unit data block, in KB (default:1)\n"
    "               (NB: overrules any blocksize derived from data)\n"
    "  -X         : Trailer size is 32-bit (default:16)\n"
    "  -2         : Display data as 2-byte words (little-endian).\n"
    "  -4         : Display data as 4-byte words (little-endian).\n"
    "  -8         : Display data as 8-byte words (little-endian).\n"
    " <filename>  : Name of file containing data to check or display.\n";
}

// ----------------------------------------------------------------------------
