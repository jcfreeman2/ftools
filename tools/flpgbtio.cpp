#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "arg.h"
#include "lpgbt-regs-v0.h"
#include "lpgbt-regs-v1.h"
#include "ic.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x22010400;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int     opt;
  int     cardnr        = 0;
  int     linknr        = -1;
  int     dma_write     = -1; // Autoselect FromHost DMA controller index
  int     dma_read      = 0;
  bool    receive       = true;
  int     gpio_bit      = -1;
  int32_t gpio_val      = -1;
  int32_t gpio_dir      = -1;
  int32_t gpio_updown   = -1;
  int32_t gpio_pullena  = -1;
  int32_t gpio_strength = -1;
  int     i2c_addr      = 0x70;
  bool    use_ec        = false;
  bool    lpgbt_v1      = false;
  bool    debug         = false;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "1d:D:e:E:G:hi:I:o:S:U:VXZ")) != -1 )
    {
      switch( opt )
        {
        case '1':
          lpgbt_v1 = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          // DMA controller to use
          if( sscanf( optarg, "%d", &dma_read ) != 1 )
            arg_error( 'D' );
          if( dma_read < 0 || dma_read > 7 )
            arg_range( 'D', 0, 7 );
          break;
        case 'e':
          use_ec = true;
          break;
        case 'E':
          // GPIO PULLENA register value
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'E' );
          gpio_pullena = x;
          if( gpio_pullena < 0 || gpio_pullena > 0xFFFF )
            arg_range( 'E', 0, 0xFFFF );
          break;
        case 'G':
          // lpGBT link number
          if( sscanf( optarg, "%d", &linknr ) != 1 )
            arg_error( 'G' );
          if( linknr < 0 || linknr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          // GPIO bit index
          if( sscanf( optarg, "%d", &gpio_bit ) != 1 )
            arg_error( 'i' );
          if( gpio_bit < 0 || gpio_bit > 15 )
            arg_range( 'i', 0, 15 );
          break;
        case 'I':
          // lpGBT I2C address
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'I' );
          i2c_addr = x;
          if( i2c_addr < 0 || i2c_addr > 127 )
            arg_range_hex( 'I', 0, 127 );
          break;
        case 'o':
          // GPIO DIR register value
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'o' );
          gpio_dir = x;
          if( gpio_dir < 0 || gpio_dir > 0xFFFF )
            arg_range( 'o', 0, 0xFFFF );
          break;
        case 'S':
          // GPIO DRIVESTRENGTH register value
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'S' );
          gpio_strength = x;
          if( gpio_strength < 0 || gpio_strength > 0xFFFF )
            arg_range( 'S', 0, 0xFFFF );
          break;
        case 'U':
          // GPIO UPDOWN register value
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'U' );
          gpio_updown = x;
          if( gpio_updown < 0 || gpio_updown > 0xFFFF )
            arg_range( 'U', 0, 0xFFFF );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'X':
          debug = true;
          break;
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
        cout << "### Provide a link number (-G)" << endl;
      if( i2c_addr < 0 )
        cout << "### Provide an I2C address (-I)" << endl;
      return 1;
    }

  // GPIO value to write
  if( optind < argc )
    {
      // Expect a value to write
      if( sscanf( argv[optind], "%x", &x ) == 1 )
        {
          gpio_val = x;
          if( gpio_bit != -1 )
            {
              // Single bit value
              if( gpio_val != 0 && gpio_val != 1 )
                {
                  cout << "### GPIO value out-of-range 0..1: "
                       << gpio_val << endl;
                }
            }
          else
            {
              // 16-bit value
              if( gpio_val < 0 || gpio_val > 0xFFFF )
                {
                  cout << "### GPIO value out-of-range 0..0xFFFF: "
                       << gpio_val << endl;
                  return 1;
                }
            }
        }
      else
        {
          cout << "### Invalid GPIO value provided" << endl;
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

  FlxReceiver *frecvr = 0;
  if( receive )
    {
      frecvr = new FlxReceiver( cardnr, 64*1024*1024, dma_read );
      if( frecvr->hasError() )
        {
          cout << "### " << frecvr->errorString() << endl;
          frecvr->stop();
          return 1;
        }
      /*
      cout << "Opened FLX-device " << cardnr
           << ", firmw " << frecvr->firmwareVersion()
           << ", " << frecvr->numberOfChans()
           <<  " chans (cmem bufsize="
           << frecvr->bufferSize() << ", receive: "
           << "DMA #" << dma_read << ")" << endl;
      */
    }

  int     reg_addr;
  uint8_t reg_vals[2]; // Read/write 2 lpGBT registers at once

  // -------------------------------------------------------------------------
  // Write/read GPIO pull-up/down

  // Write PIOUPDOWN registers
  if( gpio_updown != -1 )
    {
      reg_addr = (lpgbt_v1 ? PIOUPDOWNH_V1 : PIOUPDOWNH_V0);
      reg_vals[0] = (uint8_t) ((gpio_updown >> 8) & 0xFF);
      reg_vals[1] = (uint8_t) (gpio_updown & 0xFF);
      if( !writeIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                        use_ec, lpgbt_v1, debug ) )
        return 1;
    }

  // Read PIOUPDOWN registers
  reg_addr = (lpgbt_v1 ? PIOUPDOWNH_V1 : PIOUPDOWNH_V0);
  if( !readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                   use_ec, lpgbt_v1, debug ) )
    return 1;

  gpio_updown = ((uint32_t) reg_vals[0] << 8) | (uint32_t) reg_vals[1] ;

  // -------------------------------------------------------------------------
  // Write/read GPIO pull-up/down enable

  // Write PIOPULLENA registers
  if( gpio_pullena != -1 )
    {
      reg_addr = (lpgbt_v1 ? PIOPULLENAH_V1 : PIOPULLENAH_V0);
      reg_vals[0] = (uint8_t) ((gpio_pullena >> 8) & 0xFF);
      reg_vals[1] = (uint8_t) (gpio_pullena & 0xFF);
      if( !writeIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                        use_ec, lpgbt_v1, debug ) )
        return 1;
    }

  // Read PIOPULLENA registers
  reg_addr = (lpgbt_v1 ? PIOPULLENAH_V1 : PIOPULLENAH_V0);
  if( !readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                   use_ec, lpgbt_v1, debug ) )
    return 1;

  gpio_pullena = ((uint32_t) reg_vals[0] << 8) | (uint32_t) reg_vals[1] ;

  // -------------------------------------------------------------------------
  // Write/read GPIO drive strength

  // Write PIODRIVESTRENGTH registers
  if( gpio_strength != -1 )
    {
      reg_addr = (lpgbt_v1 ? PIODRIVESTRENGTHH_V1 : PIODRIVESTRENGTHH_V0);
      reg_vals[0] = (uint8_t) ((gpio_strength >> 8) & 0xFF);
      reg_vals[1] = (uint8_t) (gpio_strength & 0xFF);
      if( !writeIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                        use_ec, lpgbt_v1, debug ) )
        return 1;
    }

  // Read PIODRIVESTRENGTH registers
  reg_addr = (lpgbt_v1 ? PIODRIVESTRENGTHH_V1 : PIODRIVESTRENGTHH_V0);
  if( !readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                   use_ec, lpgbt_v1, debug ) )
    return 1;

  gpio_strength = ((uint32_t) reg_vals[0] << 8) | (uint32_t) reg_vals[1] ;

  // -------------------------------------------------------------------------
  // Write/read GPIO direction

  // Write PIODIR registers
  if( gpio_dir != -1 )
    {
      reg_addr = (lpgbt_v1 ? PIODIRH_V1 : PIODIRH_V0);
      reg_vals[0] = (uint8_t) ((gpio_dir >> 8) & 0xFF);
      reg_vals[1] = (uint8_t) (gpio_dir & 0xFF);
      if( !writeIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                        use_ec, lpgbt_v1, debug ) )
        return 1;
    }

  // Read PIODIR registers
  reg_addr = (lpgbt_v1 ? PIODIRH_V1 : PIODIRH_V0);
  if( !readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                   use_ec, lpgbt_v1, debug ) )
    return 1;

  gpio_dir = ((uint32_t) reg_vals[0] << 8) | (uint32_t) reg_vals[1] ;

  if( gpio_bit != -1 && gpio_val != -1 )
    {
      // Set the *selected* GPIO bit to output, if not already
      if( (gpio_dir & (1 << gpio_bit)) == 0 )
        {
          gpio_dir |= (1 << gpio_bit);

          // Write PIODIR registers
          reg_addr = (lpgbt_v1 ? PIODIRH_V1 : PIODIRH_V0);
          reg_vals[0] = (uint8_t) ((gpio_dir >> 8) & 0xFF);
          reg_vals[1] = (uint8_t) (gpio_dir & 0xFF);
          if( !writeIcRegs( &fup, frecvr, linknr, i2c_addr,
                            reg_addr, 2, reg_vals,
                            use_ec, lpgbt_v1, debug ) )
            return 1;
        }
    }

  // -------------------------------------------------------------------------
  // Read current PIOOUT registers
  uint32_t gpio_out;

  reg_addr = (lpgbt_v1 ? PIOOUTH_V1 : PIOOUTH_V0);
  if( !readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                   use_ec, lpgbt_v1, debug ) )
    return 1;

  gpio_out = ((uint32_t) reg_vals[0] << 8) | (uint32_t) reg_vals[1] ;

  // -------------------------------------------------------------------------
  // Write new value to selected GPIO output or all outputs

  if( gpio_val != -1 )
    {
      if( gpio_bit != -1 )
        {
          // Single bit
          if( gpio_val == 1 )
            gpio_out |= (1 << gpio_bit);
          else
            gpio_out &= ~(1 << gpio_bit);
        }
      else
        {
          // 32 bits
          gpio_out = gpio_val;
        }

      // Write PIOOUT registers
      reg_addr = (lpgbt_v1 ? PIOOUTH_V1 : PIOOUTH_V0);
      reg_vals[0] = (uint8_t) ((gpio_out >> 8) & 0xFF);
      reg_vals[1] = (uint8_t) (gpio_out & 0xFF);
      if( !writeIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                        use_ec, lpgbt_v1, debug ) )
        return 1;

      // Re-read PIOOUT registers
      reg_addr = (lpgbt_v1 ? PIOOUTH_V1 : PIOOUTH_V0);
      if( !readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                       use_ec, lpgbt_v1, debug ) )
        return 1;

      gpio_out = ((uint32_t) reg_vals[0] << 8) | (uint32_t) reg_vals[1] ;
    }

  // -------------------------------------------------------------------------
  // Read current PIOIN registers
  uint32_t gpio_in;

  reg_addr = (lpgbt_v1 ? PIOINH_V1 : PIOINH_V0);
  if( !readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 2, reg_vals,
                   use_ec, lpgbt_v1, debug ) )
    return 1;

  gpio_in = ((uint32_t) reg_vals[0] << 8) | (uint32_t) reg_vals[1] ;

  // -------------------------------------------------------------------------
  // Display

  cout << hex << setfill('0')
       << "GPIO pullena = 0x" << setw(4) << gpio_pullena
       << ", pullup = 0x"     << setw(4) << gpio_updown
       << ", strength = 0x"   << setw(4) << gpio_strength
       << endl
       << "GPIO out = 0x" << setw(4) << gpio_out
       << ", in = 0x"     << setw(4) << gpio_in
       << ", dir = 0x"    << setw(4) << gpio_dir
       << endl;

  if( gpio_bit != -1 )
    {
      cout << "Bit " << dec << gpio_bit << ": ";
      if( (gpio_dir & (1 << gpio_bit)) != 0 )
        {
          // It's an output
          cout << ((gpio_out & (1 << gpio_bit)) ? "1" : "0");
          cout << " (OUT)";
        }
      else
        {
          // It's an input
          cout << ((gpio_in & (1 << gpio_bit)) ? "1" : "0");
          cout << " (IN)";
        }
      cout << endl;
    }

  // -------------------------------------------------------------------------
  // Finish up

  fup.stop();
  if( frecvr )
    frecvr->stop();
  cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "flpgbtio version " << hex << VERSION_ID << dec << endl <<
    "Tool to write and/or read the lpGBT GPIO bits and direction register.\n"
    "Usage:\n"
    " flpgbtio [-h|V] [-D<dma>] [-d<devnr>] -G<gbt> -I<i2c> [-1] [-e]\n"
    "          [-i<bit>] [-o<dir>] [-E<ena>] [-U<up>] [-S<s>] "
    "[-X] [-Z] [<value>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device number (default: 0).\n"
    "  -D <dma>   : FLX-device DMA controller for receiving (default: 0).\n"
    "  -1         : Assume lpGBTv1 (default: lpGBTv0)\n"
    "  -e         : Use the EC-channel (default: IC-channel).\n"
    "  -G <gbt>   : lpGBT link number.\n"
    "  -I <i2c>   : lpGBT I2C address.\n"
    "  -i <bit>   : Read or write GPIO bit number <bit> (default: all).\n"
    "               NB: if a single I/O pin is written to, its direction bit\n"
    "                   is set to output (independent of option -o).\n"
    "  -o <dir>   : Set GPIO direction register to value <dir> (16-bit hex).\n"
    "  -E <ena>   : Set GPIO pull-up/down enable register to value <dir> (16-bit hex).\n"
    "  -U <up>    : Set GPIO pull-up/down register to value <dir> (16-bit hex).\n"
    "  -S <s>     : Set GPIO drive strength register to value <dir> (16-bit hex).\n"
    "  -X         : Debug mode: display bytes of received frames.\n"
    "  -Z         : Do NOT receive and process/display replies.\n"
    " <value>     : Value to write (0 or 1 for a single GPIO bit, or up to "
    "0xFFFF otherwise, hexadecimal);\n"
    "               if no value is provided a read operation is performed.\n";
}

// ----------------------------------------------------------------------------
