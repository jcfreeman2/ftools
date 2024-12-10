/* ----------------------------------------------------------------------------
File   : fflashprog.cpp

Descr  : Tool for programming, verifying, erasing or dumping firmware images,
         stored in a FLX-711/712 card's flash memory.
         (to load a selected firmware image into the FLX-card's FPGA use fflash)
         For documentation see:
         https://gitlab.cern.ch/atlas-tdaq-felix/felix-fpga-flash/blob/
         master/docs/Operation_Manual_for_Flash_on_BNL711.pdf

Usage  : See usage() below.

History:
29APR2017; HenkB; Start development, based on the 'spidrflash' tool.
19MAY2017;   "  ; First working version (without flash partition selection).
21JUL2017;   "  ; Flash memory partition selection;
                  fast programming (page-by-page: new in Kai's firmware).
25OCT2017;   "  ; Options to load a flash memory partition (-L)
                  (and to set I2C-addresses for I2C-switch (-s) and uC (-u)).
05DEC2017;   "  ; Changes at the request of Kai:
                  - move flash partition selection to *before* clearing status
                    and flash ID reading.
                  - After firmware load, issue an 'INIT_B' to get flash chips
                    in proper state.
06DEC2017;   "  ; Added load_firmware() and pulse_initb(); added option -I/i
                  to reset flash devices independently (Kai request).
16JUN2018;   "  ; Added option -i to read flash ID, then exit;
                  added flash functions for FLX-712 with MT28FW02GB device
                  (the xxx712() functions).
29JUN2018;   "  ; Modifications for FLX-712 with MT28FW02GB,
                  prepared by Nico Giangiacomi.
22AUG2018;   "  ; Added function set_i2cswitch() and use it in load_firmware()
                  and pulse_initb() to set and unset the I2C-switch for the uC.
24AUG2018;   "  ; Added option -b to configure I2C-bus number to use,
                  as used by load_firmware() and pulse_initb().
23MAY2019;   "  ; Made strings 'setstr' and 'getstr' for i2cset and i2cget calls
                  global variables in preparation for making them configurable:
                  optionally we may choose an alternative path (USB I2C adapter)
                  for the motherboard SMB-bus to operate the FLX-712 I2C-bus.
09JUL2019;   "  ; Made strings 'setstr' and 'getstr' for 'ipmitool raw' calls,
                  a new path for accessing the system SMBus (I2C),
                  add an additional option to select it.
13NOV2019;   "  ; Update '-h' text; '-s' default changed from 0x70 to 0x77;
                  '-u' default changed from 0x68 to 0x67.
25NOV2019;   "  ; Add option -S: use 'i2cget/set' and 'ipmitool' *without* sudo.
02DEC2019;   "  ; On the PCIe riser card there is an additional 2-channel
                  I2C-switch that has to configured, i.e. chan 0 or 1 has
                  to be selected; option -r added for this.
04DEC2019;   "  ; Reverse logic of option -S: use tools *with* sudo
                  (default: without sudo).
17JAN2020;   "  ; Fix and extend comments and help text;
                  my_system() now uses pipe stream instead of intermediate file
                  for command output.
10FEB2020;   "  ; Added option -c for 'card number' (devnr = cardnr*2);
                  removed option -i (because default if no other option given);
                  some function name adjustments.
13FEB2020;   "  ; Take the result of system() calls into account,
                  and abort the fflash command sequence when an error occurs.
14FEB2020;   "  ; Add option -q, to be quiet/silent in output (errors only).
09APR2020;   "  ; Rename to fflashprog.cpp, removing all firmware loading
                  functionality, which remains/is moved to fflash.cpp.
---------------------------------------------------------------------------- */

#include <iostream>
#include <iomanip>
using namespace std;
#include <cstring>  // For memcmp()
#include <unistd.h> // For getopt()

#include "version.h"
#include "flxdefs.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "McsFile.h"
#include "arg.h"

// Functions for PC28F00BP30 flash memory chip
uint64_t flash_id( volatile flxcard_bar2_regs_t *bar2 );
bool flash_ready( volatile flxcard_bar2_regs_t *bar2,
                  int timeout_ms, bool fastmode = false );
bool read_flash ( volatile flxcard_bar2_regs_t *bar2,
                  int byte_addr, int nbytes,
                  unsigned char *flashbytes );
bool write_flash( volatile flxcard_bar2_regs_t *bar2,
                  int byte_addr, int nbytes,
                  unsigned char *flashbytes );
bool write_flash_fast( volatile flxcard_bar2_regs_t *bar2,
                  int byte_addr, int nbytes,
                  unsigned char *flashbytes );
bool erase_flash( volatile flxcard_bar2_regs_t *bar2,
                  int byte_addr );
void clear_flash_status( volatile flxcard_bar2_regs_t *bar2 );

// Functions for MT28FW02GB flash memory chip
uint64_t flash_id_712( volatile flxcard_bar2_regs_t *bar2 );
bool flash_ready_712( volatile flxcard_bar2_regs_t *bar2,
                      int timeout_ms );
bool read_flash_712 ( volatile flxcard_bar2_regs_t *bar2,
                      int byte_addr, int nbytes,
                     unsigned char *flashbytes );
bool write_flash_712( volatile flxcard_bar2_regs_t *bar2,
                      int byte_addr, int nbytes,
                      unsigned char *flashbytes );
bool write_flash_fast_712( volatile flxcard_bar2_regs_t *bar2,
                           int byte_addr, int nbytes,
                           unsigned char *flashbytes );
bool erase_flash_712( volatile flxcard_bar2_regs_t *bar2,
                      int byte_addr );
void clear_flash_status_712 ( volatile flxcard_bar2_regs_t *bar2 );
uint32_t flash_status_712   ( volatile flxcard_bar2_regs_t *bar2 );
bool unlock_bypass_712      ( volatile flxcard_bar2_regs_t *bar2 );
bool unlock_bypass_reset_712( volatile flxcard_bar2_regs_t *bar2 );

void display_percentage( int *percentage, int bytes_done, int bytes_todo );

void usage_extended();

// Quiet mode (if true only errors will be displayed):
bool be_quiet = false;
std::ostringstream devnull;
inline std::ostream &out()
{
  return( be_quiet ? devnull : cout ); 
}

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x20040900;

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
//  2   Riser location. Depends on which SXB slot you use to insert Riser,
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
  int  devnr      = -1;
  int  cardnr     = -1;
  int  flashnr    = -1;
  bool program_it = false;
  bool erase_it   = false;
  bool dump_it    = false;
  bool fastmode   = true;
  string filename;

  // Parse the options
  while( (opt = getopt(argc, argv, "c:d:DEf:FhqV")) != -1 )
    {
      switch( opt )
        {
        case 'c':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'c' );
          break;
        case 'd':
          if( sscanf( optarg, "%d", &devnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          dump_it = true;
          break;
        case 'E':
          erase_it = true;
          break;
        case 'f':
          // Flash partition number
          if( sscanf( optarg, "%d", &flashnr ) != 1 )
            arg_error( 'f' );
          if( flashnr < 0 || flashnr > 3 )
            arg_range( 'f', 0, 3 );
          break;
        case 'F':
          fastmode = false;
          break;
        case 'h':
          usage();
          usage_extended();
          return 0;
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

  // Option -d replaced by -c, but still accepted
  if( cardnr != -1 && devnr != -1 )
    {
      // Both options given
      cerr << "### Use -c or -d option to select FLX-card "
           << "(NB: -d is deprecated)" << endl;
      return 1;
    }
  else if( cardnr == -1 && devnr == -1 )
    {
      // None given: default 0
      cardnr = 0;
    }
  // Convert card number to device number
  if( devnr == -1 )
    devnr = 2 * cardnr;

  // --------------------------------
  // Name of MCS firmware file
  if( optind < argc )
    filename = string( argv[optind] );

  // Operation selection
  ++optind;
  if( optind < argc )
    {
      string s( argv[optind] );
      if( s == string("prog" ) )
        program_it = true;
    }

  if( erase_it && (!filename.empty() || dump_it) )
    {
      cerr << "### Invalid combination of options -E and -D "
           << "and/or file name" << endl;
      return 1;
    }

  McsFile mcs;
  if( !filename.empty() )
    {
      // Read the MCS/HEX file
      try {
        mcs.setFile( filename );
      }
      catch( McsException &exc ) {
        string msg;
          msg += filename;
          msg += ": ";
          msg += exc.toString();
          cerr << msg << endl;
          return 1;
        }
      out() << "MCS-file " << filename << ":" << endl
            << "min addr " << mcs.minAddr()
            << ", max addr: " << mcs.maxAddr() << endl;

      if( dump_it )
        {
          // DEBUG: display MCS file contents
          cout << endl << "=> MCS file contents" << endl;
          cout << hex << uppercase << setfill('0');
          unsigned char *page;
          int pagesz = mcs.pageSize();
          for( int pageno=0; pageno<(mcs.maxAddr()+pagesz-1)/pagesz; ++pageno )
            {
              cout << endl << dec << "PAGE " << pageno << hex;
              page = mcs.page( pageno );
              for( int i=0; i<pagesz; ++i )
                {
                  if( (i & 0x0F) == 0 )
                    cout << endl << setw(6) << pageno*pagesz+i << ' ';
                  cout << ' ' << setw(2) << (unsigned int) page[i];
                }
            }
          return 0;
        }
    }

  out() << "=> FLX-card " << devnr/2 << ":" << endl;

  if( flashnr < 0 && (dump_it || erase_it || program_it || !filename.empty()) )
    {
      // Must provide a flash partition number...
      cerr << "### Provide a flash partition number (option -f)" << endl;
      return 1;
    }

  // --------------------------------
  // Open FELIX FLX-card
  FlxCard *flx = new FlxCard;
  try {
    flx->card_open( devnr, LOCK_FLASH );
  }
  catch( FlxException &ex ) {
    cerr << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }
  uint64_t bar2_addr = flx->openBackDoor( 2 );
  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

  // --------------------------------

  // Switch FLASH from JTAG to firmware control
  bar2->CONFIG_FLASH_WR.FLASH_SEL = 1;

  // Select FLASH partition 'flashnr'
  bar2->CONFIG_FLASH_WR.PAR_CTRL = 1; // Firmware control instead of uC
  bar2->CONFIG_FLASH_WR.PAR_WR   = 3 - flashnr; // Inverted bits

  // Clear status
  //clear_flash_status( bar2 );
  //clear_flash_status_712( bar2 );

  // Get FLASH device ID
  uint64_t id = flash_id( bar2 );
  if( id != 0x899A )
    id = flash_id_712( bar2 );
  out() << hex << "FLASH ID = 0x"<< id << dec << endl;
  if( id != 0x899A && id != 0x22012248227E0089 )
    {
      cerr << "### Expected IDs: 0x899A or 0x22012248227E0089" << endl;
      bar2->CONFIG_FLASH_WR.FLASH_SEL = 0;
      return 1;
    }

  // Is it an FLX-712 with MT28FW02GB flash device?
  bool is712 = (id == 0x22012248227E0089);

  // In case the uC is used to select a partition:
  // Select the I2C-switch channel connected to the uC:
  //   (sudo) /usr/sbin/i2cset -y 0 0x70 0x01
  // (Compare result of "i2cdetect -y 0" *before* and *after*
  //  to see uC I2C-address appear).
  // ...More....

  // --------------------------------
  // Execute the requested operation on the selected flash memory partition

  unsigned char *flashmem = mcs.mem();
  int  byte_addr    = mcs.minAddr();
  int  bytes_todo   = mcs.maxAddr() - mcs.minAddr();
  int  bytes_done   = 0;
  int  bytes_page   = mcs.pageSize();
  int  bytes_sector = mcs.sectorSize();
  int  percentage   = -1;
  int  result       = 0; // OKAY

  if( dump_it )
    {
      // Just read and display the contents of the flash
      unsigned char flashbytes[2048];
      cout << "=> FLASH memory " << flashnr << " contents...\n";
      byte_addr  = 0;
      bytes_todo = mcs.memorySize();
      cout << hex << uppercase << setfill('0');
      while( bytes_done < bytes_todo )
        {
          if( is712 )
            {
              if( !read_flash_712( bar2, byte_addr, bytes_page, flashbytes ) )
                break;
            }
          else
            {
              if( !read_flash( bar2, byte_addr, bytes_page, flashbytes ) )
                break;
            }

          int i;
          for( i=0; i<bytes_page; ++i, ++byte_addr )
            {
              if( (byte_addr & 0x0F) == 0 )
                cout << endl << setw(6) << byte_addr << ' ';
              cout << ' ' << setw(2) << (unsigned int) flashbytes[i];
            }
          bytes_done += bytes_page;

          cout << endl;
          //char ch;
          //cin >> ch;
          //if( ch == 'q' || ch == 'Q' )
          //  return 0;
        }
    }
  else if( erase_it )
    {
      bytes_todo = mcs.memorySize();
      byte_addr  = 0;

      out() << "Erasing...     ";
      while( bytes_done < bytes_todo )
        {
          // Erase a sector when necessary (crossing a sector boundary)
          if( (byte_addr & (bytes_sector-1)) == 0 )
            {
              //out() << "ERASE " << hex << byte_addr << endl;
              if( is712 )
                {
                  if( !erase_flash_712( bar2, byte_addr ) )
                    break;
                }
              else
                {
                  if( !erase_flash( bar2, byte_addr ) )
                    break;
                }

              // Display percentage done
              display_percentage( &percentage, bytes_done, bytes_todo );
            }
          byte_addr  += bytes_page;
          bytes_done += bytes_page;
        }
      display_percentage( &percentage, bytes_done, bytes_todo ); // 100%...
      if( bytes_done < bytes_todo )
        {
          out() << endl << "interrupted" << endl;
          result = 1;
        }
      else
        {
          out() << endl << "done" << endl;
        }
    }
  else if( program_it )
    {
      // If necessary, adjust the start address downwards to the start
      // of the flash memory (sub)sector in which it is located
      // (to make sure this (sub)sector gets erased)
      if( (byte_addr & (bytes_sector-1)) != 0 )
        {
          bytes_todo += (byte_addr & (bytes_sector-1));
          byte_addr  &= ~(bytes_sector-1);
        }
      flashmem += byte_addr;

      //bytes_todo = 10*bytes_sector+bytes_page; // ###DEBUG

      out() << "Programming...     ";
      if( is712 )
        unlock_bypass_712( bar2 );
      while( bytes_done < bytes_todo )
        {
          // Erase a sector when necessary (crossing a sector boundary)
          if( (byte_addr & (bytes_sector-1)) == 0 )
            {
              //out() << "ERASE " << hex << byte_addr << endl;
              if( is712 )
                {
                  if( !erase_flash_712( bar2, byte_addr ) )
                    break;
                }
              else
                {
                  if( !erase_flash( bar2, byte_addr ) )
                    break;
                }
              // ###DEBUG:
              /*char ch;
              cin >> ch;
              if( ch == 'q' || ch == 'Q' )
              return 0;*/
            }

          if( is712 )
            {
              if( fastmode )
                {
                  if( !write_flash_fast_712( bar2, byte_addr,
                                             bytes_page, flashmem ) )
                    break;
                }
              else
                {
                  if( !write_flash_712( bar2, byte_addr,
                                        bytes_page, flashmem ) )
                    break;
                }
            }
          else
            {
              if( fastmode )
                {
                  if( !write_flash_fast( bar2, byte_addr,
                                         bytes_page, flashmem ) )
                    break;
                }
              else
                {
                  if( !write_flash( bar2, byte_addr,
                                    bytes_page, flashmem ) )
                    break;
                }
            }

          /* DEBUG
          cout << hex << uppercase << setfill('0');
          int i;
          for( i=0; i<128; ++i )
            {
              if( ((byte_addr+i) & 0x0F) == 0 )
                cout << endl << setw(6) << byte_addr+i << ' ';
              cout << ' ' << setw(2) << (unsigned int) flashmem[i];
            }
          cout << endl << dec;
          */
          flashmem   += bytes_page;
          byte_addr  += bytes_page;
          bytes_done += bytes_page;

          // Display percentage done
          display_percentage( &percentage, bytes_done, bytes_todo );
        }
      if( is712 )
        unlock_bypass_reset_712( bar2 );

      if( bytes_done < bytes_todo )
        {
          out() << endl << "interrupted" << endl;
          result = 1;
        }
      else
        {
          out() << endl << "done" << endl;
        }
    }
  else if( !filename.empty() )
    {
      unsigned char flashbytes[2048];
      int errcnt = 0;
      out() << "Verifying...     ";
      flashmem += byte_addr;
      while( bytes_done < bytes_todo && errcnt < 10 )
        {
          if( is712 )
            {
              if( !read_flash_712( bar2, byte_addr, bytes_page, flashbytes ) )
                break;
            }
          else
            {
              if( !read_flash( bar2, byte_addr, bytes_page, flashbytes ) )
                break;
            }

          // Check data retrieved against file contents
          // and stop as soon as a difference is found
          if( memcmp( flashmem, flashbytes, bytes_page ) != 0 )
            {
              cerr << hex << uppercase << endl;
              cerr << "### Mismatch in page at address "
                   << byte_addr << endl;

              // Display a number of differences in more detail
              for( int i=0; i<bytes_page; ++i )
                {
                  if( flashbytes[i] != flashmem[i] )
                    {
                      cerr << "addr " << setw(6) << (byte_addr+i)
                           << ": " << setw(2)
                           << (unsigned int) flashbytes[i]
                           << " vs " << setw(2)
                           << (unsigned int) flashmem[i] << endl;
                      ++errcnt;
                      if( errcnt == 10 ) break; // for-loop
                    }
                }
              //break; // while-loop
            }

          flashmem   += bytes_page;
          byte_addr  += bytes_page;
          bytes_done += bytes_page;

          // Display percentage done
          display_percentage( &percentage, bytes_done, bytes_todo );
        }
      if( bytes_done >= bytes_todo )
        out() << endl << "Verified OKAY!" << endl;
      else
        result = 1;
    }

  // Switch FLASH back to JTAG control
  bar2->CONFIG_FLASH_WR.FLASH_SEL = 0;

  return result;
}

// ----------------------------------------------------------------------------

uint64_t flash_id( volatile flxcard_bar2_regs_t *bar2 )
{
  // Read P30 flash ID
  uint64_t id;
  bar2->CONFIG_FLASH_WR.ADDRESS         = 1;
  bar2->CONFIG_FLASH_WR.DO_READDEVICEID = 1;
  bar2->CONFIG_FLASH_WR.DO_READDEVICEID = 0;
  id = bar2->CONFIG_FLASH_RD.READ_DATA;
  return id;
}

// ----------------------------------------------------------------------------

uint64_t flash_id_712( volatile flxcard_bar2_regs_t *bar2 )
{
  // Read M28FW02GB flash ID
  uint64_t manufacturer_code, device_code_1, device_code_2, device_code_3;

  // Set READ RESET command:
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xAA;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0; 
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x55;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x2AA;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0; 
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xF0;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;   

  // Enter AUTO SELECT mode:
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xAA;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x55;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x2AA;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x90;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;

  // Read IDs:
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x0;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 1;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 0;
  usleep( 1 );
  manufacturer_code = bar2->CONFIG_FLASH_RD.READ_DATA; // Expect 0x0089
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x1;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 1;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 0;
  usleep( 1 );
  device_code_1 = bar2->CONFIG_FLASH_RD.READ_DATA;     // Expect 0x227e
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0xE;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 1;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 0;
  usleep( 1 );
  device_code_2 = bar2->CONFIG_FLASH_RD.READ_DATA;     // Expect 0x2248
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0xF;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 1;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 0;
  usleep( 1 );
  device_code_3 = bar2->CONFIG_FLASH_RD.READ_DATA;     // Expect 0x2201

  // Exit AUTO SELECT mode:
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xF0;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;

  uint64_t id;
  id = manufacturer_code | (device_code_1 << 16) |
       (device_code_2 << 32) | (device_code_3 << 48);
  return id;
}

// ----------------------------------------------------------------------------

bool flash_ready( volatile flxcard_bar2_regs_t *bar2, int timeout_ms,
                  bool fastmode )
{
  int timeout_unit = 100; // Polling period in microseconds
  int timeout_cnt  = (1000*timeout_ms + timeout_unit-1) / timeout_unit;
  // Check the 'device write status' in the status register
  if( fastmode )
    {
      bar2->CONFIG_FLASH_WR.FAST_READ = 1;
      bar2->CONFIG_FLASH_WR.FAST_READ = 0;
    }
  else
    {
      bar2->CONFIG_FLASH_WR.DO_READSTATUS = 1;
      bar2->CONFIG_FLASH_WR.DO_READSTATUS = 0;
    }
  uint64_t stat = bar2->CONFIG_FLASH_RD.READ_DATA;
  while( (stat & 0x80) == 0 && timeout_cnt > 0 )
    {
      usleep( timeout_unit );
      --timeout_cnt;
      if( fastmode )
        {
          bar2->CONFIG_FLASH_WR.FAST_READ = 1;
          bar2->CONFIG_FLASH_WR.FAST_READ = 0;
        }
      else
        {
          bar2->CONFIG_FLASH_WR.DO_READSTATUS = 1;
          bar2->CONFIG_FLASH_WR.DO_READSTATUS = 0;
        }
      stat = bar2->CONFIG_FLASH_RD.READ_DATA;
    }
  return( (stat & 0x80) == 0x80 );
  // FLASH_BUSY does not seem to work, changed to procedure above
  // checking the status register (19 May 2017)
  /*
  while( bar2->CONFIG_FLASH_RD.FLASH_BUSY == 1 && timeout_cnt > 0 )
    {
      usleep( timeout_unit );
      --timeout_cnt;
    }
  return( bar2->CONFIG_FLASH_RD.FLASH_BUSY == 0 );
  */
}

// ----------------------------------------------------------------------------

bool flash_ready_712( volatile flxcard_bar2_regs_t *bar2, int timeout_ms )
{
  int timeout_unit = 100; // Polling period in microseconds
  int timeout_cnt  = (1000*timeout_ms + timeout_unit-1) / timeout_unit;
  uint32_t stat = flash_status_712( bar2 );
  while( (stat & 0x80) == 0 && timeout_cnt > 0 )
    {
      usleep( timeout_unit );
      --timeout_cnt;
      stat = flash_status_712( bar2 );
    }
  return( (stat & 0x80) == 0x80 );
}

// ----------------------------------------------------------------------------

bool read_flash( volatile flxcard_bar2_regs_t *bar2,
                 int byte_addr, int nbytes,
                 unsigned char *flashbytes )
{
  unsigned char *f = flashbytes;
  uint64_t val, address = byte_addr/2;
  for( int i=0; i<nbytes/2; ++i, ++address )
    {
      bar2->CONFIG_FLASH_WR.ADDRESS = address;
      bar2->CONFIG_FLASH_WR.DO_READ = 1;
      bar2->CONFIG_FLASH_WR.DO_READ = 0;
      val = bar2->CONFIG_FLASH_RD.READ_DATA;
      *f = (unsigned char) ((val & 0xFF) >> 0);
      ++f;
      *f = (unsigned char) ((val & 0xFF00) >> 8);
      ++f;
    }
  return true;
}

// ----------------------------------------------------------------------------

bool read_flash_712( volatile flxcard_bar2_regs_t *bar2,
                     int byte_addr, int nbytes,
                     unsigned char *flashbytes )
{
  // Enter read CFI mode
  /*
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x98;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  */
  // Read data
  unsigned char *f = flashbytes;
  uint64_t val, address = byte_addr/2;
  for( int i=0; i<nbytes/2; ++i, ++address )
    {
      bar2->CONFIG_FLASH_WR.ADDRESS   = address;
      bar2->CONFIG_FLASH_WR.FAST_READ = 1;
      bar2->CONFIG_FLASH_WR.FAST_READ = 0;
      val = bar2->CONFIG_FLASH_RD.READ_DATA;
      *f = (unsigned char) ((val & 0xFF) >> 0);
      ++f;
      *f = (unsigned char) ((val & 0xFF00) >> 8);
      ++f;
    }

  // Exit read CFI mode
  /*
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xF0;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  */
  return true;
}

// ----------------------------------------------------------------------------

bool write_flash( volatile flxcard_bar2_regs_t *bar2,
                  int byte_addr, int nbytes,
                  unsigned char *flashbytes )
{
  //out() << "##" << byte_addr << " " << nbytes << endl;
  unsigned char *f = flashbytes;
  uint64_t val, address = byte_addr/2;
  for( int i=0; i<nbytes/2; ++i, ++address )
    {
      val = (uint64_t) f[2*i] | (((uint64_t) f[2*i+1]) << 8);
      bar2->CONFIG_FLASH_WR.WRITE_DATA = val;
      bar2->CONFIG_FLASH_WR.ADDRESS    = address;
      bar2->CONFIG_FLASH_WR.DO_WRITE   = 1;
      bar2->CONFIG_FLASH_WR.DO_WRITE   = 0;
      // Wait for PROGRAM command to finish or time out
      if( !flash_ready( bar2, 1 ) )
        {
          cerr << "### Timeout, address=0x" << hex << address << endl;
          return false;
        }
    }
  return true;
}

// ----------------------------------------------------------------------------

bool write_flash_712( volatile flxcard_bar2_regs_t *bar2,
                     int byte_addr, int nbytes,
                     unsigned char *flashbytes )
{
  unsigned char *f = flashbytes;
  uint64_t val, address = byte_addr/2;
  for( int i=0; i<nbytes/2; ++i, ++address )
    {
      val = (uint64_t) f[2*i] | (((uint64_t) f[2*i+1]) << 8);
      bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xA0;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
      bar2->CONFIG_FLASH_WR.WRITE_DATA = val;
      bar2->CONFIG_FLASH_WR.ADDRESS    = address;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
      // Wait for PROGRAM command to finish or time out
      if( !flash_ready_712( bar2, 1 ) )
        {
          cerr << "### Timeout, address=0x" << hex << address
               << ", status=0x" << flash_status_712( bar2 ) << endl;
          return false;
        }
      else if( (flash_status_712(bar2) & 0x18) != 0 )
        {
          cerr << "### Prog error, address=0x" << hex << address
               << ", status=0x" << flash_status_712( bar2 ) << endl;
          return false;
        }
    }
  return true;
}

// ----------------------------------------------------------------------------

bool write_flash_fast( volatile flxcard_bar2_regs_t *bar2,
                       int byte_addr, int nbytes,
                       unsigned char *flashbytes )
{
  unsigned char *f = flashbytes;
  uint64_t val, address = byte_addr/2;

  // Clear status
  clear_flash_status( bar2 );
  //bar2->CONFIG_FLASH_WR.ADDRESS = byte_addr/2;
  //for( int i=0; i<513; ++i )
  //  {
  //    bar2->CONFIG_FLASH_WR.DO_CLEARSTATUS = 1;
  //    bar2->CONFIG_FLASH_WR.DO_CLEARSTATUS = 0;
  //  }

  // Initialize page write
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xE8;
  bar2->CONFIG_FLASH_WR.ADDRESS    = byte_addr/2;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  if( !flash_ready( bar2, 1, true ) )
    {
      cerr << "### Timeout fast-start, pageaddr=0x"
           << hex << byte_addr << endl;
      return false;
    }

  // Write page size (minus 1)
  bar2->CONFIG_FLASH_WR.WRITE_DATA = nbytes/2 - 1;
  bar2->CONFIG_FLASH_WR.ADDRESS    = byte_addr/2;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;

  // Write page data
  for( int i=0; i<nbytes/2; ++i, ++address )
    {
      val = (uint64_t) f[2*i] | (((uint64_t) f[2*i+1]) << 8);
      bar2->CONFIG_FLASH_WR.WRITE_DATA = val;
      bar2->CONFIG_FLASH_WR.ADDRESS    = address;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
    }

  // Finalize page write
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xD0;
  bar2->CONFIG_FLASH_WR.ADDRESS    = byte_addr/2;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  if( !flash_ready( bar2, 100, true ) )
    {
      cerr << "### Timeout fast-end, pageaddr=0x"
           << hex << byte_addr << endl;
      return false;
    }

  return true;
}

// ----------------------------------------------------------------------------

bool write_flash_fast_712( volatile flxcard_bar2_regs_t *bar2,
                           int byte_addr, int nbytes,
                           unsigned char *flashbytes )
{
  unsigned char *f = flashbytes;
  uint64_t val, address = byte_addr/2;

  // Clear status
  clear_flash_status_712( bar2 );

  // Initialize page write
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x25;
  bar2->CONFIG_FLASH_WR.ADDRESS    = byte_addr/2;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.WRITE_DATA = nbytes/2 - 1;
  bar2->CONFIG_FLASH_WR.ADDRESS    = byte_addr/2;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;

  // Write page data
  for( int i=0; i<nbytes/2; ++i, ++address )
    {
      val = (uint64_t) f[2*i] | (((uint64_t) f[2*i+1]) << 8);
      bar2->CONFIG_FLASH_WR.WRITE_DATA = val;
      bar2->CONFIG_FLASH_WR.ADDRESS    = address;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
    }

  // Finalize page write
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x29;
  bar2->CONFIG_FLASH_WR.ADDRESS    = byte_addr/2;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  if( !flash_ready_712( bar2, 100 ) )
    {
      cerr << "### Timeout, pageaddr=0x" << hex << byte_addr
           << ", status=0x" << flash_status_712( bar2 ) << endl;
      return false;
    }
  else if( (flash_status_712(bar2) & 0x18) != 0 )
    {
      cerr << "### Prog error, pageaddr=0x" << hex << byte_addr
           << ", status=0x" << flash_status_712( bar2 ) << endl;

      // Set program abort:
      bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xAA;
      bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 0; 
      bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x55;
      bar2->CONFIG_FLASH_WR.ADDRESS    = 0x2AA;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 0; 
      bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xF0;
      bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
      bar2->CONFIG_FLASH_WR.FAST_WRITE = 0; 
      if( !flash_ready_712( bar2, 100 ) )
        {
          cerr << "### Timeout program abort\n";
        }
      return false;
    }

  return true;
}

// ----------------------------------------------------------------------------

bool erase_flash( volatile flxcard_bar2_regs_t *bar2,
                  int byte_addr )
{
  bar2->CONFIG_FLASH_WR.ADDRESS         = byte_addr/2;
  bar2->CONFIG_FLASH_WR.DO_UNLOCK_BLOCK = 1;
  bar2->CONFIG_FLASH_WR.DO_UNLOCK_BLOCK = 0;
  bar2->CONFIG_FLASH_WR.DO_ERASEBLOCK   = 1;
  bar2->CONFIG_FLASH_WR.DO_ERASEBLOCK   = 0;
  // Wait for ERASE command to finish or time out
  if( !flash_ready( bar2, 2000 ) )
    {
      cerr << "### Timeout ERASE-BLOCK (addr=" << hex << byte_addr << ")\n";
      return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

bool erase_flash_712( volatile flxcard_bar2_regs_t *bar2,
                      int byte_addr )
{
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x80;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x30;
  bar2->CONFIG_FLASH_WR.ADDRESS    = byte_addr/2;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  // Wait for ERASE command to finish or time out
  if( !flash_ready_712( bar2, 2000 ) )
    {
      cerr << "### Timeout ERASE-BLOCK (addr=" << hex << byte_addr << ")\n";
      return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

void clear_flash_status( volatile flxcard_bar2_regs_t *bar2 )
{
  bar2->CONFIG_FLASH_WR.ADDRESS = 0;
  for( int i=0; i<513; ++i )
    {
      bar2->CONFIG_FLASH_WR.DO_CLEARSTATUS = 1;
      bar2->CONFIG_FLASH_WR.DO_CLEARSTATUS = 0;
    }
}

// ----------------------------------------------------------------------------

void clear_flash_status_712( volatile flxcard_bar2_regs_t *bar2 )
{
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x71;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
}

// ----------------------------------------------------------------------------

uint32_t flash_status_712( volatile flxcard_bar2_regs_t *bar2 )
{ 
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x70;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1; 
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 1;
  bar2->CONFIG_FLASH_WR.FAST_READ  = 0;
  uint64_t stat = bar2->CONFIG_FLASH_RD.READ_DATA;
  if( stat & 0xFFFFFF01 )
    {
      cerr << "###Unknown FLASH status = 0x" << hex << stat << dec << endl;
      return 0x0;
    }
  return( stat & 0xFF );
}

// ----------------------------------------------------------------------------

bool unlock_bypass_712( volatile flxcard_bar2_regs_t *bar2 )
{
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0xAA;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x55;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x2AA;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x20;
  bar2->CONFIG_FLASH_WR.ADDRESS    = 0x555;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  if( !flash_ready_712( bar2, 2000 ) )
    {
      cerr << "### Timeout unlock bypass command\n";
      return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

bool unlock_bypass_reset_712( volatile flxcard_bar2_regs_t *bar2 )
{
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x90;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  bar2->CONFIG_FLASH_WR.WRITE_DATA = 0x00;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 1;
  bar2->CONFIG_FLASH_WR.FAST_WRITE = 0;
  if( !flash_ready_712( bar2, 2000 ) )
    {
      cerr << "### Timeout unlock bypass reset command\n";
      return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

void display_percentage( int *percentage, int bytes_done, int bytes_todo )
{
  // Display percentage done
  int p = *percentage;
  double pf = ((double) 100.0*bytes_done)/(double) bytes_todo;
  if( (int) pf != p )
    {
      p = (int) pf;
      out() << "\b\b\b\b\b    \b\b\b\b" << setw(3) << p << "% ";
      out().flush();
      *percentage = p;
    }
}

// ----------------------------------------------------------------------------

void usage()
{
  out() << "fflashprog version " << hex << VERSION_ID << dec << endl <<
    "Tool for programming, verifying, erasing or dumping firmware images,\n"
    "stored in a FLX-711/712 card's flash memory.\n"
    "(to load a selected firmware image into the FLX-card's FPGA use fflash)\n"
    "Usage: fflashprog [-h|V] [-q] [-c <cardnr>] -f <flashnr> [-D] [-E] [-F]\n"
    "                  [<filename>] [prog]\n"
    "  -h          : Show this help text.\n"
    "  -V          : Show version.\n"
    "  -q          : Be quiet (only errors will be displayed).\n"
    "  -c <cardnr> : FLX-card selected (default: 0).\n"
    "  -d <devnr>  : FLX-device to use (default: 0) OBSOLETE: use -c.\n"
    "  -D          : Read and display contents of the selected flash partition"
    " or flash file.\n"
    "  -E          : Erase the selected flash partition.\n"
    "  -f <flashnr>: Flash memory segment partition [0..3] to dump, to erase,\n"
    "                to verify or to program (no default).\n"
    "  -F          : Use the (slow) word-by-word instead of (fast) page "
    "programming method.\n\n"
    "  <filename>  : Name of MCS file to dump, verify or program.\n"
    "  prog        : Literal text string to initiate flash programming\n"
    "                (or else flash verification will take place).\n";
}

// ----------------------------------------------------------------------------

void usage_extended()
{
  out() <<
    "Examples:\n"
    "---------\n"
    "Read and dump to screen flash memory image partition #2:\n"
    "  fflashprog -f 2 -D\n"
    "Erase flash memory partition #2:\n"
    "  fflashprog -f 2 -E\n"
    "Verify flash memory partition #2 against mcs file <filename>:\n"
    "  fflashprog -f 2 <filename>\n"
    "Program flash memory partition #2 with the contents of mcs file "
    "<filename>:\n"
    "  fflashprog -f 2 <filename> prog\n"
    "Read flash ID only:\n"
    "  fflashprog -f 0\n"
    "Extra:\n"
    "Read and dump to screen the memory image in mcs file <filename>:\n"
    "  fflashprog -D <filename>\n\n";
}

// ----------------------------------------------------------------------------
