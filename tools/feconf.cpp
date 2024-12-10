#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>
#include <time.h>

#include "version.h"
#include "FlxConfig.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x22022400; // Log output with preceding timestamp
//const int VERSION_ID = 0x22011800; // DMA index configuration (for RM5);
                                     // option -N: do not write RM5 regsettings
//const int VERSION_ID = 0x21100500; // Display emulator info: chunks, rate
//const int VERSION_ID = 0x21092900; // Use FlxConfig class
//const int VERSION_ID = 0x21032600; // 'RegisterSettings' in .yelc config file
//const int VERSION_ID = 0x21022100; // GbtConfig5.h in FlxUpload.cpp
//const int VERSION_ID = 0x20121700; // New: TTC Clock setting in config files
//const int VERSION_ID = 0x20121600; // Set fanout to DAQ, unless option -F
//const int VERSION_ID = 0x20071600; // FM bug fix; support for NSW 16-bit Elink
//const int VERSION_ID = 0x19112600; // Support for StreamID in emulator data
//const int VERSION_ID = 0x19091900; // Support for StreamID bits in .jelc files
//const int VERSION_ID = 0x19081600; // Can read JSON-formatted .jelc files
//const int VERSION_ID = 0x18121800; // Fix LSB-first option (see FlxUpload)
//const int VERSION_ID = 0x18061900;

std::string timestamp();

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    cardnr                    = 0;
  bool   write_it                  = true;
  int    emu_chunksize             = 32;
  int    emu_idle_chars            = 8;
  bool   emu_random_size           = false;
  bool   emu_ewidth_dependent_size = false;
  bool   emu_with_streamid         = false;
  bool   lsb_first                 = false;
  bool   do_regsettings            = true;
  bool   set_fanout                = true;
  string filename;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:FhI:LnNs:RSVw")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'h':
          usage();
          return 0;
        case 'F':
          set_fanout = false;
          break;
        case 'I':
          // Emulator data number of idle characters between chunks
          if( sscanf( optarg, "%d", &emu_idle_chars ) != 1 )
            arg_error( 'I' );
          break;
        case 'L':
          lsb_first = true;
          break;
        case 'n':
          write_it = false;
          break;
        case 'N':
          do_regsettings = false;
          break;
        case 'R':
          emu_random_size = true;
          break;
        case 'S':
          emu_with_streamid = true;
          break;
        case 's':
          // Emulator data chunk size
          if( sscanf( optarg, "%d", &emu_chunksize ) != 1 )
            arg_error( 's' );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'w':
          emu_ewidth_dependent_size = true;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Name of file with configuration to write to FLX-card
  if( optind < argc )
    filename = string( argv[optind] );
  if( filename.empty() )
    {
      cout << timestamp() << "### Missing configuration file name" << endl;
      return 1;
    }

  FlxConfig fconf( cardnr );
  if( fconf.hasError() )
    {
      cout << timestamp() << "### " << fconf.errorString() << endl;
      return 1;
    }
  cout << timestamp()
       << "Opened FLX-device " << cardnr
       << ", firmw " << fconf.firmwareVersion()
       << endl;

  int elinks_th, eclinks_th, elinks_fh, eclinks_fh, linkmode, settings_cnt;
  if( !fconf.writeLinkConfig( filename, write_it,
                              emu_chunksize, emu_ewidth_dependent_size,
                              emu_random_size, emu_idle_chars,
                              emu_with_streamid,
                              lsb_first,
                              do_regsettings,
                              &elinks_th, &eclinks_th, &elinks_fh, &eclinks_fh,
                              &linkmode, &settings_cnt ) )
    {
      cout << timestamp()
           << "### writeLinkConfig(): " << fconf.errorString() << endl;
    }
  else
    {
      cout << timestamp();
      if( write_it )
        cout << "Link configuration from file written: ";
      else
        cout << "Link configuration from file read: ";
      if( linkmode == 0 )
        cout << "GBT";
      else if( linkmode == 1 )
        cout << "GBT-wide";
      else if( linkmode == 2 )
        cout << "FULLMODE";
      else if( linkmode == 3 )
        cout << "LPGBT";
      else
        cout << "????";
      cout << endl;

      cout << timestamp();
      if( linkmode == 2 )
        cout << "Links: ";
      else
        cout << "E-links: ";
      cout << "ToHost=" << elinks_th << "+" << eclinks_th
           << ", FromHost=" << elinks_fh << "+" << eclinks_fh
           << endl;

      cout << timestamp();
      if( settings_cnt > 0 )
        {
          cout << "Additional register settings: " << settings_cnt;
          if( !do_regsettings )
            cout << " (NOT written)";
        }
      else
        {
          cout << "No additional register settings";
        }
      cout << endl;

      // Emulator
      cout << timestamp()
           << "(Emulator: " << fconf.emulatorChunkCount() << " chunks, ";
      if( fconf.emulatorChunkRate() < 10.0 )
        cout << fixed << setprecision(3);
      else
        cout << fixed << setprecision(1);
      cout << fconf.emulatorChunkRate() << " KHz per (e)link)" << endl;

      //cout << "Bandwidth: ";
      //if( elinks2 > 0 )  cout << elinks2 << "x80 ";
      //if( elinks4 > 0 )  cout << elinks4 << "x160 ";
      //if( elinks8 > 0 )  cout << elinks8 << "x320 ";
      //if( elinks16 > 0 ) cout << "(" << elinks16 << "x640) ";
      //int total = elinks2*80 + elinks4*160 + elinks8*320;// + elinks16*640;
      //cout << " Total: " << total << " Mb/s" << " = " << total/8 << " MB/s";
      //cout << " (w/o 16bit)" << endl;

      if( fconf.dmaIndexInvalidCount() > 0 )
        {
          cout << "WARNING: " << fconf.dmaIndexInvalidCount() << " (E-)links "
               << "with invalid DMA index in file (set to 0)" << endl;
        }
    }

  if( write_it && set_fanout )
    {
      // Configure ToHost and FromHost fan-out for DAQ
      if( fconf.fanOutIsLocked() )
        cout << timestamp()
             <<  "WARNING: FanOut-Select registers are locked!" << endl;
      fconf.setFanOutForDaq();
    }

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "feconf version " << hex << VERSION_ID << dec << endl <<
    "Upload an e-link configuration from file "
    "(as generated by elinkconfig) to the given FLX-device,\n"
    "including generation and upload of emulator data contents.\n"
#if REGMAP_VERSION >= 0x500
    "Any 'RegisterSettings' present in the file are applied as well\n"
    "(after the e-link and emulator configuration).\n"
#endif // REGMAP_VERSION
    "Usage: feconf [-h|V] [-d <devnr>] [-s <chunksz>] [-w] [-R] [-S] "
    "[-I <idles>] [-n] [-N] <filename>\n"
    "  -h          : Show this help text.\n"
    "  -V          : Show version.\n"
    "  -d <devnr>  : FLX-device to use (default: 0).\n"
    "  -F          : Do not set FLX-card fan-out registers "
    "(default: set for DAQ, not emulator).\n"
    "  -L          : 8b10b-words LSB first (GBT only; default: MSB first).\n"
    "  -n          : Don't write the configuration, "
    "just read it in and display some info.\n"
    "  -N          : Don't write the register settings of the configuration "
    "file (RM5 only).\n"
    "  -R          : Generate emulator data chunks with pseudo-random size.\n"
    "  -S          : Generate emulator data with a StreamID (first byte).\n"
    "  -s <chunksz>: Emulator data chunksize to generate (default: 32).\n"
    "  -w          : Generate emulator data chunksize dependent on "
    "e-link width (default: false).\n"
    "  -I <idles>  : The number of idles between generated emulator "
    "data chunks (default: 8).\n"
#if REGMAP_VERSION < 0x500
    " <filename>   : Name of .elc or .jelc file with FLX-device E-link "
#else
    " <filename>   : Name of .yelc file with FLX-device E-link "
#endif // REGMAP_VERSION
    "configuration.\n";
}

// ----------------------------------------------------------------------------

std::string timestamp()
{
  // Get current time
  time_t rawtime;
  time( &rawtime );

  // Convert time into a struct tm
  struct tm *timeinfo;
  timeinfo = localtime( &rawtime );

  // Generate string from struct tm
  char timestr[32];
  strftime( timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S ", timeinfo );

  return std::string( timestr );
}

// ----------------------------------------------------------------------------
