#include <fstream>
#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "JtagSequence.h"
#include "JtagPort.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x18043000;

#define sleep_ms(x) usleep((x)*1000)

void swap_bits( size_t sz, unsigned char *bytes );
void swap_bits( unsigned int *val );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    cardnr      = 0;
  int    dma_write   = -1; // Autoselect FromHost DMA controller index
  int    dma_read    = 0;
  int    gbtnr       = -1;
  int    egroupnr    = 7; // EC-link
  int    epathnr     = 7; // EC-link
  int    elinknr     = -1;
  bool   receive     = true;
  bool   use_intrpt  = false;
  bool   debug       = false;
  bool   noop        = false;
  int    jtag_rate   = 20; // In MHz
  //bool byte_rev    = true;
  string filename;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "d:De:G:g:hnp:rR:V")) != -1 )
    {
      switch( opt )
        {
        //case 'b':
          //byte_rev = false;
          //break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          debug = true;
          break;
        case 'e':
          // E-link number
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elinknr = x;
          if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
          break;
        case 'G':
          // GBT link number
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'g':
          // E-group number
          // (NB: Egroup 7 (or 5) + Epath 7 is EC link)
          if( sscanf( optarg, "%d", &egroupnr ) != 1 )
            arg_error( 'g' );
          if( egroupnr < 0 || (egroupnr > FLX_FROMHOST_GROUPS-1 &&
                               egroupnr != 5 && egroupnr != 7) )
            arg_range( 'g', 0, FLX_FROMHOST_GROUPS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'n':
          noop = true;
          break;
        case 'p':
          // E-path number
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'r':
          receive = false;
          break;
        case 'R':
          // JTAG clock rate (in MHz)
          if( sscanf( optarg, "%d", &jtag_rate ) != 1 )
            arg_error( 'R' );
          //if( jtag_rate < 1 || jtag_rate > 20 )
          if( !(jtag_rate == 20 || jtag_rate == 10 ||
                jtag_rate == 5 || jtag_rate == 4 ||
                jtag_rate == 2 || jtag_rate == 1) )
            {
              //arg_range( 'R', 1, 20 );
              cout << "### -R: argument not one of "
                   << "[1,2,4,5,10,20]*1MHz" << endl;
              return 1;
            }
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

  // -------------------------------------------------------------------------
  // JTAG bits file

  // File name
  if( optind < argc )
    {
      filename = string( argv[optind] );
    }
  else
    {
      //cout << "### Filename missing.." << endl;
      //exit( 0 );
    }

  JtagSequence jseq;
  if( !filename.empty() )
    {
      try {
        jseq.setFile( filename );
      }
      catch( std::runtime_error &exc ) {
        string msg;
        msg += filename;
        msg += ": ";
        msg += exc.what();
        cout << msg << endl;
        return 1;
      }

      jseq.displayInfo( debug );
    }

  if( noop )
    return 0;

  // -------------------------------------------------------------------------

  // Check for either a valid -e or valid -G/g/p options
  if( (elinknr != -1 && !(gbtnr == -1 || egroupnr == -1 || epathnr == -1)) ||
      (elinknr == -1 && !(gbtnr != -1 && egroupnr != -1 && epathnr != -1)) )
    {
      cout << "### Provide either -e or -G/g/p options" << endl;
      return 1;
    }
  if( elinknr != -1 )
    {
      // Derive GBT, e-group and e-path numbers from the given e-link number
      gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
      egroupnr = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
      epathnr  = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
    }
  else
    {
      // Derive e-link number from the given GBT, group and epath numbers
      elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                 (egroupnr << BLOCK_EGROUP_SHIFT) |
                 (epathnr  << BLOCK_EPATH_SHIFT));
   }

  // -------------------------------------------------------------------------
  // FLX-card sender and receiver

  FlxUpload fuploader( cardnr, 0, dma_write );
  if( fuploader.hasError() )
    {
      cout << "### " << fuploader.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", firmw " << fuploader.firmwareVersion() << endl;

  if( fuploader.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fuploader.setFanOutForDaq();

  FlxReceiver *freceiver = 0;
  if( receive )
    {
      freceiver = new FlxReceiver( cardnr, 512*1024*1024, dma_read );
      if( freceiver->hasError() )
        {
          cout << "### " << freceiver->errorString() << endl;
          freceiver->stop();
          return 1;
        }
      /*
      cout << "Opened FLX-device " << cardnr
           << ", firmw " << freceiver->firmwareVersion()
           << ", " << freceiver->numberOfChans()
           <<  " chans (cmem bufsize="
           << freceiver->bufferSize() << ", receive: "
           << "DMA #" << dma_read;
      if( use_intrpt )
        cout << " interrupt-driven)" << endl;
      else
        cout << " polling)" << endl;
      */
      freceiver->setUseInterrupt( use_intrpt );
    }

  if( !fuploader.scaConnect( elinknr ) )
    cout << "###GBT-SCA connect: " << fuploader.errorString() << endl;

  // -------------------------------------------------------------------------
  // Enable and configure the GBT-SCA JTAG channel

  JtagPort jtag( fuploader, freceiver, elinknr );
  jtag.configure( jtag_rate );

  if( debug && receive )
    // Raw dump of GBT-SCA replies
    jtag.displayReplies( "JTAG enable/config" );

  // -------------------------------------------------------------------------
  // JTAG operations

  // Go to a defined state
  jtag.gotoState( JtagPort::TEST_LOGIC_RESET );

  // Upload the JTAG strings, either in SHIFT-IR or in SHIFT-DR state
  for( unsigned int i=0; i<jseq.stringCount(); ++i )
    {
      bool reversed       = false;
      int  nbits          = jseq.bitCount( i );
      unsigned char *bits = jseq.bits( i, reversed );
      if( jseq.isInstruction(i) )
        {
          cout << "Instr " << nbits << endl;
          jtag.shiftIr( nbits, bits );
        }
      else
        {
          cout << "Data " << nbits << endl;
          jtag.shiftDr( nbits, bits, jseq.readTdi(i) );
        }
      // Cycle through state RUN_TEST_IDLE ?
      //jtag.gotoState( JtagPort::RUN_TEST_IDLE );
    }

  if( !debug && receive )
    {
      unsigned char *tdi;
      int nbits = jtag.getTdi( &tdi, 10000 );
      //if( nbits > 0 )
        cout << "TDI returned " << nbits << " bits:" << endl;
      cout << hex << setfill( '0' );
      for( int i=0; i<(nbits+7)/8; ++i )
        {
          if( (i & 0x3F) == 0x20 ) cout << endl;
          cout << setw(2) << (uint32_t) tdi[i] << " ";
        }
      cout << endl;
      cout << dec << setfill( ' ' );
      jtag.clearTdi();
    }
  else
    {
      // Raw dump of GBT-SCA replies
      if( receive )
        jtag.displayReplies( "JTAG operations" );
    }
  cout << "(Replies: expected " << jtag.repliesExpected() + 1
       << ", received " << jtag.repliesReceived() << endl;
  cout << " Errors : CRC=" << jtag.scaCrcErrors()
       << " CMD=" << jtag.scaCmdErrors()
       << " (S)REJ=" << jtag.scaRejErrors() << ")" << endl;

  // -------------------------------------------------------------------------
  // Finish up...

  fuploader.stop();
  if( freceiver )
    freceiver->stop();
  cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscajfile version " << hex << VERSION_ID << dec << endl <<
    "*** UNDER DEVELOPMENT ***\n"
    "Tool to upload JTAG bit strings from file to the JTAG port "
    "of a GBT-SCA,\nconnected to any FLX-device GBT E-link (2-bit HDLC).\n"
    "Usage:\n"
    " fscajfile [-h|V] [-D] [-n] [-d <devnr>] [-e <elink>] "
    "[-G <gbt> [-g <group> -p <path>]]\n"
    "           [-b] [-R <rate>] [-r] [<filename>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    //"  -b       : Do NOT byte-reverse bit-file data.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -D         : Enable debug mode: display all GBT-SCA replies.\n"
    "  -n         : 'No operation', only read the JTAG file.\n"
    "  -e <elink> : E-link number (hex), or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number (default: 7=EC-channel).\n"
    "  -p <path>  : E-path number (default: 7=EC-channel).\n"
    "  -R <rate>  : JTAG clock rate, in MHz, 1,2,4,5,10 or 20 (default: 20).\n"
    "  -r         : Do NOT receive and process/display GBT-SCA replies.\n"
    " <filename>  : Name of file containing JTAG instructions and data.\n";
}

// ----------------------------------------------------------------------------

void swap_bits( size_t sz, unsigned char *bytes )
{
  // Bit-reverse the data array byt-by-byte
  unsigned int i, j;
  unsigned char tmp, bitmask;
  for( i=0; i<sz; ++i )
    {
      tmp = 0;
      bitmask = 1;
      for( j=0; j<8; ++j, bitmask<<=1 )
        if( bytes[i] & bitmask )
          tmp |= (1 << (7-j));
      bytes[i] = tmp;
    }
}

// ----------------------------------------------------------------------------

void swap_bits( unsigned int *val )
{
  unsigned int tmp = 0, bitmask = 0x1;;
  for( int i=0; i<32; ++i, bitmask<<=1 )
    if( (*val) & bitmask )
      tmp |=  (1 << (31-i));
  *val = tmp;
}

// ----------------------------------------------------------------------------
