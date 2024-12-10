#include <fstream>
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
const   int VERSION_ID = 0x21122800; // Value to write decimal or hex; FlxParser
//const int VERSION_ID = 0x17111700;

#define sleep_ms(x) usleep((x)*1000)

int read_txt_file( string filename, u8 *regbytes, int size );

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
  int    i2c_chan    = -1;
  int    i2c_addr    = -1;
  int    reg_addr    = -1;
  int    databyte    = -1;
  bool   write_reg   = false;
  bool   read_writable_only = false;
  bool   sca_reset   = false;
  //bool file_is_xml = false;
  string filename;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "a:C:d:e:G:g:hI:p:rRVW")) != -1 )
    {
      switch( opt )
        {
        case 'a':
          // GBTX register address
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
        case 'C':
          // GBT-SCA I2C channel number
          if( sscanf( optarg, "%d", &i2c_chan ) != 1 )
            arg_error( 'C' );
          if( i2c_chan < 0 || i2c_chan > 15 )
            arg_range( 'C', 0, 15 );
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
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
        case 'I':
          // GBTX I2C address
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'I' );
          i2c_addr = x;
          if( i2c_addr < 0 || i2c_addr > 127 )
            arg_range( 'I', 0, 127 );
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
          sca_reset = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'W':
          read_writable_only = true;
          break;
        /*
          case 'X':
          file_is_xml = true;
          break;
        */
        default: // '?'
          usage();
          return 1;
        }
    }

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

  // Check for mandatory parameters
  // I2C channel and I2C address
  if( i2c_chan == -1 || i2c_addr == -1 )
    {
      if( i2c_chan == -1 )
        cout << "### Provide a GBT-SCA I2C channel number (-C)" << endl;
      if( i2c_addr == -1 )
        cout << "### Provide a GBTX I2C address (-I)" << endl;
      return 1;
    }

  // Data byte value to write or name of file with GBTX register values
  if( optind < argc )
    {
      // There is an additional parameter:
      // it must be a byte value or a file name
      if( reg_addr != -1 )
        {
          // Expect a byte value to write
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
  cout << "Opened FLX-card " << cardnr
       << ", firmw " << fup.firmwareVersion() << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

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
           << " (cmem buffersize="
           << frecv->bufferSize() << ", receive)" << endl;
      */
      frecv->setUseInterrupt( use_intrpt );
    }

  // -------------------------------------------------------------------------

  // Describe the requested operation:
  cout << ">>> GBTX I2C-addr=" << i2c_addr << ": ";
  if( !filename.empty() )
    cout << "apply file " << filename << endl; 
  else if( reg_addr == -1 && read_writable_only )
    cout << "reading all (writable) registers" << endl;
  else if( reg_addr == -1 && !read_writable_only )
    cout << "reading all registers" << endl;
  else if( write_reg )
    cout << "write 0x" << hex << databyte
         << " to reg 0x" << reg_addr << dec << endl;
  else
    cout << "read reg " << reg_addr << endl;
  cout << ">>> using FLX-device #" << cardnr << ", E-link " << hex << elinknr
       << dec << " (" << gbtnr << "-" << egroupnr << "-" << epathnr << ")"
       << ", GBT-SCA I2C-chan " << i2c_chan
       << endl;

  if( !fup.scaConnect( elinknr ) )
    cout << "###GBT-SCA connect: " << fup.errorString() << endl;

  // Optionally reset the GBT-SCA
  if( sca_reset )
    {
      if( fup.scaReset( elinknr ) )
        cout << "GBT-SCA reset" << endl;
      else
        cout << "###GBT-SCA reset: " << fup.errorString() << endl;
    }

  // -------------------------------------------------------------------------
  // Enable

  // Prepare I2C channel
  u8  data[4] = { 0, 0, 0, 0 };
  int trid = 1;
  int recv_trid; // TRID used to indicate a reply containing data to extract
  int i2c_dev = SCA_DEV_I2C + i2c_chan;

  // ###NB: Should read and modify the current channel enable mask
  // ...

  // Enable selected I2C channel
  int cmd;
  if( i2c_chan >= 0 && i2c_chan < 5 )
    cmd = SCA_CONFIG_WR_B;
  else if( i2c_chan < 0xD )
    cmd = SCA_CONFIG_WR_C;
  else if( i2c_chan < 0x10 )
    cmd = SCA_CONFIG_WR_D;
  else
    {
      i2c_chan = 0;
      cmd = SCA_CONFIG_WR_B;
    }
  data[0] = 0x00;
  data[1] = 1 << (i2c_dev & 0x7);
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2, cmd, data );
  sleep_ms( 1 );

  // -------------------------------------------------------------------------
  // Configure

  // Write I2C channel control byte: 400 KHz and 2 or 3 bytes payload
  // (i.e. the 2-byte GBTx register address and -if writing- a data byte)
  data[0] = 0;
  if( !write_reg && filename.empty() )
    // Going to read
    data[1] = SCA_I2C_CTRL_400K | (2<<2);
  else
    // Going to write
    data[1] = SCA_I2C_CTRL_400K | (3<<2);
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_WR_CTRL, data );

  // -------------------------------------------------------------------------
  // The I2C operation

  // Now do the actual GBTX register read(s) or write(s)
  int reg_addr_min = 0, reg_addr_max;
  if( !write_reg && filename.empty() )
    {
      // Read either a single register or all registers
      if( reg_addr != -1 )
        {
          reg_addr_min = reg_addr;
          reg_addr_max = reg_addr + 1;
        }
      else
        {
          reg_addr_min = 0;
          if( read_writable_only )
            reg_addr_max = 366; // Writable registers
          else
            reg_addr_max = 436; // Including read-only registers
        }
      for( int reg=reg_addr_min; reg<reg_addr_max; ++reg )
        {
          // The bytes to write: the GBTx register address
          data[0] = (u8) ((reg & 0xFF00) >> 8);
          data[1] = (u8) (reg & 0xFF);
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_WR_DATA0, data );

          // Execute writing the (2) bytes as preset above
          data[0] = 0;
          data[1] = i2c_addr;
          data[2] = 0;
          data[3] = 0;
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_MULTI_7B_WR, data );
          //sleep_ms( 1 );
          usleep( 300 );

          // Read the GBTx register content
          recv_trid = 254;
          fup.uploadScaFrame( elinknr, &recv_trid, i2c_dev, 2,
                              SCA_I2C_SINGLE_7B_RD, data );
          //sleep_ms( 1 );
          usleep( 200 );
        }
    }
  else
    {
      // Write either a single register or
      // multiple registers with the file's contents
      u8  regbytes[512];
      int reg_i;
      if( write_reg )
        {
          reg_addr_min = reg_addr;
          reg_addr_max = reg_addr + 1;
          regbytes[0] = databyte;
        }
      else
        {
          // Read the file contents
          reg_i = read_txt_file( filename, regbytes, sizeof(regbytes) );
          if( reg_i > 0 )
            {
              cout << "Writing registers 0-" << reg_i-1 << endl;
              reg_addr_min = 0;
              reg_addr_max = reg_i;
            }
          else
            {
              // Something wrong: write nothing
              reg_addr_min = 0;
              reg_addr_max = 0;
            }
        }
      reg_i = 0;
      for( int reg=reg_addr_min; reg<reg_addr_max; ++reg, ++reg_i )
        {
          // The bytes to write: the GBTx register address + databyte
          data[0] = (u8) ((reg & 0xFF00) >> 8);
          data[1] = (u8) (reg & 0xFF);
          data[2] = (u8) 0;
          data[3] = (u8) (regbytes[reg_i] & 0xFF);
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_WR_DATA0, data );
          //sleep_ms( 1 );

          // DEBUG: read it back
          //recv_trid = 253;
          //fup.uploadScaFrame( elinknr, &recv_trid, i2c_dev, 4,
          //                  SCA_I2C_RD_DATA0, data );
          //sleep_ms( 1 );

          // Execute writing the (3) bytes as preset above
          data[0] = 0;
          data[1] = i2c_addr;
          data[2] = 0;
          data[3] = 0;
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_MULTI_7B_WR, data );
          //sleep_ms( 1 );
          usleep( 500 );
        }
    }

  // -------------------------------------------------------------------------
  // Extract the GBT-SCA replies from the datablock(s) received
  // and extract the I2C reads from those replies;
  // NB: replies with TRID=0xFE are the ones we're interested in.

  // Do not receive replies? In that case we're done
  if( frecv == 0 )
    {
      fup.stop();
      return 0;
    }

  FlxParser fparser;
  uint8_t  *chunkdata = 0;
  uint32_t  size;
  int       reg_count = 0, reg = reg_addr_min;
  fparser.setReceiver( frecv );
  while( fparser.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
    {
      if( size == 0 ) continue;

      // Find chunks with SCA replies containing
      // our 'special' Transaction ID
      if( size == 12 && chunkdata[2] == 0xFE ) // Transaction ID
        {
          // Display chunkdata (DEBUG)
          //for( uint32_t i=0; i<size; ++i )
          //  cout << " " << setw(2) << (uint32_t) chunkdata[i];
          //cout << endl;

          // Extract the I2C byte we're interested in
          if( reg_addr != -1 )
            {
              cout << setfill(' ') << dec
                   << "Reg " << reg << ": "
                   << setfill('0') << hex
                   << setw(2) << (uint32_t) chunkdata[6]
                   << endl;
            }
          else
            {
              if( (reg & 0xF) == 0 )
                cout << endl << setfill(' ') << dec
                     << setw(3) << reg << ":";
              cout << " " << setfill('0') << hex
                   << setw(2) << (uint32_t) chunkdata[6];
            }

          ++reg;
          ++reg_count;
        }
    }

  if( reg_addr == -1 )
    cout << endl;

  if( reg_count > 1 )
    cout << "Read " << dec << reg_count << " register values" << endl;

  //cout << endl << dec << "Processed " << blocknr << " blocks" << endl;

  fup.stop();
  if( frecv )
    frecv->stop();

  return 0;
}

// ----------------------------------------------------------------------------

int read_txt_file( string filename, u8 *regbytes, int size )
{
  int  reg_i   = 0;
  int  max_reg = size;

  ifstream ifs( filename );
  if( !ifs.good() )
    {
      cout << "### Failed to open file " << filename << endl;
      return 0;
    }

  char line[64];
  int line_no = 0;
  unsigned int x;
  while( ifs.good() )
    {
      // Read line by line, expecting at most a single value per line
      ifs.getline( line, sizeof(line) );
      if( !ifs.eof() ) ++line_no;
      if( sscanf( line, "%x", &x ) == 1 )
        {
          if( x > 255 )
            {
              cout << "### Register value out-of-range: 0x"
                   << hex << x << dec << endl;
              break;
            }
          if( reg_i == max_reg )
            {
              cout << "### Max number of register values exceeded: "
                   << max_reg << endl;
              break;
            }
          regbytes[reg_i] = x;
          ++reg_i;
        }
    }

  if( ifs.eof() )
    {
      // File read up to end-of-file
      cout << "File " << filename << ", lines: " << line_no
           << ", registers: " << reg_i << endl;
    }
  else
    {
      // File not read till end-of-file
      cout << "### Error file " << filename
           << ", near line " << line_no << endl;
      reg_i = 0;
    }

  // Return the number of register values read
  return reg_i;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fgbtxconf version " << hex << VERSION_ID << dec << endl <<
    "Tool to read or write GBTX registers via an I2C-channel "
    "of a GBT-SCA chip,\n"
    "connected to any FLX-device GBT (2-bit HDLC) E-link:\n"
    "read or write a single byte from or to the given GBTX register address\n"
    "or write to multiple consecutive GBTX registers using the contents "
    "of a file.\n"
    "(i.e. ASCII file: 1 (register) byte value (hex) per line,\n"
    " e.g. the 'TXT' file generated by the GBTXProgrammer tool).\n"
    "Provide a file name *or* use option -a with an optional additional "
    "byte value\n"
    "to read resp. write a single GBTX register or, without option -a, "
    "to read all registers.\n"
    "Usage:\n"
    " fgbtxconf [-h|V] [-d <devnr>] [-e <elink>] "
    "[-G <gbt> [-g <group> -p <path>]] [-R] [-r] [-W]\n"
    "        -C <ichan> -I <iaddr> -a <addr> [<byte>] | <filename>\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number (default: 7=EC).\n"
    "  -p <path>  : E-path number (default: 7=EC).\n"
    "  -R         : Reset GBT-SCA.\n"
    "  -r         : Do not receive and display the GBT-SCA replies.\n"
    "  -W         : Read writable registers only (default: all).\n"
    "  -C <ichan> : GBT-SCA I2C channel number.\n"
    "  -I <iaddr> : GBTX I2C address (hex).\n"
    "  -a <addr>  : GBTX register address (decimal or hex, 0x.. or x..) "
    "to read or write.\n"
    " <byte>      : Byte value (decimal or hex, 0x.. or x..) to write to GBTX register <addr> "
    "(option -a).\n"
    " <filename>  : Name of file with GBTX register data to write to "
    "consecutive registers.\n"
    "=> Examples:\n"
    "Read all registers of GBTX (I2C address 1) connected to GBT-SCA "
    "I2C-channel 0,\n"
    "GBT-SCA connected to FLX-device GBT link 3 EC-link:\n"
    "  fgbtxconf -G 3 -I 1 -C 0 (or: fgbtxconf -e ff -I 1 -C 0)\n"
    "Read GBTX register 32 (0x20):\n"
    "  fgbtxconf -G 3 -I 1 -C 0 -a 32 (or: fgbtxconf -G 3 -I 1 -C 0 -a 0x20)\n"
    "Write 0xA5 to GBTX register 32 (0x20):\n"
    "  fgbtxconf -G 3 -I 1 -C 0 -a 32 A5\n"
    "Write contents of GBTX-conf.txt to GBTX registers:\n"
    "  fgbtxconf -G 3 -I 1 -C 0 GBTX-conf.txt\n";
}

// ----------------------------------------------------------------------------
