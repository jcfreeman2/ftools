#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxParser.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x21122800; // Data value hex must have x or 0x
//const int VERSION_ID = 0x21122300; // Option -1 (lpGBTv1) and -e (EC-link)
//const int VERSION_ID = 0x21092100; // Option -i (select ToHost DMA controller)
//const int VERSION_ID = 0x21091300; // Option -e (use EC-link, for lpGBT)
//const int VERSION_ID = 0x21012200; // Option -D (debug)
//const int VERSION_ID = 0x21012000; // Check for lpGBT firmware
//const int VERSION_ID = 0x18102400;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    cardnr      = 0;
  int    dma_write   = -1; // Autoselect FromHost DMA controller index
  int    dma_read    = 0;
  int    linknr      = -1;
  int    i2c_addr    = -1;
  int    reg_addr    = -1;
  int    databyte    = -1;
  bool   write_reg   = false;
  bool   one_by_one  = false;
  bool   receive     = true;
  bool   receive_any = false;
  bool   use_intrpt  = false;
  bool   txt_output  = false;
  //bool file_is_xml = false;
  bool   lpgbt       = false;
  bool   debug       = false;
  bool   use_ec      = false;
  bool   lpgbt_v1    = false;
  string filename;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "1a:d:DeG:hi:I:oRtVZ")) != -1 )
    {
      switch( opt )
        {
        case '1':
          lpgbt_v1 = true;
          break;
        case 'a':
          // GBTX/lpGBT register address
          {
            // Accept hex values "x12" and "0x12", as well as decimal "18"
            int result, i = 0;
            if( optarg[0] == 'x' || optarg[0] == 'X' )
              i = 1;
            else if( optarg[0] == '0' &&
                     (optarg[1] == 'x' || optarg[1] == 'X') )
              i = 2;

            if( i != 0 )
              {
                unsigned int x;
                result = sscanf( &optarg[i], "%x", &x );
                if( result == 1 )
                  {
                    arg_check_hex( &optarg[i], opt );
                    reg_addr = (int) x;
                  }
              }
            else
              {
                result = sscanf( &optarg[i], "%d", &reg_addr );
                if( result == 1 )
                  arg_check_dec( optarg, opt );
              }

            if( result != 1 )
              arg_error( 'a' );

            if( reg_addr < 0 || reg_addr > 511 )
              arg_range( 'a', 0, 511 );
          }
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          debug = true;
          break;
        case 'e':
          use_ec = true;
          break;
        case 'G':
          // (lp)GBT link number
          if( sscanf( optarg, "%d", &linknr ) != 1 )
            arg_error( 'G' );
          if( linknr < 0 || linknr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          // DMA controller to use
          if( sscanf( optarg, "%d", &dma_read ) != 1 )
            arg_error( 'i' );
          if( dma_read < 0 || dma_read > 7 )
            arg_range( 'i', 0, 7 );
          break;
        case 'I':
          // GBTX I2C address
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'I' );
          i2c_addr = x;
          if( i2c_addr < 0 || i2c_addr > 127 )
            arg_range_hex( 'I', 0, 127 );
          break;
        case 'o':
          one_by_one = true;
          break;
        case 'R':
          receive_any = true;
          break;
        case 't':
          txt_output = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        /*
          case 'X':
          file_is_xml = true;
          break;
        */
        case 'Z':
          receive = false;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Check for mandatory parameters
  if( linknr < 0 || i2c_addr < 0 )
    {
      if( linknr < 0 )
        cout << "### Provide a (lp)GBT link number (-G)" << endl;
      if( i2c_addr < 0 )
        cout << "### Provide an I2C address (-I)" << endl;
      return 1;
    }

  // Data byte value to write or name of file with GBTX/lpGBT register values
  if( optind < argc )
    {
      // There is an additional parameter:
      // it must be a byte value or a file name
      if( reg_addr != -1 )
        {
          // Expect a byte value to write;
          // accept hex values "x12" and "0x12", as well as decimal "18"
          char *str = argv[optind];
          int result, i = 0;
          if( str[0] == 'x' || str[0] == 'X' )
            i = 1;
          else if( str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
            i = 2;

          if( i != 0 )
            {
              unsigned int x;
              result = sscanf( &str[i], "%x", &x );
              if( result == 1 )
                {
                  arg_check_hex( &str[i], 0 );
                  databyte = (int) x;
                }
            }
          else
            {
              result = sscanf( &str[i], "%d", &databyte );
              if( result == 1 )
                arg_check_dec( str, 0 );
            }

          if( result == 1 )
            {
              if( databyte < 0 || databyte > 255 )
                {
                  cout << "### Data byte value out-of-range 0..0xFF: 0x"
                       << hex << setfill('0') << setw(2)
                       << (u32) databyte << dec << endl;
                  return 1;
                }
              write_reg = true;
            }
          else
            {
              cout << "### Invalid data byte value provided" << endl;
              return 1;
            }
        }
      else
        {
          filename = string( argv[optind] );
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

#if REGMAP_VERSION >= 0x500
  int fw = fup.firmwareMode();
  if( fw == FIRMW_PIXEL || fw == FIRMW_STRIP || fw == FIRMW_LPGBT )
    // It's a firmware using lpGBT
    lpgbt = true;
#endif // REGMAP_VERSION

  if( use_ec && !lpgbt )
    cout << "### WARNING: option -e for lpGBT only" << endl;

  FlxReceiver *frecv = 0;
  if( receive )
    {
      frecv = new FlxReceiver( cardnr, 64*1024*1024, dma_read );
      if( frecv->hasError() )
        {
          cout << "### " << frecv->errorString() << endl;
          frecv->stop();
          return 1;
        }
      /*cout << "Opened FLX-device " << cardnr
           << ", firmw " << frecv->firmwareVersion()
           <<  " (cmem buffersize="
           << frecv->bufferSize() << ", receive: "
           << "DMA #" << dma_read;
      if( use_intrpt )
        cout << " interrupt-driven)" << endl;
      else
        cout << " polling)" << endl;
      */
      frecv->setUseInterrupt( use_intrpt );
    }

  // -------------------------------------------------------------------------

  // Describe the requested operation:
  if( lpgbt )
    cout << ">>> lpGBTv" << (lpgbt_v1 ? "1" : "0") << "#";
  else
    cout << ">>> GBTX#";
  cout << linknr << " I2C-addr=0x" << hex << i2c_addr << dec << ": ";
  if( !filename.empty() )
    cout << "apply file " << filename << endl;
  else if( write_reg )
    cout << "WRITE 0x" << hex << setfill('0') << setw(2) << databyte
         << dec << " (" << databyte << ")"
         << " to reg 0x" << hex << setw(3) << reg_addr
         << dec << " (" << reg_addr << ")" << endl;
  else if( reg_addr != -1 )
    cout << "READ reg 0x" << hex << setfill('0') << setw(3) << reg_addr
         << dec << " (" << reg_addr << ")" << endl;
  else
    cout << "READ all registers" << endl;

  if( filename.empty() )
    {
      int nbytes = 1;
      u8  databyte_u8 = (u8) (databyte & 0xFF);
      if( write_reg )
        {
          // Write a single register
          if( !fup.writeIcChannel( linknr, i2c_addr, reg_addr,
                                   nbytes, &databyte_u8, use_ec, lpgbt_v1 ) )
            cout << "###write: " << fup.errorString() << endl;
          //else
          //  cout << "Write " << nbytes << " bytes: "
          //       << hex << setfill('0') << setw(2) << (u32) databyte_u8
          //       << dec << endl;
        }
      else
        {
          // Read either a single register or all registers
          if( reg_addr != -1 )
            {
              if( !fup.readIcChannel( linknr, i2c_addr, reg_addr,
                                      nbytes, use_ec, lpgbt_v1 ) )
                cout << "###read: " << fup.errorString() << endl;
              //else
              //  cout << "Read " << nbytes << " bytes" << endl;
            }
          else
            {
              // GBTX   : 436 8-bit registers; first 366 R/W, rest read-only
              // lpGBTv0: 463 8-bit registers; first 320 R/W,  "    "
              // lpGBTv1: 494 8-bit registers; first 326 R/W,  "    "
              int max_regs = 436;
              if( lpgbt )
                max_regs = (lpgbt_v1 ? 494 : 463);
              if( one_by_one )
                {
                  // Read the registers one-by-one
                  for( reg_addr=0; reg_addr<max_regs; ++reg_addr )
                    {
                      nbytes = 1;
                      if( !fup.readIcChannel( linknr, i2c_addr,
                                              reg_addr, nbytes,
                                              use_ec, lpgbt_v1 ) )
                        {
                          cout << "###read " << reg_addr << ": "
                               << fup.errorString() << endl;
                          //break;
                        }
                    }
                 }
              else
                {
                  // Read all registers in one operation
                  reg_addr = 0;
                  nbytes   = max_regs;
                  if( !fup.readIcChannel( linknr, i2c_addr, reg_addr,
                                          nbytes, use_ec, lpgbt_v1 ) )
                    cout << "###read-all: " << fup.errorString() << endl;
                }
            }
        }
    }
  else
    {
      int sz_file;
      if( fup.writeIcTxtFile( linknr, i2c_addr, filename, &sz_file,
                              use_ec, lpgbt_v1 ) )
        cout << "Wrote " << sz_file << " values" << endl;
      else
        cout << "###write file: " << fup.errorString() << endl;
    }

  // -------------------------------------------------------------------------

  // Do not receive replies? In that case we're done
  if( frecv == 0 )
    {
      fup.stop();
      return 0;
    }

  // The IC (or EC) elink number (will be mapped to the real number internally)
  int egroup = 7;
  int epath  = 6;
  if( use_ec )
    epath = 7;
  int elinknr = ((linknr << BLOCK_GBT_SHIFT) |
                 (egroup << BLOCK_EGROUP_SHIFT) |
                 (epath  << BLOCK_EPATH_SHIFT));
  if( debug )
    {
      int e_send = fup.mapElink( elinknr );
      FlxParser fparser;
      fparser.setReceiver( frecv );
      int e_recv = fparser.mapElink( elinknr );
      if( use_ec )
        cout << hex << "DEBUG: EC-send=0x" << e_send
             << " EC-recv=0x" << e_recv << endl;
      else
        cout << hex << "DEBUG: IC-send=0x" << e_send
             << " IC-recv=0x" << e_recv << endl;
    }
  if( receive_any )
    elinknr = -1; // Receive on any E-link...

  // Reading chunks from IC (or EC)
  FlxParser fparser;
  fparser.setReceiver( frecv );
  uint8_t *chunkdata = 0;
  uint32_t size, chunknr = 0;
  uint32_t parity_start_index = (lpgbt_v1 ? 0 : 2);
  int offset = (lpgbt_v1 ? 0 : 1);
  while( fparser.nextChunkRecvd( &chunkdata, &size, 10000, elinknr ) )
    {
      if( size == 0 ) continue;

      // Process chunk from the IC (or EC) channel
      uint32_t frame_len = size;
      uint8_t *frame     = chunkdata;
      // Check reply frame CRC
      // (do not include zero-byte and I2C-address, in case of GBT or lpGBTv0)
      if( frame_len > 2 )
        {
          if( !txt_output )
            cout << "Reply (size=" << frame_len << "): ";
          uint8_t parity = 0;
          for( uint32_t i=parity_start_index; i<frame_len-1; ++i )
            parity ^= frame[i];
          if( parity != frame[frame_len-1] )
            {
              cout << hex << setfill('0')
                   << "### Parity "
                   << setw(2) << (uint32_t) frame[frame_len-1]
                   << ", expected "
                   << setw(2) << (uint32_t) parity
                   << endl << dec << setfill(' ');
              //continue;
            }
          else if( !txt_output && !one_by_one )
            {
              cout << "Parity OK" << endl;
            }
        }

      // Display returned data words (bytes) in frame
      // (I2C address, zero-byte (lpGBTv0), command, #bytes, #address, parity:
      //  7 (v1) or 8 (v0) bytes in total) (###TBD: define v0 and v1 structs?)
      int data_len = frame_len - (offset+1+1+2+2+1);
      if( data_len > 0 )
        {
          uint32_t len  = ((uint32_t) frame[offset+2] +
                           ((uint32_t) frame[offset+3]<<8));
          uint32_t addr = ((uint32_t) frame[offset+4] +
                           ((uint32_t) frame[offset+5]<<8));
          cout << hex << setfill('0');
          for( uint32_t i=0; i<len; ++i )
            {
              if( (i & 0xF) == 0 && !txt_output )
                cout << hex << setfill('0')
                     << "Reg 0x" << setw(3) << (addr+i) << " ("
                     << dec << setfill(' ')
                     << setw(3) << (addr+i) << "): "
                     << hex << setfill('0');
              if( !txt_output ) cout << " ";
              cout << hex << setw(2)
                   << (uint32_t) frame[offset+6+i];
              if( (i & 0xF) == 0xF || (txt_output && len > 1) )
                cout << endl;
            }
          cout << endl;
          if( data_len != (int) len )
            {
              cout << "### Mismatched frame data length / length word: "
                   << data_len << ", " << len << endl;
            }
        }
      ++chunknr;
    }

  if( debug )
    cout << "(chunks received: " << chunknr << ")" << endl;

  if( chunknr == 0 )
    cout << "### Nothing received" << endl;

  fup.stop();
  if( frecv )
    frecv->stop();

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fice version " << hex << VERSION_ID << dec << endl <<
    "Tool to read or write GBTX or lpGBT registers via the IC-channel\n"
    "of an FLX-device (lp)GBT link (for lpGBT optionally via the EC-channel):\n"
    "read or write a single byte from or to the given GBTX/lpGBT register address\n"
    "or write to multiple consecutive GBTX/lpGBT registers using the contents\n"
    "of a file (i.e. ASCII file: 1 (register) byte value (hex) per line,\n"
    "e.g. the 'TXT' file generated by the GBTXProgrammer tool).\n"
    "Provide a file name *or* use option -a with an optional additional "
    "byte value\n"
    "to read resp. write a single GBTX register or, without option -a,\n"
    "to read all registers.\n"
    "Without option -a and file name all registers are read out and displayed\n"
    "either in one IC read operation or one-by-one (option -o).\n"
    "Option -t displays the register values in a format that could be used\n"
    "as a 'TXT' file for this tool or the I2C-dongle GBTX programmer.\n"
    "Usage:\n"
    " fice [-h|V] [-d <devnr>] [-G <gbt>] [-1] [-e] [-i <dma>] [-I <i2c>] [-Z|R] [-t]\n"
    "      [-a <addr> [<byte>] | <filename>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -1         : If lpGBT, assume v1 (default: v0)\n"
    "  -a <addr>  : GBTX/lpGBT register address (decimal or hex, 0x.. or x..)\n"
    "               to read or write.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e         : Use the EC-channel (for lpGBT only).\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -i <dma>   : FLX-device DMA controller for receiving (default: 0).\n"
    "  -I <i2c>   : GBTX/lpGBT I2C address.\n"
    "  -o         : When reading all registers, do it one-by-one\n"
    "               (default: one multi-reg read op).\n"
    "  -R         : Receive replies on any E-link.\n"
    "  -t         : Display one register value per line in output\n"
    "               (i.e. 'TXT'-format like).\n"
    "  -Z         : Do NOT receive and process/display replies.\n"
    " <byte>      : Byte value (decimal or hex, 0x.. or x..) to write to "
    "GBTX/lpGBT register (option -a).\n"
    " <filename>  : Name of file with GBTX/lpGBT register data to write\n"
    "               to consecutive registers.\n"
    "=> Examples:\n"
    "Read all registers of GBTX/lpGBT (I2C address 1)\n"
    "connected to FLX-device GBT link 3:\n"
    "  fice -G 3 -I 1\n"
    "Read GBTX/lpGBT register 32 (0x20):\n"
    "  fice -G 3 -I 1 -a 32 (or: fice -G 1 -I 3 -a 0x20)\n"
    "Write 0xA5 to GBTX/lpGBT register 32 (0x20):\n"
    "  fice -G 3 -I 1 -a 32 0xA5\n"
    "Write contents of GBT-conf.txt to GBTX/lpGBT registers:\n"
    "  fice -G 3 -I 1 GBT-conf.txt\n";
}

// ----------------------------------------------------------------------------
