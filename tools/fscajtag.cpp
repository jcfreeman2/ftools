#include <fstream>
#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>
#include <string.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "BitFile.h"
#include "JtagPort.h"
#include "arg.h"

// Xilinx VIRTEX7 JTAG instructions
#define V7_IRLEN        6
#define V7_IDCODE_LEN   32
#define V7_CFG_OUT      0x04
#define V7_CFG_IN       0x05
#define V7_IDCODE       0x09
#define V7_JPROGRAM     0x0B
#define V7_JSTART       0x0C
#define V7_JSHUTDOWN    0x0D
#define V7_ISC_ENABLE   0x10
#define V7_ISC_PROGRAM  0x11
#define V7_ISC_DISABLE  0x16
#define V7_BYPASS       0x3F

const string V7_STATBITS_STR[] = {
  "CRC ERROR                     ",
  "DECRYPTOR ENABLE              ",
  "PLL LOCK STATUS               ",
  "DCI MATCH STATUS              ",
  "END OF STARTUP (EOS) STATUS   ",
  "GTS_CFG_B STATUS              ",
  "GWE STATUS                    ",
  "GHIGH STATUS                  ",
  "MODE PIN M[0]                 ",
  "MODE PIN M[1]                 ",
  "MODE PIN M[2]                 ",
  "INIT_B INTERNAL SIGNAL STATUS ",
  "INIT_B PIN                    ",
  "DONE INTERNAL SIGNAL STATUS   ",
  "DONE PIN                      ",
  "IDCODE ERROR                  ",
  "SECURITY ERROR                ",
  "SYSTEM MON OVERTEMP ALARM STAT",
  "CFG STARTUP STATEMACHINE PHASE",
  "CFG STARTUP STATEMACHINE PHASE",
  "CFG STARTUP STATEMACHINE PHASE",
  "RESERVED                      ",
  "RESERVED                      ",
  "RESERVED                      ",
  "RESERVED                      ",
  "CFG BUS WIDTH DETECTION       ",
  "CFG BUS WIDTH DETECTION       ",
  "HMAC ERROR                    ",
  "PUDC_B PIN                    ",
  "BAD PACKET ERROR              ",
  "CFGBVS PIN                    ",
  "RESERVED                      "
};

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x19061800; // Fix data time-out issue (JtagPort.cpp)
//const int VERSION_ID = 0x19020500; // Add circular-DMA option -c
//const int VERSION_ID = 0x18121200; // Replace get_dataword() by jtag.getTdi()
//const int VERSION_ID = 0x18101200; // Support for devices in chain to BYPASS
//const int VERSION_ID = 0x18041100; // Major changes to JtagPort
//const int VERSION_ID = 0x17120100; // Added CFG register read-out
//const int VERSION_ID = 0x17112700;

#define sleep_ms(x) usleep((x)*1000)

void swap_bits   ( size_t        sz,
                   unsigned char *bytes );
void swap_bits   ( unsigned int  *val );

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
  int    jtag_rate   = 20; // In MHz
  bool   byte_rev    = true;
  bool   stat_bits   = false;
  bool   dma_circular= false;
  string filename;

  // Variables pertaining to other devices in the JTAG chain
  // that need to be BYPASSed
  int      devs_before  = 0; // Number of devices before the FPGA
  int      ibits_before = 0; // Total number of instruction bits of those devs
  int      devs_after   = 0; // Number of devices after the FPGA
  int      ibits_after  = 0; // Total number of instruction bits of those devs
  int      dev_instr    = -1;// BYPASS instruction if *different* from only 1s
                             // (which actually should be the default!
                             //  Variables ibits_before/after in that case
                             //  become *single* instruction register length!)
  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "cd:De:G:g:hp:rR:sVx:X:y:Y:z:")) != -1 )
    {
      switch( opt )
        {
        //case 'b':
          //byte_rev = false;
          //break;
        case 'c':
          dma_circular = true;
          break;
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
        case 's':
          stat_bits = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'x':
          if( sscanf( optarg, "%d", &devs_before ) != 1 )
            arg_error( 'x' );
          if( devs_before < 1 || devs_before > 32 )
            arg_range( 'x', 1, 32 );
          break;
        case 'X':
          // Total number of 'before' instruction bits, or
          // -with option -z given- the number of instruction bits per device
          if( sscanf( optarg, "%d", &ibits_before ) != 1 )
            arg_error( 'X' );
          if( ibits_before < 1 || ibits_before > 256 )
            arg_range( 'X', 1, 256 );
          break;
        case 'y':
          if( sscanf( optarg, "%d", &devs_after ) != 1 )
            arg_error( 'y' );
          if( devs_after < 1 || devs_after > 32 )
            arg_range( 'y', 1, 32 );
          break;
        case 'Y':
          // Total number of 'after' instruction bits, or
          // -with option -z given- the number of instruction bits per device
          if( sscanf( optarg, "%d", &ibits_after ) != 1 )
            arg_error( 'Y' );
          if( ibits_after < 1 || ibits_after > 256 )
            arg_range( 'Y', 1, 256 );
          break;
        case 'z':
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'z' );
          dev_instr = x;
          if( dev_instr < 0 || dev_instr > 0xFFFF )
            arg_range( 'z', 0, 0xFFFF );
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Check for either a valid -e or valid -G/g/p options:
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
  // .BIT firmware file

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

  BitFile bitfile;
  if( !filename.empty() )
    {
      try {
        bitfile.setFile( filename );
      }
      catch( std::runtime_error &exc ) {
        string msg;
        msg += filename;
        msg += ": ";
        msg += exc.what();
        cout << msg << endl;
        return 1;
      }
      bitfile.displayInfo( debug );
      if( byte_rev )
        {
          // Note: alternatively set GBT-SCA JTAG channel to shift MSB-to-LSB
          // (instead of LSB-TO-MSB)
          // and change the byte-ordering in JtagPort::shiftDr(),
          // although that may have implications for the TMS bits in
          // gotoState() and the instruction in shiftIr() (TO BE TESTED)

          //cout << "NB: Reversing bits-within-bytes" << endl;
          bitfile.reverseBytes();
          //bitfile.displayInfo();
        }
    }

  // -------------------------------------------------------------------------
  // Other devices in the JTAG chain

  // Show info about additional devices in the JTAG chain
  // before and after the HPTDC devices
  if( devs_before > 0 || devs_after > 0 )
    {
      if( devs_before > 0 )
        {
          if( ibits_before == 0 )
            {
              cout << "### Provide number of BYPASS bits: -X option" << endl;
              return 1;
            }

          cout << "# Devices before: " << devs_before << " ";
          if( dev_instr >= 0 )
            cout << "Instr=0x" << hex << dev_instr << dec << " (total bits: "
                 << devs_before*ibits_before << ")" << endl;
          else
            cout << "(total bits: " << ibits_before << ")" << endl;
        }
      if( devs_after > 0 )
        {
          if( ibits_after == 0 )
            {
              cout << "### Provide number of BYPASS bits: -Y option" << endl;
              return 1;
            }

          cout << "# Devices after: " << devs_after << ", ";
          if( dev_instr >= 0 )
            cout << "Instr=0x" << hex << dev_instr << dec << " (total bits: "
                 << devs_after*ibits_after << ")" << endl;
          else
            cout << "(total bits: " << ibits_after << ")" << endl;
        }
    }

  // -------------------------------------------------------------------------
  // FLX-card sender and receiver

  FlxUpload fup( cardnr, 0, dma_write );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", firmw " << fup.firmwareVersion() << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  //fup.setDmaSize( 32*1024 );
  fup.setDmaCircular( dma_circular );

  FlxReceiver *frecv = 0;
  if( receive )
    {
      frecv = new FlxReceiver( cardnr, 512*1024*1024, dma_read );
      if( frecv->hasError() )
        {
          cout << "### " << frecv->errorString() << endl;
          frecv->stop();
          return 1;
        }
      /*
      cout << "Opened FLX-device " << cardnr
           << ", firmw " << frecv->firmwareVersion()
           << ", " << frecv->numberOfChans()
           <<  " chans (cmem bufsize="
           << frecv->bufferSize() << ", receive: "
           << "DMA #" << dma_read;
      if( use_intrpt )
        cout << " interrupt-driven)" << endl;
      else
        cout << " polling)" << endl;
      */
      frecv->setUseInterrupt( use_intrpt );
    }

  // NB: one extra GBT-SCA reply received: take it into account, below
  if( !fup.scaConnect( elinknr ) )
    cout << "###GBT-SCA connect: " << fup.errorString() << endl;

  // -------------------------------------------------------------------------
  // Enable and configure the GBT-SCA JTAG channel

  JtagPort jtag( fup, frecv, elinknr );
  jtag.configure( jtag_rate );

  if( debug && receive )
    // Raw dump of GBT-SCA replies
    jtag.displayReplies( "JTAG enable/config" );

  // -------------------------------------------------------------------------
  // JTAG operations on a Virtex7

  // Go to a defined state
  jtag.gotoState( JtagPort::TEST_LOGIC_RESET );

  if( debug && receive )
    // Raw dump of GBT-SCA replies
    jtag.displayReplies( "JTAG test-logic-reset" );

  unsigned char instr;
  if( bitfile.valid() )
    {
      instr = V7_JPROGRAM;
      //jtag.shiftIr( V7_IRLEN, &instr );
      jtag.shiftIr( V7_IRLEN, instr, devs_before, ibits_before,
                    devs_after, ibits_after, dev_instr );

      jtag.gotoState( JtagPort::TEST_LOGIC_RESET );
      jtag.shift( JtagPort::RUN_TEST_IDLE, 10000*jtag_rate );

      instr = V7_CFG_IN;
      //jtag.shiftIr( V7_IRLEN, &instr );
      jtag.shiftIr( V7_IRLEN, instr, devs_before, ibits_before,
                    devs_after, ibits_after, dev_instr );

      jtag.showProgress( true );
      //jtag.shiftDr( bitfile.nbytes()*8, bitfile.data() );
      jtag.shiftDr( bitfile.nbytes()*8, bitfile.data(), false,
                    devs_before, devs_after );
      jtag.showProgress( false );

      instr = V7_JSTART;
      //jtag.shiftIr( V7_IRLEN, &instr );
      jtag.shiftIr( V7_IRLEN, instr, devs_before, ibits_before,
                    devs_after, ibits_after, dev_instr );

      jtag.shift( JtagPort::RUN_TEST_IDLE, 2000*jtag_rate );
      jtag.gotoState( JtagPort::TEST_LOGIC_RESET );

      // Check for DONE and INIT bits in 'Instruction Capture Values loaded
      // into IR' ("7-Series FPGA Configuration, User Guide", Sep 2016, pg 170)
      // (bit 0=1, 1=0, 2=ISC_DONE, 3=ISC_ENABLED, 4=INIT, 5=DONE)
      bool read_tdi = true;
      //jtag.shift( JtagPort::SHIFT_IR, V7_IRLEN, 0, read_tdi );
      jtag.shiftIr( V7_IRLEN, 0, devs_before, ibits_before,
                    devs_after, ibits_after, dev_instr, read_tdi );
      jtag.gotoState( JtagPort::TEST_LOGIC_RESET );

      // Extract the data of interest from the GBT-SCA replies
      uint32_t ir = 0;
      uint8_t *tdi_bytes;
      if( jtag.getTdi( &tdi_bytes, 10000 ) >= 32 )
        {
          ir = *((uint32_t *) tdi_bytes);
          cout << endl << ((ir==0x35) ? "OKAY" : "FAILED");
          cout << hex << setfill('0') << setw(8)
               << " (result=0x" << ir << ")";
        }
      else
        {
          cout << endl << "###Result NOT received";
        }
      jtag.clearTdi();
    }
  else
    {
      // Get the device ID code
      instr = V7_IDCODE;
      //jtag.shiftIr( V7_IRLEN, &instr );
      jtag.shiftIr( V7_IRLEN, instr, devs_before, ibits_before,
                    devs_after, ibits_after, dev_instr );

      // DEBUG: upload BYPASS instruction
      //instr = V7_BYPASS;
      //jtag.shiftIr( V7_IRLEN, instr, devs_before, ibits_before,
      //              devs_after, ibits_after, dev_instr );
      //jtag.displayReplies( "JTAG SHIFT-IR" );

      unsigned char tdo[4];
      memset( tdo, 0x00, sizeof(tdo) );
      bool read_tdi = true;
      //jtag.shiftDr( V7_IDCODE_LEN, tdo, read_tdi );
      jtag.shiftDr( V7_IDCODE_LEN, tdo, read_tdi,
                    devs_before, devs_after );

      // DEBUG: check what's shifted out of IR
      //bool read_tdi = true;
      ////jtag.shift( JtagPort::SHIFT_IR, V7_IRLEN, read_tdi );
      //jtag.shiftIr( V7_IRLEN, 0, devs_before, ibits_before,
      //              devs_after, ibits_after, dev_instr, read_tdi );

      if( !debug && receive )
        {
          // Extract the data of interest from the GBT-SCA replies
          uint32_t id = 0;
          uint8_t *tdi_bytes;
          cout << "FPGA ID=";
          if( jtag.getTdi( &tdi_bytes, 10000 ) >= 32 )
            {
              id = *((uint32_t *) tdi_bytes);
              cout << hex << setfill('0') << setw(8) << id;
            }
          else
            {
              cout << "###NOT RECEIVED";
            }
          jtag.clearTdi();

          // Xilinx 7-series FPGA family member IDs
          id = (id & 0x0FFFF000) >> 12;
          if( id == 0x3622 || id == 0x3620 || id == 0x37C4 ||
              id == 0x362F || id == 0x37C8 || id == 0x37C7 )
            cout << " (Spartan-7)";
          else if( id == 0x37C3 || id == 0x362E || id == 0x37C2 ||
                   id == 0x362D || id == 0x362C || id == 0x3632 ||
                   id == 0x3631 || id == 0x3636 )
            cout << " (Artix-7)";
          else if( id == 0x3647 || id == 0x364C || id == 0x3651 ||
                   id == 0x3747 || id == 0x3656 || id == 0x3752 ||
                   id == 0x3751 )
            cout << " (Kintex-7)";
          else if( id == 0x3671 || id == 0x36B3 || id == 0x3667 ||
                   id == 0x3682 || id == 0x3687 || id == 0x3692 ||
                   id == 0x3691 || id == 0x3696 || id == 0x36D5 ||
                   id == 0x36D9 || id == 0x36DB )
            cout << " (Virtex-7)";
          else if( id == 0x0000 || id == 0xFFFF )
            cout << " (not connected?)";
          else
            cout << " (<unknown>)";
        }
      else
        {
          // Raw dump of GBT-SCA replies
          if( receive )
            jtag.displayReplies( "JTAG IDCODE" );
        }

      // Get a device configuration register:
      // compose the FPGA configuration packets
      // (See "7-Series FPGA Configuration, User Guide", Chapter 5 and 6:
      //  Read operation 0x28xxxxxx with register address in bits 13-17
      //  and word count in bits 0-10;
      //  register addresses: STAT: 7, IDCODE: C, COR0: 9, COR1: E, etc.)
      unsigned char cfg[] =
        { 0xAA, 0x99, 0x55, 0x66,   // Sync word
          0x20, 0x00, 0x00, 0x00,   // NOOP
          0x28, 0x00, 0xE0, 0x01,   // Read STAT
          //0x28, 0x01, 0x80, 0x01, // Read IDCODE
          0x20, 0x00, 0x00, 0x00,   // NOOP
          0x20, 0x00, 0x00, 0x00 }; // NO0P
      swap_bits( sizeof(cfg), cfg ); // Swap bits within bytes
      instr = V7_CFG_IN;
      //jtag.shiftIr( V7_IRLEN, &instr );
      jtag.shiftIr( V7_IRLEN, instr, devs_before, ibits_before,
                    devs_after, ibits_after, dev_instr );
      //jtag.shiftDr( sizeof(cfg)*8, cfg );
      jtag.shiftDr( sizeof(cfg)*8, cfg, false, devs_before, devs_after );
      instr = V7_CFG_OUT;
      //jtag.shiftIr( V7_IRLEN, &instr );
      jtag.shiftIr( V7_IRLEN, instr, devs_before, ibits_before,
                    devs_after, ibits_after, dev_instr );
      //jtag.shiftDr( 32, tdo, read_tdi );
      jtag.shiftDr( 32, tdo, read_tdi, devs_before, devs_after );
      if( !debug && receive )
        {
          // Extract the data of interest from the GBT-SCA replies
          uint32_t cfg = 0;
          uint8_t *tdi_bytes;
          cout << ", CFG-STAT=";
          if( jtag.getTdi( &tdi_bytes, 10000 ) >= 32 )
            {
              cfg = *((uint32_t *) tdi_bytes);
              swap_bits( &cfg ); // Swap bits within the 32-bit word
              cout << hex << setfill('0') << setw(8) << cfg;
              if( stat_bits )
                {
                  cout << dec << setfill( ' ' );
                  for( int i=0; i<32; ++i )
                    cout << endl << setw(2) << i << " "
                         << V7_STATBITS_STR[i] << ": "
                         << ((cfg & (1U<<i)) ? "1" : "0");
                }
            }
          else
            {
              cout << "###NOT RECEIVED";
            }
          jtag.clearTdi();
        }
      else
        {
          // Raw dump of GBT-SCA replies
          if( receive )
            jtag.displayReplies( "JTAG CFG" );
        }
    }

  //if( debug )
  cout << endl << endl << dec
       << "Replies: expected " << jtag.repliesExpected() + 1
       << ", received " << jtag.repliesReceived() << endl;
  cout << "(Errors: CRC=" << jtag.scaCrcErrors()
       << " CMD=" << jtag.scaCmdErrors()
       << " (S)REJ=" << jtag.scaRejErrors() << ")" << endl;

  // -------------------------------------------------------------------------
  // Finish up...

  fup.stop();
  if( frecv )
    frecv->stop();
  cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscajtag version " << hex << VERSION_ID << dec << endl <<
    "Tool to program a bit-file into a Xilinx 7-Series FPGA connected "
    "to the JTAG port\n"
    "of a GBT-SCA, connected to any FLX-device GBT (2-bit HDLC) E-link.\n"
    "The FPGA may be part of a JTAG chain containing multiple devices;\n"
    "the tool has options to take this into account.\n"
    "If a bit-file name is not provided the ID-code and Status register\n"
    "of the FPGA are read out and displayed.\n"
    "Usage:\n"
    " fscajtag [-h|V] [-D] [-d <devnr>] [-e <elink>] "
    "[-G <gbt> [-g <group> -p <path>]]\n"
    "          [-c] [-R <rate>] [-r] [-s]\n"
    "          [-x <devs> -X <ibits> -y <devs> -Y <ibits> -z <instr>]\n"
    "          [<filename>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    //"-b       : Do NOT byte-reverse bit-file data.\n"
    "  -c         : Use continuous-mode FromHost DMA (default: single-shots).\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -D         : Enable debug mode: display all GBT-SCA replies.\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number (default: 7=EC).\n"
    "  -p <path>  : E-path number (default: 7=EC).\n"
    "  -R <rate>  : JTAG clock rate, in MHz, 1,2,4,5,10 or 20 (default: 20).\n"
    "  -r         : Do NOT receive and process/display GBT-SCA replies.\n"
    "  -s         : Display FPGA Configuration register bits.\n"
    "  -x <devs>  : Number of devices preceeding the FPGA in the JTAG chain.\n"
    "  -X <ibits> : Total number of preceeding BYPASS instruction bits, or\n"
    "               (with option -z) the number of instruction bits "
    "per device.\n"
    "  -y <devs>  : Number of devices trailing the FPGA in the JTAG chain.\n"
    "  -Y <ibits> : Total number of trailing BYPASS instruction bits, or\n"
    "               (with option -z) the number of instruction bits "
    "per device.\n"
    "  -z <instr> : The BYPASS instruction value for each of the preceeding\n"
    "               and trailing devices (when unequal to only '1'-bits).\n"
    " <filename>  : Name of .bit file containing the FPGA configuration.\n";
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
