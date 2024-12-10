#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>
#include <string.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x21011500; // -I/a/A decimal or hex; 10-bit mode
//const int VERSION_ID = 0x18101200; // Use FlxDataChecker::displayDataRecvd()
//const int VERSION_ID = 0x17092000;
//const int VERSION_ID = 0x17090200;

#define sleep_ms(x) usleep((x)*1000)

void display_replies( FlxReceiver *recvr,
                      uint64_t     timestamp_sent,
                      int          elinknr,
                      string       title = string() );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr      = 0;
  int  dma_write   = -1;// Autoselect FromHost DMA controller index
  int  dma_read    = 0;
  int  gbtnr       = -1;
  int  egroupnr    = 7; // EC-link
  int  epathnr     = 7; // EC-link
  int  elinknr     = -1;
  bool receive     = true;
  bool use_intrpt  = false;
  int  i2c_rate    = 4; // 400 KHz
  int  i2c_chan    = -1;
  int  i2c_addr    = -1;
  int  i2c_raddr   = -1;
  int  i2c_raddr_sz= 0;
  int  i2c_rval    = -1;
  int  i2c_rval_sz = 0;
  int  i2c_nbytes  = 0;
  bool i2c_read    = true; // An I2C read (or write) operation
  int  i2c_read_sz = 1;
  bool i2c_enable  = true;
  bool i2c_disable = false;
  bool i2c_10b_mode= false;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "a:A:C:d:De:EG:g:hI:p:r:R:tV")) != -1 )
    {
      switch( opt )
        {
        case 'a':
          // I2C register address (1-byte)
          if( optarg[0] == '0' && (optarg[1] == 'x' || optarg[1] == 'X') )
            {
              if( sscanf( optarg, "%x", &x ) != 1 )
                arg_error( 'a' );
              i2c_raddr = x;
            }
          else
            {
              if( sscanf( optarg, "%d", &i2c_raddr ) != 1 )
                arg_error( 'a' );
            }
          if( i2c_raddr < 0 || i2c_raddr > 255 ) // One byte
            arg_range( 'a', 0, 255 );
          i2c_raddr_sz = 1;
          break;
        case 'A':
          // I2C register address (2-byte)
          if( optarg[0] == '0' && (optarg[1] == 'x' || optarg[1] == 'X') )
            {
              if( sscanf( optarg, "%x", &x ) != 1 )
                arg_error( 'A' );
              i2c_raddr = x;
            }
          else
            {
              if( sscanf( optarg, "%d", &i2c_raddr ) != 1 )
                arg_error( 'A' );
            }
          if( i2c_raddr < 0 || i2c_raddr > 65535 ) // Two bytes
            arg_range( 'A', 0, 65535 );
          i2c_raddr_sz = 2;
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
        case 'D':
          i2c_disable = true;
          break;
        case 'e':
          // E-link number
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elinknr = x;
          if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
          break;
        case 'E':
          i2c_enable = false;
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
          if( optarg[0] == '0' && (optarg[1] == 'x' || optarg[1] == 'X') )
            {
              if( sscanf( optarg, "%x", &x ) != 1 )
                arg_error( 'I' );
              i2c_addr = x;
            }
          else
            {
              if( sscanf( optarg, "%d", &i2c_addr ) != 1 )
                arg_error( 'I' );
            }
          // NB: for 10-bit I2C address mode may have to allow a larger range
          if( i2c_addr < 0 || i2c_addr > 1023 )
            arg_range( 'I', 0, 1023 );
          break;
        case 'p':
          // E-path number
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'r':
          // I2C number of bytes to read in a read operation
          if( sscanf( optarg, "%d", &i2c_read_sz ) != 1 )
            arg_error( 'r' );
          if( i2c_read_sz < 1 || i2c_read_sz > 16 )
            arg_range( 'r', 1, 16 );
          break;
        case 'R':
          // I2C data transfer rate (in multiples of 100KHz)
          if( sscanf( optarg, "%d", &i2c_rate ) != 1 )
            arg_error( 'R' );
          if( !(i2c_rate == 1 || i2c_rate == 2 ||
                i2c_rate == 4 || i2c_rate == 10) )
            {
              cout << "### -R: argument not one of [1,2,4,10]*100KHz" << endl;
              return 1;
            }
          break;
        case 't':
          i2c_10b_mode = true;
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

  // Recheck I2C address to distinguish 7-bit and 10-bit addressing
  if( !i2c_10b_mode && i2c_addr > 127 )
    arg_range( 'I', 0, 127 );

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
  // I2C channel and I2C address and I2C register address
  //if( i2c_chan == -1 || i2c_addr == -1 || i2c_raddr == -1 )
  if( i2c_chan == -1 || i2c_addr == -1 )
    {
      if( i2c_chan == -1 )
        cout << "### Provide a GBT-SCA I2C channel number (-C)" << endl;
      if( i2c_addr == -1 )
        cout << "### Provide an I2C address (-I)" << endl;
      //if( i2c_raddr == -1 )
      //  cout << "### Provide an I2C register address (-a or -A)" << endl;
      return 1;
    }

  // Value to write ? (a hex value up to 4 bytes is supported)
  if( optind < argc )
    {
      if( strlen(argv[optind]) > 4*2 )
        {
          cout << "### Value to write cannot exceed 4 bytes" << endl;
          return 1;
        }

      // Expect a value to write, number of bytes determined by string length
      i2c_rval_sz = (strlen( argv[optind] ) + 1)/2;
      unsigned int x;
      if( sscanf( argv[optind], "%x", &x ) == 1 )
        {
          i2c_rval = x;
          i2c_read = false;
        }
      else
        {
          cout << "### Invalid data write value" << endl;
          return 1;
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

      frecv->setUseInterrupt( use_intrpt );
    }

  // -------------------------------------------------------------------------
  // Connect

  if( !fup.scaConnect( elinknr ) )
    cout << "###GBT-SCA connect: " << fup.errorString() << endl;
  display_replies( frecv, fup.timestamp(), elinknr, "GBT-SCA connect" );

  u8  data[4];
  int trid = 1;

  // -------------------------------------------------------------------------
  // Enable

  int i2c_dev = SCA_DEV_I2C + i2c_chan;
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

  if( i2c_enable )
    {
      // ###NB: Should read and modify the current channel enable mask
      // ...

      // Enable the selected I2C channel
      data[0] = 0x00;
      data[1] = 1 << (i2c_dev & 0x7);
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2, cmd, data );
      sleep_ms( 1 );

      // Read I2C channel status
      fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_RD_STAT, data );

      display_replies( frecv, fup.timestamp(), elinknr,
                       "I2C-port enable+status" );
    }

  // -------------------------------------------------------------------------
  // Configure

  if( i2c_rate == 2 )
    i2c_rate = SCA_I2C_CTRL_200K;
  else if( i2c_rate == 4 )
    i2c_rate = SCA_I2C_CTRL_400K;
  else if( i2c_rate == 10 )
    i2c_rate = SCA_I2C_CTRL_1M;
  else
    i2c_rate = SCA_I2C_CTRL_100K;

  // Number of bytes to write in the I2C write part:
  // size of register address plus number of bytes to write (if any)
  i2c_nbytes = i2c_raddr_sz + i2c_rval_sz;

  // Write I2C channel control: I2C speed and payload number of bytes
  // (i.e. the 1- or 2-byte GBTx register address
  //  plus -optionally- the bytes to write)
  data[0] = 0;
  data[1] = i2c_rate | (i2c_nbytes << 2);
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_WR_CTRL, data );

  // Read I2C channel control (optional)
  //data[0] = 0x00;
  //data[1] = 0x00;
  //fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_RD_CTRL, data );

  // Read I2C channel status (optional)
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_RD_STAT, data );

  display_replies( frecv, fup.timestamp(), elinknr, "I2C configure+status" );

  // -------------------------------------------------------------------------
  // The I2C operation

  // Display info about the intended operation
  cout << endl
       << "Operation: SCA@Elink" << hex << setfill('0') << uppercase
       << elinknr << " I2C-chan=" << dec << i2c_chan << hex
       << ": I2C-addr=0x" << (i2c_10b_mode ? setw(3) : setw(2)) << i2c_addr;
  if( i2c_10b_mode )
    cout << " (10b-mode)";
  if( i2c_raddr_sz > 0 )
    cout << " reg=0x" << setw(i2c_raddr_sz*2) << i2c_raddr;
  else
    cout << ", NO register addr";
  cout << dec << endl;
  if( i2c_read )
    cout << "           read " << i2c_read_sz << " bytes" << endl;
  else
    cout << "           write " << i2c_rval_sz << " bytes: 0x"
         << hex << setw(i2c_rval_sz*2) << i2c_rval << dec << endl;

  // Fill I2C data transmit buffer,
  // i.e. the register address plus additional byte(s) to write
  if( i2c_raddr_sz == 2 )
    {
      data[0] = (u8) ((i2c_raddr & 0xFF00) >> 8);
      data[1] = (u8) (i2c_raddr & 0xFF);
      if( i2c_rval_sz >= 1 )
        data[3] = (u8) (i2c_rval & 0xFF);
      else
        data[3] = 0;
      if( i2c_rval_sz >= 2 )
        data[2] = (u8) ((i2c_rval & 0xFF00) >> 8);
      else
        data[2] = 0;
      fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                          SCA_I2C_WR_DATA0, data );

      if( i2c_rval_sz >= 3 )
        {
          data[1] = (u8) ((i2c_rval & 0xFF0000) >> 16);
          if( i2c_rval_sz >= 4 )
            data[0] = (u8) ((i2c_rval & 0xFF000000) >> 24);
          else
            data[0] = 0;
          data[3] = 0;
          data[2] = 0;
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_WR_DATA1, data );
        }
    }
  else if( i2c_raddr_sz == 1 )
    {
      data[1] = (u8) (i2c_raddr & 0xFF);
      if( i2c_rval_sz >= 1 )
        data[0] = (u8) (i2c_rval & 0xFF);
      else
        data[0] = 0;
      if( i2c_rval_sz >= 2 )
        data[3] = (u8) ((i2c_rval & 0xFF00) >> 8);
      else
        data[3] = 0;
      if( i2c_rval_sz >= 3 )
        data[2] = (u8) ((i2c_rval & 0xFF0000) >> 16);
      else
        data[2] = 0;
      fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                          SCA_I2C_WR_DATA0, data );

      if( i2c_rval_sz >= 4 )
        {
          data[1] = (u8) ((i2c_rval & 0xFF000000) >> 24);
          data[0] = 0;
          data[3] = 0;
          data[2] = 0;
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_WR_DATA1, data );
        }
    }
  else
    {
      // No register address
      if( i2c_rval_sz >= 1 )
        data[1] = (u8) (i2c_rval & 0xFF);
      else
        data[1] = 0;
      if( i2c_rval_sz >= 2 )
        data[0] = (u8) ((i2c_rval & 0xFF00) >> 8);
      else
        data[0] = 0;
      if( i2c_rval_sz >= 3 )
        data[3] = (u8) ((i2c_rval & 0xFF0000) >> 16);
      else
        data[3] = 0;
      if( i2c_rval_sz >= 4 )
        data[2] = (u8) ((i2c_rval & 0xFF0000) >> 16);
      else
        data[2] = 0;
      fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                          SCA_I2C_WR_DATA0, data );
    }

  // Write the bytes (for multi-byte I2C read or write), if any
  if( i2c_nbytes > 0 )
    {
      if( i2c_10b_mode )
        {
          data[1] = i2c_addr & 0xFF;
          data[0] = 0x38 | ((i2c_addr & 0x300) >> 8);
          data[2] = 0; data[3] = 0;
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_MULTI_10B_WR, data );
        }
      else
        {
          data[1] = i2c_addr;
          data[0] = 0; data[2] = 0; data[3] = 0;
          fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                              SCA_I2C_MULTI_7B_WR, data );
        }

      if( i2c_read )
        display_replies( frecv, fup.timestamp(), elinknr,
                         "I2C write (reg addr)" );
      else
        display_replies( frecv, fup.timestamp(), elinknr,
                         "I2C write (reg addr+data)" );
    }

  // Read the bytes, if any
  if( i2c_read )
    {
      i2c_nbytes = i2c_read_sz;
      //cout << "nbytes=" << i2c_nbytes << endl;

      fup.resetScaFrames();

      if( i2c_nbytes == 1 )
        {
          // Read the resulting byte
          cout << "\nData (single) in TRID=" << trid;
          if( i2c_10b_mode )
            {
              data[1] = i2c_addr & 0xFF;
              data[0] = 0x38 | ((i2c_addr & 0x300) >> 8);
              data[2] = 0; data[3] = 0;
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_SINGLE_10B_RD, data );
            }
          else
            {
              data[1] = i2c_addr;
              data[0] = 0; data[2] = 0; data[3] = 0;
              //fup.uploadScaFrame( elinknr, &trid, i2c_dev,
              //                    4, SCA_I2C_SINGLE_7B_RD, data );
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_SINGLE_7B_RD, data );
            }
        }
      else
        {
          // Write I2C channel control: I2C speed and payload number of bytes
          data[0] = 0;
          data[1] = i2c_rate | (i2c_nbytes << 2);
          //fup.uploadScaFrame( elinknr, &trid, i2c_dev,
          //                    2, SCA_I2C_WR_CTRL, data );
          fup.addScaFrame( &trid, i2c_dev, 2, SCA_I2C_WR_CTRL, data );
          //sleep_ms( 1 );

          // Read the resulting bytes
          if( i2c_10b_mode )
            {
              data[1] = i2c_addr & 0xFF;
              data[0] = 0x38 | ((i2c_addr & 0x300) >> 8);
              data[2] = 0; data[3] = 0;
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_MULTI_10B_RD, data );
            }
          else
            {
              data[1] = i2c_addr;
              data[0] = 0; data[2] = 0; data[3] = 0;
              //fup.uploadScaFrame( elinknr, &trid, i2c_dev,
              //                    4, SCA_I2C_MULTI_7B_RD, data );
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_MULTI_7B_RD, data );
            }
          fup.addScaDelayUs( 500 );
          //sleep_ms( 1 );

          if( i2c_nbytes > 0 )
            {
              cout << "\nData in TRID=" << trid;
              //fup.uploadScaFrame( elinknr, &trid, i2c_dev,
              //                    4, SCA_I2C_RD_DATA3, data );
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_RD_DATA3, data );
            }
          if( i2c_nbytes > 4 )
            {
              cout << "," << trid;
              //fup.uploadScaFrame( elinknr, &trid, i2c_dev,
              //                    4, SCA_I2C_RD_DATA2, data );
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_RD_DATA2, data );
            }
          if( i2c_nbytes > 8 )
            {
              cout << "," << trid;
              //fup.uploadScaFrame( elinknr, &trid, i2c_dev,
              //                    4, SCA_I2C_RD_DATA1, data );
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_RD_DATA1, data );
            }
          if( i2c_nbytes > 12 )
            {
              cout << "," << trid;
              //fup.uploadScaFrame( elinknr, &trid, i2c_dev,
              //                    4, SCA_I2C_RD_DATA0, data );
              fup.addScaFrame( &trid, i2c_dev, 4, SCA_I2C_RD_DATA0, data );
            }
          cout << endl;
        }
      fup.uploadScaFrames( elinknr );

      display_replies( frecv, fup.timestamp(), elinknr, "I2C read" );
    }

  // -------------------------------------------------------------------------
  // Disable

  if( i2c_disable )
    {
      // ###NB: Should read and modify the current channel enable mask
      // ...

      data[0] = 0x00;
      data[1] = 0x00;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2, cmd, data );

      // Check if disabled: read I2C channel status
      //fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_RD_STAT, data );

      display_replies( frecv, fup.timestamp(), elinknr, "I2C disable" );
    }

  // -------------------------------------------------------------------------
  // Finish up

  fup.stop();
  if( frecv )
    frecv->stop();
  cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscai2c version " << hex << VERSION_ID << dec << endl <<
    "Tool to read or write from an I2C device register\n"
    "on any I2C port of a GBT-SCA chip connected to any FLX-device E-link\n"
    "(the latter given by options -G/g/p or option -e)\n"
    "Usage:\n"
    " fscai2c [-h|V] [-d <devnr>] [-e <elink>] [-G <gbt>] [-g <group>] "
    "[-p <path>]\n"
    "         [-R <rate>] -C <ichan> -I <iaddr> [-t] [-a|A <addr>] "
    "[-r <nbytes>]\n"
    "         [-D] [-E] [<value-to-write>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -C <ichan> : GBT-SCA I2C channel number.\n"
    "  -I <iaddr> : I2C device address (decimal or '0x..' for hexadecimal).\n"
    "  -t         : Use 10-bit I2C addressing mode.\n"
    "  -a|A <addr>: I2C register address ('a':1-byte, 'A':2-byte).\n"
    "               (decimal or '0x..' for hexadecimal).\n"
    "  -r <bytes> : Register content number of bytes (default: 1).\n"
    "  -R <rate>  : I2C transfer rate (1=100KHz, 2=200KHz, 4=400KHz, 10=1MHz, "
    "default: 4).\n"
    "  -D         : Disable GBT-SCA I2C port after operation "
    "(default: leave enabled)\n"
    "  -E         : Do *not* enable GBT-SCA I2C port at start of operation, "
    "assume it already is.\n"
    " <value-to-write>: hexadecimal value to write, the number of nibbles "
    "determining\n"
    "                   how many bytes to write.\n"
    "=> Examples:\n"
    "Read 2-byte register 6 from a device with I2C address 5 on GBT-SCA "
    "I2C channel 4\n"
    "connected to the EC channel of GBT #3:\n"
    "  fscai2c -G 3 -C 4 -I 5 -a 6 -r 2 \n"
    "Write 0x1234 to 2-byte register 6 from I2C device address 5 "
    "on GBT-SCA I2C channel 4\n"
    "connected to the EC channel of GBT #3:\n"
    "  fscai2c -G 3 -C 4 -I 5 -a 6 1234 \n";
}

// ----------------------------------------------------------------------------

void display_replies( FlxReceiver *recvr,
                      uint64_t     timestamp_sent,
                      int          elinknr,
                      string       title )
{
  if( recvr == 0 ) return;

  // Optionally have a title for the GBT-SCA replies displayed
  cout << endl;
  if( title.length() )
    cout << ">> " << title << " <<" << endl;

  // Go through the chunks of each block (skipping any TIMEOUT/NULL/OOB chunks)
  // displaying the data bytes, interpreting chunks as GBT-SCA replies
  FlxDataChecker flxChecker;
  int  timeout_us  = 4000;
  bool display_sca = true;
  flxChecker.displayDataRecvd( recvr, timeout_us, elinknr, display_sca );

  /*
  uint64_t ts = recvr->timestamp();
  if( ts > timestamp_sent )
    cout << "T = " << setw(3) << setfill(' ') << ((ts-timestamp_sent)+500)/1000
      //<<"." << setfill('0') << setw(3) << (ts-timestamp_sent)%1000
      //<< " (" << ts << " " << timestamp_sent << ")"
         << endl;
  else
    cout << "T = ??? " << ts << ", " << timestamp_sent << endl;
  */
}

// ----------------------------------------------------------------------------
