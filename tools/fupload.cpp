#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxConfig.h"
#include "FlxUpload.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x21092900; // Use FlxConfig class
//const int VERSION_ID = 0x21061500; // Add TLP size option -y
//const int VERSION_ID = 0x21011500; // Automatic DMA index selection
//const int VERSION_ID = 0x19061900; // Add no-upload option -u; MROD data file
//const int VERSION_ID = 0x19012200; // Add circular DMA option -X
//const int VERSION_ID = 0x18080100;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int      opt;
  bool     dbg_mode     = false;
  bool     check_link   = true;
  bool     timestamp    = false;
  int      cardnr       = 0;
  int      elinknr      = -1;
  int      gbtnr        = -1;
  int      egroupnr     = -1;
  int      epathnr      = -1;
  int      eindex       = -1;
  int      ewidth       = -1;
  int      dma_index    = -1; // Autoselect FromHost DMA controller index
  uint32_t dma_size     = 1;
  int      dma_tlp      = 0;
  int      pattern_id   = 0;
  int      repeat       = -1;
  uint64_t datasize     = 32;
  uint64_t buffersize   = 128ULL*1024ULL*1024ULL;
  int      secs_timeout = 0;
  int      speed_factor = 1;
  bool     raw          = false;
  bool     dma_circular = false;
  bool     binary       = false;
  bool     upload_it    = true;
  string   filename;

  int      elinknr2     = -1;
  int      gbtnr2       = -1;
  int      egroupnr2    = -1;
  int      epathnr2     = -1;

  // Parse the options
  while( (opt = getopt(argc, argv,
                       "b:Bcd:De:E:f:g:G:hi:I:p:P:r:Rs:t:TuVw:x:Xy:")) != -1 )
    {
      switch( opt )
        {
        case 'b':
          // Buffer size in MBytes
          if( sscanf( optarg, "%lu", &buffersize ) != 1 )
            arg_error( 'b' );
          buffersize *= 1024ULL*1024ULL;
          if( buffersize > 4096ULL*1024ULL*1024ULL )
            {
              buffersize = 4096ULL*1024ULL*1024ULL;
              cout << "(buffer size capped to 4GB)" << endl;
            }
          break;
        case 'B':
          binary = true;
          break;
        case 'c':
          check_link = false;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          dbg_mode = true;
          break;
        case 'e':
          // E-link number to upload to
          unsigned int x;
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elinknr = x;
          if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
          break;
        case 'E':
          // Optional second E-link number to upload to
          {
            unsigned int x;
            if( sscanf( optarg, "%x", &x ) != 1 )
              arg_error( 'E' );
            elinknr2 = x;
            if( elinknr2 < 0 || elinknr2 > FLX_MAX_ELINK_NR )
              arg_range( 'E', 0, FLX_MAX_ELINK_NR );
          }
          break;
        case 'f':
          // A factor to increase upload speed with (at your own risk)
          if( sscanf( optarg, "%d", &speed_factor ) != 1 )
            arg_error( 'f' );
          if( speed_factor < 0 || speed_factor > 999 )
            arg_range( 'f', 0, 999 );
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
          // (NB: Egroup 7 (or 5 (not anymore)) + Epath 7 is EC link)
          // (NB: Egroup 7 + Epath 6 is IC link)
          // (NB: Egroup 7 + Epath 5 is SCA-AUX link)
          if( sscanf( optarg, "%d", &egroupnr ) != 1 )
            arg_error( 'g' );
          if( egroupnr < 0 ||
              (egroupnr > FLX_FROMHOST_GROUPS-1 && egroupnr != 7) )
            arg_range( 'g', 0, FLX_FROMHOST_GROUPS-1 );
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
          // Index of e-link in GBT frame
          if( sscanf( optarg, "%d", &eindex ) != 1 )
            arg_error( 'I' );
          if( eindex < 0 || eindex > 5*16-1 )
            arg_range( 'I', 0, 5*16-1 );
          break;
        case 'p':
          // E-path number to upload to
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'P':
          // Test data pattern identifier
          if( sscanf( optarg, "%d", &pattern_id ) != 1 )
            arg_error( 'P' );
          if( pattern_id < 0 || pattern_id > 3 )
            arg_range( 'P', 0, 3 );
          break;
        case 'r':
          // Test data chunk repeat counter
          if( sscanf( optarg, "%d", &repeat ) != 1 )
            arg_error( 'r' );
          if( repeat < 0 )
            arg_range( 'r', 0, 0x7FFFFFFF );
          break;
        case 'R':
          raw = true;
          break;
        case 's':
          // Number of data bytes per chunk to upload
          if( sscanf( optarg, "%lu", &datasize ) != 1 )
            arg_error( 's' );
          if( datasize <= 0 )
            arg_range( 's', 1, 0x7FFFFFFF );
          break;
        case 't':
          if( sscanf( optarg, "%d", &secs_timeout ) != 1 )
            arg_error( 't' );
          break;
        case 'T':
          timestamp = true;
          break;
        case 'u':
          upload_it = false;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'w':
          // E-link width in bits
          if( sscanf( optarg, "%d", &ewidth ) != 1 )
            arg_error( 'w' );
          if( !(ewidth == 2 || ewidth == 4 || ewidth == 8 || ewidth == 16) )
            {
              cout << "### -w: argument not one of [2,4,8,16]" << endl;
              return 0;
            }
          break;
        case 'x':
          // DMA size in KByte
          if( sscanf( optarg, "%u", &dma_size ) != 1 )
            arg_error( 'x' );
          if( dma_size < 1 || dma_size > buffersize/1024 )
            arg_range( 'x', 1, buffersize/1024 );
          break;
        case 'X':
          // Going to use Julia's directly-into-CR upload registers (OBSOLETE)
          //use_dma = false;
          // Use circular (endless) DMA
          dma_circular = true;
          break;
        case 'y':
          // Set FromHost TLP size (default: automatic, i.e. 32 bytes)
          if( sscanf( optarg, "%d", &dma_tlp ) != 1 )
            arg_error( 'y' );
          if( dma_tlp < 0 )
            arg_range( 'y', 0, 0xFFFF );
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  if( !raw )
    {
      // Check for either a valid -e option or valid -G/g/p or -G/I/w options
      if( (elinknr != -1 && (gbtnr != -1 || egroupnr != -1 || epathnr != -1 ||
                             eindex != -1 || ewidth != -1)) ||
          (elinknr == -1 && gbtnr == -1) ||
          (elinknr == -1 &&
           !(egroupnr != -1 && epathnr != -1) &&
           !(eindex != -1 && ewidth != -1)) )
        {
          cout << "### Provide either -e, -G/g/p or -G/I/w options" << endl;
          return 1;
        }

      if( elinknr != -1 )
        {
          // Derive GBT, e-group and e-path numbers from the given e-link number
          gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
          egroupnr = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
          epathnr  = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
        }
      else if( eindex != -1 )
        {
          egroupnr = eindex / 16;
          epathnr  = GbtConfig::toEpath( eindex, ewidth );
          if( epathnr == -1 )
            {
              cout << "### Invalid -I and/or -w option(s)" << endl;
              return 1;
            }
        }

      if( elinknr == -1 )
        {
          // Derive e-link number from the given GBT, group and epath numbers
          elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                     (egroupnr << BLOCK_EGROUP_SHIFT) |
                     (epathnr  << BLOCK_EPATH_SHIFT));
        }

      if( egroupnr == 7 && !(epathnr == 7 || epathnr == 6 || epathnr == 5) )
        {
          cout << "### Only group #7 + path #7(EC), #6(IC) "
               << "or #5(SCA-AUX) allowed"
               << endl;
          return 1;
        }

      cout << "GBT #" << gbtnr
           << " group #" << egroupnr
           << " path #" << epathnr;
      if( egroupnr == 7 && epathnr == 7 )
        cout << " (EC-link)";
      else if( egroupnr == 7 && epathnr == 6 )
        cout << " (IC-link)";
      else if( egroupnr == 7 && epathnr == 5 )
        cout << " (AUX-link)";
      cout << endl;

      if( elinknr2 != -1 )
        {
          // Derive GBT, e-group and e-path numbers from the given e-link number
          gbtnr2    = (elinknr2 & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
          egroupnr2 = (elinknr2 & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
          epathnr2  = (elinknr2 & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
        }
    }

  // Name of file with data to upload
  // (at the same time, adjust the default repeat count)
  if( optind < argc )
    {
      filename = string( argv[optind] );
      if( repeat == -1 )
        repeat = 1; // Default file content repeat count
    }
  else if( repeat == -1 )
    {
      repeat = 30; // Default pattern repeat count
    }

  // Need space for headers in the data structure to be uploaded
  // for simplicity allow upload of up to half the buffer size
  if( filename.empty() &&
      datasize * (uint64_t) repeat > buffersize/2 )
    {
      cout << "### DMA buffer size too small: " << buffersize << "/2 < "
           << datasize * (uint64_t) repeat << endl;
      cout << "(Use option -b to increase size)\n";
      return 1;
    }

  FlxUpload fup( cardnr, buffersize, dma_index );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", firmw " << fup.firmwareVersion()
       << " (cmem buffersize="
       << fup.bufferSize()/(1024*1024) << "MB)" << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  fup.setDmaSize( dma_size*1024 );

  fup.setDmaTlp( dma_tlp );

  fup.setDmaCircular( dma_circular );

  FlxConfig fconfig( cardnr );
  if( fconfig.hasError() )
    {
      cout << "### " << fconfig.errorString() << endl;
      return 1;
    }

  if( !raw && check_link )
    {
      // Check whether the selected e-link is configured
      // and thus a valid link on the selected FLX-card
      if( (eindex == -1 &&
           fconfig.isEnabledElink( gbtnr, egroupnr, epathnr, &ewidth )) ||
          (eindex != -1 &&
           fconfig.isEnabledElink( gbtnr, eindex, ewidth )) )
        {
          cout << "Selected e-link has a width of " << ewidth << endl;
        }
      else
        {
          cout << "### Selected e-link is not valid or not configured" << endl;
          return 1;
        }
      if( elinknr2 != -1 )
        {
          if( fconfig.isEnabledElink( gbtnr2, egroupnr2, epathnr2, &ewidth ) )
            {
              cout << "Selected 2nd e-link has a width of " << ewidth << endl;
            }
          else
            {
              cout << "### Selected 2nd e-link is not valid or not configured"
                   << endl;
              return 1;
            }
        }
    }
  else if( !check_link )
    {
      cout <<  "**NB**: E-link validity not checked!" << endl;
    }

  if( dbg_mode )
    cout << "**DEBUG ON**" << endl;
  //if( dbg_mode )
  //  cout << "Before upload: " << fup.debugString();

  int uploadsize;
  if( filename.empty() )
    {
      if( raw )
        {
          uploadsize = fup.prepareDataRaw( datasize );
        }
      else
        {
          if( elinknr2 == -1 )
            {
              // Upload to 1 E-link
              uploadsize = fup.prepareData( elinknr,
                                            datasize, pattern_id,
                                            repeat );
            }
          else
            {
              // Upload to two E-links
              //int gbt[2]  = { gbtnr,    gbtnr2 };
              //int egrp[2] = { egroupnr, egroupnr2 };
              //int epth[2] = { epathnr,  epathnr2 };
              int elink[2] = { elinknr, elinknr2 };
              uploadsize = fup.prepareData( 2, elink, //gbt, egrp, epth,
                                            datasize, pattern_id,
                                            repeat );
            }
        }

      if( dbg_mode && uploadsize > 0 )
        fup.dumpData( uploadsize+16 );
    }
  else
    {
      if( raw )
        uploadsize = fup.prepareDataRaw( filename, binary );
      else
        uploadsize = fup.prepareData( elinknr, filename, repeat, binary,
                                      dbg_mode );

      if( dbg_mode && uploadsize > 0 )
        fup.dumpData( uploadsize+16 );
    }

  int result = 0;
  if( uploadsize > 0 && upload_it )
    {
      fup.stop(); // Abort ongoing DMA, if any

      cout << "Uploading ";
      if( filename.empty() )
        cout << repeat << " chunks of " << datasize << " bytes: ";
      cout << uploadsize << " bytes";
      //if( use_dma )
      {
        int dma_size = fup.dmaSize();
        cout << ", using ";
        if( dma_circular ) cout << "CIRCULAR ";
        cout << "DMA #" << fup.dmaIndex();
        if( filename.empty() )
          cout << ", in units of " << dma_size << " bytes -> "
               << (uploadsize+dma_size-1)/dma_size << " DMAops";
      }
      //else
      //  {
      //    cout << ", using 'upload' registers";
      //  }
      cout << ", timeout=";
      if( secs_timeout == 0 )
        cout << "none";
      else
        cout << secs_timeout;
      cout << endl;

      // Calculate time-out time
      struct timespec ts;
      time_t sec_start, sec_to;
      long   nsec_start, nsec_to;
      clock_gettime( CLOCK_REALTIME, &ts );
      sec_start  = ts.tv_sec;
      nsec_start = ts.tv_nsec;
      sec_to     = sec_start + secs_timeout;
      nsec_to    = nsec_start;

      int cnt = 0;
      while( !fup.uploadFinished() )
        {
          //if( fup.upload( use_dma, speed_factor ) ) OLD
          if( fup.upload( speed_factor ) )
            {
              ++cnt;
            }
          else if( secs_timeout > 0 )
            {
              // Check if the clock time is equal to
              // or past the calculated time-out time,
              // then break out of the while-loop
              clock_gettime( CLOCK_REALTIME, &ts );
              if( ts.tv_sec == sec_to )
                {
                  if( ts.tv_nsec >= nsec_to )
                    break;
                }
              else if( ts.tv_sec > sec_to )
                {
                  break;
                }
            }
        }
      // End time
      clock_gettime( CLOCK_REALTIME, &ts );

      if( fup.uploadFinished() )
        {
          cout << "Upload finished";

          time_t sec;
          long   nsec;
          sec = ts.tv_sec - sec_start;
          if( ts.tv_nsec < nsec_start )
            {
              --sec;
              nsec = 1000000000 + ts.tv_nsec - nsec_start;
            }
          else
            {
              nsec = ts.tv_nsec - nsec_start;
            }
          cout << " in " << sec << "s ";
          if( nsec < 1000000 )
            cout << (nsec+500)/1000 << "us, ";
          else
            cout << (nsec+500000)/1000000 << "ms, ";

          double mb_per_s = (((double) uploadsize) /
                             ((double) (sec*1000000)+((nsec+500)/1000)));
          cout.precision( 3 );
          cout << mb_per_s << " MB/s";
        }
      else
        {
          cout << "### Upload timed out";
          result = 1;
        }
      //if( use_dma )
      cout << " (#DMAs=" << cnt << ")";
      //else
      //  cout << " (#CR-blocks=" << cnt << ")";

      if( timestamp )
        {
          uint64_t t = fup.timestamp();
          uint64_t s = t/1000000000;
          uint64_t h = s/3600;
          // Hours, minutes, seconds, microseconds
          cout << setfill('0')
               << " T=" << h << ":" << setw(2) << (s - h*3600)/60
               << ":" << setw(2) << (s % 60)
               << "." << setw(6) << (t - s*1000000000 + 500)/1000;
        }

      cout << endl;
    }
  else if( upload_it )
    {
      cout << "### Upload: something is wrong, no action" << endl;
      result = 1;
    }
  else
    {
      cout << "NB: Upload skipped" << endl;
    }

  if( !dma_circular ) // In order to allow check of DMA status afterwards
    fup.stop();

  //if( dbg_mode )
  //  cout << "After stop: " << fup.debugString();

  cout << "Exiting.." << endl;
  return result;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fupload version " << hex << VERSION_ID << dec << endl <<
    "Upload data (test data or from file) to the given FLX-device E-link.\n"
    "The E-link number is provided as a (hex) number directly (-e option),\n"
    "as a set of -G/g/p options, or as a set of -G/I/w options,\n"
    "unless option -R is given ('raw' unformatted upload).\n\n"
    "Checks whether the E-link is valid and configured on the selected "
    "FLX-device,\n"
    "unless option -c is given.\n\n"
    "In ASCII data files one line is one data packet (hexadecimal byte "
    "values separated by spaces),\n"
    "while lines starting with certain characters may be used to:\n"
    "  # insert a comment line\n"
    "  + insert a packet of the given length containing bytes of the given "
    "byte value\n"
    "  & insert a configurable delay in microseconds between two packets\n"
    "  > change the E-link number to upload to\n\n"

    "Usage: fupload [-h|V] [-D] [-d <devnr>] [-b <size>] [-c] (-e <elink>\n"
    "               | (-G <gbt> (-g <group> -p <path>) | "
    "(-I <index> -w <width>)) [-i <dma>]\n"
    "               [-s <bytes>] [-P <patt>] [-f <speed>] [-R] [-t <secs>] "
    "[-u] [-x <size>] [-X] [-y <tlp>] [<filename>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -b <size>  : DMA (cmem_rcc) memory buffer size to use, in MB "
    "(default 128, max 4096).\n"
    "  -B         : Contents of <filename> is read as binary data "
    "(default: ASCII).\n"
    "  -c         : Do not check whether E-link is configured on FLX-device.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -D         : Debug mode on, i.e. display blocks being uploaded.\n"
    "  -f <speed> : Speed up default upload rate of about 8MB/s by "
    "factor <speed> (default: 1)\n"
    "  -i <dma>   : FLX-device DMA controller to use (default: auto).\n"
    "  -P <patt>  : Test data pattern: 0=incr, 1=0x55/0xAA, 2=0xFF, "
    "3=incr-per-chunk (default: 0).\n"
    "  -r <repeat>: Test data repeat count: upload <repeat>*<bytes> bytes "
    "of data (default: 30).\n"
    "  -R         : Upload data unformatted, not as CR from-host "
    "data packets with header.\n"
    "  -s <bytes> : Number of bytes per chunk to upload (default: 32).\n"
    "  -t <secs>  : Number of seconds for DMA time-out or "
    "wait until DMA done when 0 (default: 0).\n"
    "  -u         : Do not perform the actual upload operation.\n"
    "  -x <size>  : Size of single-shot DMA transfers, in KByte "
    "(default: 1).\n"
    "  -X         : Use continuous-mode DMA for upload "
    "(default: single-shot).\n"
    "  -y <tlp>   : Size of TLP used in DMA transfers, in bytes "
    "(default: 32).\n\n"
    "Options to define the E-link to use:\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p or -G/I/w options.\n"
    "  -E <elink> : an optional 2nd E-link number to upload to\n"
    "               (alternating with the first given E-link number).\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number.\n"
    "  -p <path>  : E-path number.\n"
    "  -I <index> : Index of first bit of E-link in GBT frame.\n"
    "  -w <width> : E-link width in bits (2, 4, 8 or 16).\n\n"
    "  <filename> : Name of file with data to upload (ASCII or binary),\n"
    "               or test pattern data if no name is given.\n";
}

// ----------------------------------------------------------------------------
