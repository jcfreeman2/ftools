/* ----------------------------------------------------------------------------
File   : fflash.cpp

Descr  : Commandline tool to load a firmware image from one of the (4)
         partitions in an FLX-712's onboard flash memory into the card's FPGA,
         issueing commands to a host system I2C bus to achieve this.
         (The source of this code has been extracted from the original fflash
          tool source code, which combined both flash memory programming/
          verifying/erasing/dumping and *loading* of a firmware partition
          into the FPGA; the former functionality has been moved to
          the fflashprog tool).

         For documentation see:
         https://gitlab.cern.ch/atlas-tdaq-felix/felix-fpga-flash/blob/
         master/docs/Operation_Manual_for_Flash_on_BNL711.pdf

Usage  : See usage() below.

History:
09APR2020; HenkB; Code extracted/removed from the original fflash.cpp
                  which has been renamed to fflashprog.cpp
                  (for the history of changes, see there).
02JUL2020; HenkB; Remove option -c: no card selection taking place;
                  -d remains in use in combination with ipmitool -P option.
08JUL2020; HenkB; Increase prog-done time-out some more: from 5 to 7 secs.
08FEB2021; HenkB; Make prog-done time-out configurable: option -T.
---------------------------------------------------------------------------- */

#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h> // For getopt()

#include "version.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

bool load_firmware      ( int flashnr, int i2cbusnr,
                          int i2caddr_sw, int i2caddr_uc, int timeout );
bool pulse_initb        ( int i2cbusnr, int i2caddr_sw, int i2caddr_uc );
bool set_i2cswitch_riser( int i2cbusnr, int i2caddr_sw, int chan );
bool set_i2cswitch      ( int i2cbusnr, int i2caddr_sw, int val );

bool load_firmware_ipmi ( int flashnr,
                          int i2caddr_sw, int i2caddr_uc, int timeout );
bool pulse_initb_ipmi   ( int i2caddr_sw, int i2caddr_uc );
bool set_i2cswitch_ipmi ( int i2caddr_sw, int val );

void usage_extended     ( );

std::string my_system   ( const string &command );

// Quiet mode (if true only errors will be displayed):
bool be_quiet = false;
std::ostringstream DevNull;
inline std::ostream &out()
{
  return( be_quiet ? DevNull : cout ); 
}

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x21020800;
//const int VERSION_ID = 0x20070800;
//const int VERSION_ID = 0x20070200;
//const int VERSION_ID = 0x20040900;

// uC register selection identifiers
//const int PINC_REG  = 0x0b;
const   int DDRC_REG  = 0x10;
const   int PORTC_REG = 0x11;
const   int PIND_REG  = 0x0c;
const   int DDRD_REG  = 0x12;
const   int PORTD_REG = 0x13;

// Used for loading firmware:
string setstr    = "/usr/sbin/i2cset -y ";
string getstr    = "/usr/sbin/i2cget -y ";
string detectstr = "/usr/sbin/i2cdetect -y ";

//string setstr = "/opt/flx/i2cset.py ";
//string getstr = "/opt/flx/i2cget.py ";
//string setstr = "ipmitool raw 0x30 0x70 0xd5 ";
//string getstr = "ipmitool raw 0x30 0x70 0xd5 ";

// The ipmitool command line usage:
// ipmitool raw 0x30 0x70 0xd5 {Request Data}:
// Request data:
// Byte Data field
// ---- ----------
//  1   Riser ID. Depends on which Riser you use, such as RSC-W2-66 is 0x1b.
//  2   Riser location. Depends  on which SXB slot you use to insert Riser,
//      such as Riser is inserted on SXB1 so that this parameter is 0x01.
//  3   Device slot. Depends on which slot you use to insert the card on Riser,
//      such as the card is inserted on slot 1 so that this parameter is 0x01.
//  4   [7:1] – Slave Address
//      [0]   – reserved.
//  5   Read count. Number of bytes to read, 1 based. 0 = no bytes to read.
// 6:N  Data to write.
//
// Response data:
// Byte Data field
// ---- ----------
//  1   Completion Code
//      83h = NAK on Write
// 2:M  Bytes read from specified slave address.
//      This field will be absent if the read count is 0.

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  devslot       = -1;
  int  flashnr       = -1;
  bool load_it       = false;
  bool initb_it      = false;
  bool usb_i2c       = false;
  bool ipmi_i2c      = false;
  bool use_sudo      = false;
  int  i2cchan_riser = 0;    // I2C channel on riser card I2C-switch (0 or 1)
  int  i2cbusnr      = 0;    // I2C-bus number
  int  i2caddr_sw    = 0x77; // I2C-switch I2C-address
  int  i2caddr_uc    = 0x67; // Microcontroller's I2C-address
  int  prog_timeout  = 7;    // Prog-done timeout in seconds

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "b:d:f:hILr:s:ST:u:UPqV")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &devslot ) != 1 )
            arg_error( 'd' );
          break;
        case 'f':
          // Flash partition number
          if( sscanf( optarg, "%d", &flashnr ) != 1 )
            arg_error( 'f' );
          if( flashnr < 0 || flashnr > 3 )
            arg_range( 'f', 0, 3 );
          break;
        case 'h':
          usage();
          usage_extended();
          return 0;
        case 'I':
          initb_it = true;
          break;
        case 'L':
          load_it = true;
          break;
        case 'b':
          // I2C-bus number
          if( sscanf( optarg, "%d", &i2cbusnr ) != 1 )
            arg_error( 'b' );
          if( i2cbusnr < 0 || i2cbusnr > 255 )
            arg_range( 'b', 0, 255 );
          break;
        case 'r':
          // Riser I2C-switch channel number
          if( sscanf( optarg, "%d", &i2cchan_riser ) != 1 )
            arg_error( 'r' );
          if( i2cchan_riser < 0 || i2cchan_riser > 1 )
            arg_range( 'r', 0, 1 );
          break;
        case 's':
          // I2C-switch I2C-address
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 's' );
          i2caddr_sw = x;
          if( i2caddr_sw < 0 || i2caddr_sw > 127 )
            arg_range( 's', 0, 127 );
          break;
        case 'T':
          // I2C-bus number
          if( sscanf( optarg, "%d", &prog_timeout ) != 1 )
            arg_error( 'T' );
          if( prog_timeout < 1 || prog_timeout > 100 )
            arg_range( 'T', 0, 100 );
          break;
        case 'u':
          // Microcontroller I2C-address
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'u' );
          i2caddr_uc = x;
          if( i2caddr_uc < 0 || i2caddr_uc > 127 )
            arg_range( 'u', 0, 127 );
          break;
        case 'S':
          use_sudo = true;
          break;
        case 'U':
          usb_i2c = true;
          if( ipmi_i2c )
            {
              cerr << "### Choose either -U or -P option" << endl;
              return 1;
            }
          break;
        case 'P':
          ipmi_i2c = true;
          if( usb_i2c )
            {
              cerr << "### Choose either -U or -P option" << endl;
              return 1;
            }
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'q':
          be_quiet = true;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Option -d only in combination with ipmi_i2c
  if( devslot != -1 && ipmi_i2c == false )
    {
      // -d but not -P..
      cerr << "WARNING: option -d valid in combination with -P -> ignored"
           << endl;
    }

  if( load_it || initb_it )
    {
      if( usb_i2c )
        {
          setstr = "/opt/flx/i2cset.py ";
          getstr = "/opt/flx/i2cget.py ";
        }
      else if( ipmi_i2c )
        {
          // Additional 'ipmitool' variables
          int  ipmi_riserid  = 0x1B;
          int  ipmi_riserloc = 0x01;
          int  ipmi_devslot  = devslot;
          ostringstream oss;
          oss << "ipmitool raw 0x30 0x70 0xd5 ";
          oss << ipmi_riserid << " "
              << ipmi_riserloc << " "
              << ipmi_devslot << " ";
          setstr = oss.str();
          getstr = oss.str();
        }

      if( use_sudo )
        {
          setstr.insert( 0, "sudo " );
          getstr.insert( 0, "sudo " );
          detectstr.insert( 0, "sudo " );
        }

      if( !(usb_i2c || ipmi_i2c) )
        {
          // Set the riser's I2C-switch channel:
          // the I2C-switch's I2C-address is 0x70, hard-coded!
          if( set_i2cswitch_riser( i2cbusnr, 0x70, i2cchan_riser ) == false )
            return 1;
        }
    }

  if( initb_it )
    {
      // Send commands to the FLX-711/712 microcontroller (accessible via I2C)
      // to trigger a pulse on the INIT_B signal
      bool success;
      if( ipmi_i2c )
        success = pulse_initb_ipmi( (i2caddr_sw << 1), (i2caddr_uc << 1) );
      else
        success = pulse_initb( i2cbusnr, i2caddr_sw, i2caddr_uc );

      if( success )
        return 0;
      else
        return 1;
    }

  if( flashnr < 0 && load_it )
    {
      // Must provide a flash partition number to load
      cerr << "### Provide a flash partition number (option -f)" << endl;
      return 1;
    }

  // --------------------------------

  if( load_it )
    {
      // Send commands to the FLX-711/712 microcontroller (accessible via I2C)
      // to trigger it to load the firmware from the selected flash partition
      bool success;
      if( ipmi_i2c )
        success = load_firmware_ipmi( flashnr, (i2caddr_sw << 1),
                                      (i2caddr_uc << 1), prog_timeout );
      else
        success = load_firmware( flashnr, i2cbusnr, i2caddr_sw,
				 i2caddr_uc, prog_timeout );

      if( success )
        {
          // Added (5 Dec 2017): issue an INIT_B pulse
          if( ipmi_i2c )
            success = pulse_initb_ipmi( (i2caddr_sw << 1), (i2caddr_uc << 1) );
          else
            success = pulse_initb( i2cbusnr, i2caddr_sw, i2caddr_uc );
        }

      if( success )
        return 0;
      else
        return 1;
    }

  cout << "No operation..." << endl;
  return 0;
}

// ----------------------------------------------------------------------------

bool load_firmware( int flashnr, int i2cbusnr, int i2caddr_sw, int i2caddr_uc,
		    int prog_timeout )
{
  // Send commands to the FLX-711/712 microcontroller (accessible via I2C)
  // to trigger it to load the firmware from the selected flash partition
  int portval, ddrval;
  int i2cbus = i2cbusnr;
  ostringstream oss;

  out() << "=> Load firmware partition " << flashnr
        << " (I2C: bus=" << i2cbus << hex
        << ", switch=0x" << i2caddr_sw << ", uC=0x" << i2caddr_uc
        << "):" << endl << dec;

  // Select the I2C-switch channel connected to the uC
  if( set_i2cswitch( i2cbus, i2caddr_sw, 1 ) == false )
    return false;

  // Set I/Os: PROG_B low (bit 5), A26/25 partition selection (bits 4+3)
  portval = (0 << 5) | (flashnr << 3);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << PORTC_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Set I/Os direction: to output
  ddrval  = (1 << 5) | (3 << 3);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << DDRC_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Set I/Os: PROG_B high (trigger a rising edge)
  portval = (1 << 5) | (flashnr << 3);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << PORTC_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // ###THIS IS WEIRD; From Kai:
  // "It looks like for some unknown reason PORT C usage will affect
  //  PORT D status (for partition 2, we set PORT C bit 3 as '1',
  //  while FPGA_PROG_DONE is bit 3 of port D)."
  // To fix it, add here: DDRD.2 to 0
  ddrval  = (0 << 2);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << DDRD_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );

  // Instead of 'sleep' could use status of PIND bit 3:
  // is set to 1 when configuration is done
  //sleep( 5 );
  oss << getstr << i2cbus << " " << i2caddr_uc << " " << PIND_REG;
  out() << oss.str() << endl;
  string str;
  unsigned int x;
  int cnt = 0;
  bool prog_done = false;
  while( !prog_done && cnt < prog_timeout*10 )
    {
      usleep( 100000 ); // 100 ms
      str = my_system( oss.str() );
      ++cnt;
      if( sscanf( str.c_str(), "%x", &x ) != 1 )
        break;
      if( x & 0x08 ) // Status of PIND bit 3
        prog_done = true;
    }
  bool success;
  if( prog_done )
    {
      out() << "Prog done (time: " << cnt*100 << " ms)" << endl;
      success = true;
    }
  else
    {
      cerr << "###Prog failed (timeout: " << cnt*100 << " ms)" << endl;
      success = false;
    }
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Set I/Os direction: to input
  ddrval  = (0 << 5) | (0 << 3);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << DDRC_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Deselect the I2C-switch channel connected to the uC
  set_i2cswitch( i2cbus, i2caddr_sw, 0 );

  return success;
}

// ----------------------------------------------------------------------------

bool load_firmware_ipmi( int flashnr, int i2caddr_sw, int i2caddr_uc,
			 int prog_timeout )
{
  // Send commands to the FLX-711/712 microcontroller (accessible via I2C)
  // to trigger it to load the firmware from the selected flash partition
  int portval, ddrval;
  ostringstream oss;

  out() << "=> Load firmware partition " << flashnr
        << " (I2C via IPMI: " << hex
        << "switch=0x" << i2caddr_sw << ", uC=0x" << i2caddr_uc
        << "):" << endl << dec;

  // Select the I2C-switch channel connected to the uC
  if( set_i2cswitch_ipmi( i2caddr_sw, 1 ) == false )
    return false;

  // Set I/Os: PROG_B low (bit 5), A26/25 partition selection (bits 4+3)
  portval = (0 << 5) | (flashnr << 3);
  oss << setstr << i2caddr_uc << " 0 "
      << PORTC_REG << " 0 " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Set I/Os direction: to output
  ddrval  = (1 << 5) | (3 << 3);
  oss << setstr << i2caddr_uc << " 0 "
      << DDRC_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Set I/Os: PROG_B high (trigger a rising edge)
  portval = (1 << 5) | (flashnr << 3);
  oss << setstr << i2caddr_uc << " 0 "
      << PORTC_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // ###THIS IS WEIRD; From Kai:
  // "It looks like for some unknown reason PORT C usage will affect
  //  PORT D status (for partition 2, we set PORT C bit 3 as '1',
  //  while FPGA_PROG_DONE is bit 3 of port D)."
  // To fix it, add here: DDRD.2 to 0
  ddrval  = (0 << 2);
  oss << setstr << i2caddr_uc << " 0 "
      << DDRD_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );

  // Instead of 'sleep' could use status of PIND bit 3:
  // is set to 1 when configuration is done
  //sleep( 5 );
  oss << getstr << i2caddr_uc << " 1 " << PIND_REG;
  out() << oss.str() << endl;
  string str;
  unsigned int x;
  int cnt = 0;
  bool prog_done = false;
  while( !prog_done && cnt < prog_timeout*10 )
    {
      usleep( 100000 ); // 100 ms
      str = my_system( oss.str() );
      ++cnt;
      if( sscanf( str.c_str(), "%x", &x ) != 1 )
        break;
      if( x & 0x08 ) // Status of PIND bit 3
        prog_done = true;
    }
  bool success;
  if( prog_done )
    {
      out() << "Prog done (time: " << cnt*100 << " ms)" << endl;
      success = true;
    }
  else
    {
      cerr << "###Prog failed (timeout: " << cnt*100 << " ms)" << endl;
      success = false;
    }
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Set I/Os direction: to input
  ddrval  = (0 << 5) | (0 << 3);
  oss << setstr << i2caddr_uc << " 0 "
      << DDRC_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" ); // Reset ostringstream

  // Deselect the I2C-switch channel connected to the uC
  set_i2cswitch_ipmi( i2caddr_sw, 0 );

  return success;
}

// ----------------------------------------------------------------------------

bool pulse_initb( int i2cbusnr, int i2caddr_sw, int i2caddr_uc )
{
  // Send commands to the FLX-711/712 microcontroller (accessible via I2C)
  // to trigger a pulse on the INIT_B signal
  int portval, ddrval;
  int i2cbus = i2cbusnr;
  ostringstream oss;

  out() << "=> Pulse INIT_B "
        << "(I2C: bus=" << i2cbus << hex
        << ", switch=0x" << i2caddr_sw << ", uC=0x" << i2caddr_uc
        << "):" << endl << dec;

  // Select the I2C-switch channel connected to the uC
  if( set_i2cswitch( i2cbus, i2caddr_sw, 1 ) == false )
    return false;

  // PORTD.2 to 1
  portval = (1 << 2);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << PORTD_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // DDRD.2 to 1
  ddrval  = (1 << 2);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << DDRD_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // PORTD.2 to 0
  portval = (0 << 2);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << PORTD_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // PORTD.2 to 1
  portval = (1 << 2);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << PORTD_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // DDRD.2 to 0
  ddrval  = (0 << 2);
  oss << setstr << i2cbus << " " << i2caddr_uc << " "
      << DDRD_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );

  // Deselect the I2C-switch channel connected to the uC
  return set_i2cswitch( i2cbus, i2caddr_sw, 0 );
}

// ----------------------------------------------------------------------------

bool pulse_initb_ipmi( int i2caddr_sw, int i2caddr_uc )
{
  // Send commands to the FLX-711/712 microcontroller (accessible via I2C)
  // to trigger a pulse on the INIT_B signal
  int portval, ddrval;
  ostringstream oss;

  out() << "=> Pulse INIT_B "
        << "(I2C via IPMI: " << hex
        << "switch=0x" << i2caddr_sw << ", uC=0x" << i2caddr_uc
        << "):" << endl << dec;

  // Select the I2C-switch channel connected to the uC
  if( set_i2cswitch_ipmi( i2caddr_sw, 1 ) == false )
    return false;

  // PORTD.2 to 1
  portval = (1 << 2);
  oss << setstr << i2caddr_uc << " 0 "
      << PORTD_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // DDRD.2 to 1
  ddrval  = (1 << 2);
  oss << setstr << i2caddr_uc << " 0 "
      << DDRD_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // PORTD.2 to 0
  portval = (0 << 2);
  oss << setstr << i2caddr_uc << " 0 "
      << PORTD_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // PORTD.2 to 1
  portval = (1 << 2);
  oss << setstr << i2caddr_uc << " 0 "
      << PORTD_REG << " " << portval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );
  // DDRD.2 to 0
  ddrval  = (0 << 2);
  oss << setstr << i2caddr_uc << " 0 "
      << DDRD_REG << " " << ddrval;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 ) return false;
  oss.clear(); oss.str( "" );

  // Deselect the I2C-switch channel connected to the uC
  return set_i2cswitch_ipmi( i2caddr_sw, 0 );
}

// ----------------------------------------------------------------------------

bool set_i2cswitch_riser( int i2cbusnr, int i2caddr_sw, int chan )
{
  ostringstream oss;
  int i2cbus = i2cbusnr;

  out() << "=> Detecting riser card I2C-switch "
        << "(I2C: bus=" << i2cbus << ", address=0x" << hex
        << i2caddr_sw << "):" << endl;

  // Check if the expected I2C-address is actually present:
  // i2cdetect -y <busnr> <first> <last> | grep "--"
  // (results in an empty string when the I2C-address is found!)
  oss << detectstr << i2cbus << " " << i2caddr_sw << " " << i2caddr_sw
      << " | grep \"\\-\\-\"";
  out() << oss.str() << endl;
  std::string str = my_system( oss.str() );

  if( str.empty() )
    {
      // Select the I2C-switch channel using 'i2cset'
      // ('-y': non-interactive mode, followed by I2C-bus number,
      //  switch-address and switch channel select command;
      //  see PCA9540B datasheet)
      out() << "=> Set riser card I2C-switch (address=0x" << hex
           << i2caddr_sw << ") to " << chan << endl << dec;
      chan += 4; // Enable bit !
      oss.clear(); oss.str( "" ); // Reset ostringstream
      oss << setstr << i2cbus << " " << i2caddr_sw << " " << chan;
      out() << oss.str() << endl;
      if( system( oss.str().c_str() ) != 0 ) return false;
    }
  else
    {
      out() << "(riser card switch NOT found)" << endl;
    }
  return true;
}

// ----------------------------------------------------------------------------

bool set_i2cswitch( int i2cbusnr, int i2caddr_sw, int val )
{
  // Select the I2C-switch channel using 'i2cset'
  // ('-y': non-interactive mode, followed by I2C-bus number,
  //  switch-address and switch channel number to set)
  int i2cbus = i2cbusnr;
  ostringstream oss;
  oss << setstr << i2cbus << " " << i2caddr_sw << " " << val;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 )
    return false;
  return true;
}

// ----------------------------------------------------------------------------

bool set_i2cswitch_ipmi( int i2caddr_sw, int val )
{
  // Select the I2C-switch channel
  ostringstream oss;
  oss << setstr << i2caddr_sw << " 0 " << val;
  out() << oss.str() << endl;
  if( system( oss.str().c_str() ) != 0 )
    return false;
  return true;
}

// ----------------------------------------------------------------------------

void usage()
{
  out() << "fflash version " << hex << VERSION_ID << dec << endl <<
    "Tool for loading a firmware image from one of the partitions\n"
    "of the onboard flash memory of an FLX-712 into the card's FPGA,\n"
    "issueing commands to the host system I2C bus to achieve this.\n"
    "A subsequent hotplug procedure or machine reboot is required.\n\n"
    "Usage: fflash [-h|V] [-q] -f <flashnr>\n"
    "              [[-L|I] [-U|P -d <devslot>] [-S] [-b <busnr>] [-r <chan>]\n"
    "              [-s <saddr>] [-u <uaddr>] [-T <sec>]]\n"
    "  -h          : Show this help text.\n"
    "  -V          : Show version.\n"
    "  -q          : Be quiet (only errors will be displayed).\n"
    "  -f <flashnr>: Flash memory segment partition [0..3] selection "
    "(no default).\n"
    "  -I          : Generate an INIT_B pulse on the FLX-card "
    "(to reset flash devices).\n"
    "  -L          : Load firmware from the given flash partition "
    "into the card.\n\n"
    "The following options are relevant in conjunction with "
    "the -L and/or -I option:\n"
    "  -b <busnr>  : I2C-bus number (default=0).\n"
    "  -r <chan>   : Riser card I2C-switch channel number (default=0).\n"
    "                NB: I2C-switch has hard-coded I2C address 0x70.\n"
    "  -s <addr>   : I2C-switch I2C-address (hex, default=0x77, "
    "expected range: 0x70-0x77).\n"
    "                NB: 0x70 already taken by the riser card I2C-switch!\n"
    "  -u <addr>   : uC I2C-address (hex, default=0x67, "
    "expected range: 0x60-0x67).\n"
    "  -U          : Use USB I2C-dongle instead of system SMBus\n"
    "                (requires scripts i2cset.py and i2cget.py installed in "
    "/opt/flx).\n"
    "  -P          : Use 'ipmitool' to access system SMBus.\n"
    "                !NB: use -d option to select 'device slot': 1 or 2.\n"
    "  -T <sec>    : Set 'Prog-done' timeout [s] (default: 7)\n"
    "  -d <devslot>: Device slot (1 or 2), only in combination with -P.\n"
    "  -S          : Preceed calls to i2cget/set or ipmitool "
    "with 'sudo'.\n"
    "                (default: 'sudo' not used; applies to options -L|I|P|U)."
    "\n";
}

// ----------------------------------------------------------------------------

void usage_extended()
{
  out() <<
    "Examples:\n"
    "---------\n"
    "Load flash memory image partition #2 into the card:\n"
    "  fflash -f 2 -L\n\n"
    "Load flash memory image partition #2 into the card, using I2C-bus #1,\n"
    "riser card I2C-switch channel #0, FLX-card I2C-switch address 0x75 and\n"
    "FLX-card microcontroller I2C-address 0x65:\n"
    "  fflash -f 2 -L -b 1 -r 0 -s 75 -u 65\n\n"
    "How to determine the I2C-switch and uC I2C addresses\n"
    "(options -s and -u respectively) :\n"
    "Note 1: there is an I2C-bus number (option -b) to select as well,\n"
    "  which is assumed to have the value '1' (following '-y') "
    "in the examples below.\n"
    "Note 2: in the standard FELIX server there is an additional I2C-switch\n"
    "  on the socalled riser card; its channel is selected using option -r;\n"
    "  it means that the 2 FLX-cards in such a server may have identical\n"
    "  '-s' and '-u' addresses, i.e. most likely their defaults\n"
    "  while the riser card setting is: 'top' position = -r 0, "
    "'bottom' = -r 1.\n"
    "'sudo i2cdetect -y 1' should show you an address in the range "
    "0x70-0x77,\n"
    "let's say 0x77; this is then the address to use in option -s;\n"
    "subsequently run 'sudo i2cset -y 1 0x77 1' to set the I2C-switch\n"
    "causing an additional address in the range 0x60-0x67 to appear\n"
    "in the output of 'sudo i2cdetect -y 1', so run that command again;\n"
    "this is the address to use in option -u.\n"
    "On the FLX-712 dipswitch J14 configures the '-s' and '-u' addresses:\n"
    "  switch 1-3 to set 3 LSBs of '-s', i.e. 0x70-0x77\n"
    "  switch 4-6 to set 3 LSBs of '-u', i.e. 0x60-0x67\n";
}

// ----------------------------------------------------------------------------

std::string my_system( const string &command )
{
  // Use a pipe stream to retrieve and return the output
  // of the shell command line in parameter 'command'
  string output;
  FILE * stream;
  const int BUFSIZE = 256;
  char buffer[BUFSIZE];
  
  //command.append( " 2>&1" ); // Redirect stderr to stdout
  stream = popen( command.c_str(), "r" );
  if( stream )
    {
      while( !feof( stream ) )
        if( fgets( buffer, BUFSIZE, stream ) != NULL )
          output.append( buffer );
      pclose( stream );
    }
  else
    {
      output = "popen() error";
    }
  return output;
}

// ----------------------------------------------------------------------------

#include <fstream>
//std::string my_system( const char *command )
std::string my_system_old( const string &command )
{
  // Runs a 'system()' call retrieving the output from the called command
  // by directing output into a temporary file and reading the file contents
  //char tmpname[L_tmpnam]; // Use of tmpnam deemed dangerous
  //std::tmpnam( tmpname ); // Use of tmpnam deemed dangerous
  //std::string scommand = command;
  std::string tmpname = "/tmp/fflash-tmp.txt";
  std::string cmd = command + " > " + tmpname;
  std::system( cmd.c_str() );
  std::ifstream file( tmpname, std::ios::in );
  std::string result;
  if( file.is_open() )
    {
      //while( !file.eof() )
      //  result.push_back( file.get() );
      char c;
      while( file.get(c) )
        result.push_back( c );
      file.close();
    }
  //remove( tmpname );
  return result;
}

// ----------------------------------------------------------------------------
