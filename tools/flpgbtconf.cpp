#include <cstring>
#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "arg.h"
#include "lpgbt-items-v0.h"
#include "lpgbt-items-v1.h"
#include "ic.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x22010500; // Use functions in ic.cpp
//const int VERSION_ID = 0x21122800; // 'list' items/regs from real device
//const int VERSION_ID = 0x21122200; // Option -1 (lpGBTv1) and -e (EC-link)
//const int VERSION_ID = 0x21092100; // Option -i (select ToHost DMA controller)
//const int VERSION_ID = 0x21082400;

// Function declarations
const lpgbt_item_t *selectField  ( const char *fieldname,
                                   std::string &info_str,
                                   bool lpgbt_v1 );
void                listAllFields( bool lpgbt_v1,
                                   FlxUpload *fup = 0,
                                   FlxReceiver *frecvr = 0,
                                   int linknr = 0, int i2c_addr = 0,
                                   bool use_ec = false,
                                   bool debug = false );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int   opt;
  int   cardnr     = 0;
  int   dma_write  = -1; // Autoselect FromHost DMA controller index
  int   dma_read   = 0;
  int   linknr     = -1;
  int   i2c_addr   = -1;
  int   reg_addr   = -1;
  char *fieldname  = 0;
  bool  receive    = true;
  bool  use_intrpt = false;
  bool  write_it   = false;
  bool  use_ec     = false;
  bool  lpgbt_v1   = false;
  bool  debug      = false;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "1d:DeG:hi:I:VZ")) != -1 )
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
        case 'I':
          // lpGBT I2C address
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'I' );
          i2c_addr = x;
          if( i2c_addr < 0 || i2c_addr > 127 )
            arg_range( 'I', 0, 127 );
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

  if( optind < argc )
    {
      // Convert all chars to uppercase and change any '-' into '_'
      char *name = argv[optind];
      for( unsigned int i=0; i<strlen(name); ++i )
        {
          if( name[i] == '-' )
            name[i] = '_';
          else
            name[i] = toupper( name[i] );
        }

      // Check if a (decimal or hex) number is provided
      // which is to be taken as a register address,
      // else consider it to be a register or bit field name
      bool is_hex = true;
      bool is_dec = true;
      for( unsigned int i=0; i<strlen(name); ++i )
        {
          char ch = name[i];
          if( (i == 0 && ch != '0') ||
              (i == 1 && ch != 'x' && ch != 'X') ||
              (i >= 2 &&
               !((ch >= 'a' && ch <= 'f') ||
                 (ch >= 'A' && ch <= 'F') ||
                 (ch >= '0' && ch <= '9'))) )
            is_hex = false;
 
          if( ch < '0' || ch > '9' )
            is_dec = false;
        }

      if( is_dec )
        {
          // Not a register address after all?
          if( sscanf( name, "%d", &reg_addr ) != 1 )
            fieldname = name;
        }
      else if( is_hex )
        {
          // Not a register address after all?
          if( sscanf( name, "%x", &x ) != 1 )
            fieldname = name;
          else
            reg_addr = x;
        }
      else
        {
          fieldname = name;
        }
    }
  else
    {
      cout << "Provide a bitfield or register name to read or write" << endl;
      usage();
      return 0;
    }

  int max_addr = 463-1;
  std::string info_str;
  const lpgbt_item_t *item = 0;

  // SPECIAL: if name is "LIST", display the info of all known
  // registers and bitfields, e.g. for the user to 'grep' on
  if( fieldname && strcmp(fieldname, "LIST") == 0 )
    {
      if( linknr >= 0 || i2c_addr >= 0 )
        // Also display the values read from an lpGBT device
        goto list_all;
      listAllFields( lpgbt_v1 );
      return 0;
    }

  // Check for a valid name or address
  // lpGBTv0: 463 8-bit registers; first 320 R/W, rest read-only
  // lpGBTv1: 494 8-bit registers; first 326 R/W,  "    "
  if( lpgbt_v1 )
    max_addr = 494-1;
  if( fieldname )
    {
      item = selectField( fieldname, info_str, lpgbt_v1 );
    }
  else
    {
      if( reg_addr < 0 || reg_addr > max_addr )
        {
          cout << "### lpGBTv" << (lpgbt_v1 ? "1" : "0")
               << " address (hex) not in range [0..0x"
               << hex << uppercase << max_addr << "]" << dec << endl;
          return 1;
        }

      // Find the first item corresponding to the given address
      // (which should be the full register)
      const lpgbt_item_t *it;
      if( lpgbt_v1 )
        it = &LPGBTv1_ITEM[0];
      else
        it = &LPGBTv0_ITEM[0];
      while( strlen(it->name) != 0 )
        {
          if( it->addr == reg_addr )
            {
              // Found it
              item = it;
              break;
            }
          ++it;
        }
    }

  // Is the register or bitfield name valid?
  if( item == 0 && reg_addr < 0 )
    {
      if( info_str.empty() )
        {
          if( fieldname )
            cout << "### Bitfield \"" << fieldname
                 << "\" not found" << endl;
          else
            cout << "### Register 0x" << hex << setfill('0') << uppercase
                 << setw(4) << reg_addr << " not found" << endl;
        }
      else
        {
          // Info string is not empty, but item name is invalid,
          // and the string now contains the list of available options
          cout << "->Item not found, suggested options are:"
               << endl << info_str;
        }
      return 1;
    }

  if( item != 0 )
    {
      // Display some info about the item
      cout << item->name << ": addr=0x"
           << hex << uppercase << setfill('0') << setw(3) << item->addr
           << dec << " (" << item->addr << ")"
           << " size=" << item->nbits
           << "bits index=" << item->bitindex
           << (item->readonly ? " R" : " RW") << endl;
      if( strlen(item->desc) > 0 )
        cout << "\"" << item->desc << "\"" << endl;
      else
        cout << "<no description>" << endl;

      reg_addr = item->addr;
    }

  uint32_t val_to_write;
  if( optind < argc-1 )
    {
      // A value to be written is provided, apparently
      char *towrite = argv[optind+1];
      bool  ishex = false;
      if( strlen(towrite) > 2 &&
          towrite[0] == '0' && tolower(towrite[1]) == 'x' )
        ishex = true;

      int result;
      if( ishex )
        {
          // Parse as hexadecimal number
          result = sscanf( towrite, "%x", &val_to_write );
          if( result == 1 )
            arg_check_hex( &towrite[2], 0 );
        }
      else
        {
          // Parse as decimal number
          result = sscanf( towrite, "%u", &val_to_write );
          if( result == 1 )
            arg_check_dec( towrite, 0 );
        }

      if( result != 1 )
        {
          cout << "### Value to write is invalid" << endl;
          return 1;
        }

      //cout << "DEBUG: value = " << towrite << " = " << val_to_write
      //     << ", hex=" << ishex << endl;

      uint32_t max;
      if( item == 0 )
        max = 0xFF;
      else
        max = (1<<item->nbits) - 1;
      if( val_to_write > max )
        {
          cout << "### Value not in range, max=0x"
               << hex << max << dec << " (" << max << ")" << endl;
          return 1;
        }

      write_it = true;

      // Check whether item/register is writable
      if( (!lpgbt_v1 && reg_addr >= 320) ||
          (lpgbt_v1 && reg_addr >= 326) )
        {
          cout << "### Register or item is read-only" << endl;
          return 1;
        }
    }

 list_all:

  // Check for mandatory parameters
  if( linknr < 0 && i2c_addr < 0 )
    {
      // Only displayed info about the item
      return 0;
    }
  else if( linknr < 0 || i2c_addr < 0 )
    {
      if( linknr < 0 )
        cout << "### Provide an lpGBT link number (-G)" << endl;
      if( i2c_addr < 0 )
        cout << "### Provide an I2C address (-I)" << endl;
      return 1;
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
  if( !(fw == FIRMW_PIXEL || fw == FIRMW_STRIP || fw == FIRMW_LPGBT) )
    {
      // It's *not* a firmware using lpGBT
      cout << "### flpgbtconf tool is for RM5/lpGBT firmware only" << endl;
      return 1;
    }
#else
  cout << "### flpgbtconf tool is for RM5/lpGBT firmware only" << endl;
  return 1;
#endif // REGMAP_VERSION

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
      /*cout << "Opened FLX-device " << cardnr
           << ", firmw " << frecvr->firmwareVersion()
           <<  " (cmem buffersize="
           << frecvr->bufferSize() << ", receive: "
           << "DMA #" << dma_read;
      if( use_intrpt )
        cout << " interrupt-driven)" << endl;
      else
        cout << " polling)" << endl;
      */
      frecvr->setUseInterrupt( use_intrpt );
    }

  // SPECIAL: if name is "LIST", display the info of all known
  // registers and bitfields, e.g. for the user to 'grep' on,
  // in this case including the items' value read from the lpGBT device
  if( fieldname && strcmp(fieldname, "LIST") == 0 )
    {
      listAllFields( lpgbt_v1, &fup, frecvr, linknr, i2c_addr, use_ec, debug );
      return 0;
    }

  // -------------------------------------------------------------------------

  uint8_t reg_val;

  // First read the register containing the item
  cout << "=> Read:" << endl;
  if( readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 1, &reg_val,
                  use_ec, lpgbt_v1, debug ) )
    {
      if( item )
        {
          uint32_t val;
          val = (reg_val >> item->bitindex) & ((1<<item->nbits)-1);
          cout << item->name << ": 0x" << hex << val
               << " (" << dec << val << ")" << endl;
        }
    }
  else
    {
      if( !debug )
        {
          fup.stop();
          if( frecvr )
            frecvr->stop();
          return 1;
        }
    }

  if( !write_it )
    {
      fup.stop();
      if( frecvr )
        frecvr->stop();
      return 0;
    }

  // Going to write: update the register value
  if( item )
    {
      reg_val &= ~(((1<<item->nbits)-1) << item->bitindex);
      reg_val |= (val_to_write << item->bitindex);
    }
  else
    {
      reg_val = val_to_write;
    }

  // Write the updated register value
  cout << "=> Write:" << endl;
  writeIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 1, &reg_val,
               use_ec, lpgbt_v1, debug );

  // Re-read the register containing the item
  cout << "=> Reread:" << endl;
  if( readIcRegs( &fup, frecvr, linknr, i2c_addr, reg_addr, 1, &reg_val,
                  use_ec, lpgbt_v1, debug ) )
    {
      if( item )
        {
          uint32_t val;
          val = (reg_val >> item->bitindex) & ((1<<item->nbits)-1);
          cout << item->name << ": 0x" << hex << val
               << " (" << dec << val << ")" << endl;
        }
    }

  // -------------------------------------------------------------------------

  fup.stop();
  if( frecvr )
    frecvr->stop();

  return 0;
}

// ----------------------------------------------------------------------------

const lpgbt_item_t *selectField( const char *fieldname,
                                 std::string &info_str,
                                 bool lpgbt_v1 )
{
  // Searches for the lpGBT register bit field name in the list and
  // returns a pointer if found; the name should be exact or a unique substring;
  // returns NULL if not found and returns a string with possible options
  // in 'info_str' (if any)
  bool found = false;
  const lpgbt_item_t *item_list;
  if( lpgbt_v1 )
    item_list = LPGBTv1_ITEM;
  else
    item_list = LPGBTv0_ITEM;

  const lpgbt_item_t *item = &item_list[0];
  while( strlen(item->name) != 0 )
    {
      if( strcmp(item->name, fieldname) == 0 )
        {
          found = true;
          break;
        }
      ++item;
    }

  // Require at least 2 chars to compile a name options list
  size_t len = strlen( fieldname );
  if( !found && len >= 2 )
    {
      // Find out if the given name substring is unique
      // and compile a list of name options at the same time
      int cnt = 0;
      std::ostringstream oss;
      const lpgbt_item_t *first_item = 0;
      item = &item_list[0];
      while( strlen(item->name) != 0 )
        {
          if( strncmp(item->name, fieldname, len) == 0 )
            {
              first_item = item;
              ++cnt;
              oss << item->name << std::endl;
            }
          ++item;
        }

      if( cnt == 1 )
        {
          // Name is unique
          found = true;
          item = first_item;
        }
      else
        {
          // Add items containing the substring avoiding duplicates (pos != 0)
          if( true ) //if( cnt == 0 )
            {
              // Look for field names with the substring contained
              std::string f;
              item = &item_list[0];
              while( strlen(item->name) != 0 )
                {
                  f = std::string( item->name );
                  size_t pos = f.find( fieldname );
                  // Compile a list of names, avoiding items already found
                  if( pos != std::string::npos && pos != 0 )
                    oss << item->name << std::endl;
                  ++item;
                }
            }
          // Provide the caller with the list of names
          info_str = oss.str();
        }
    }

  if( found )
    // Return string with information about the bit field
    info_str = std::string( item->desc );
  else
    // Nothing found
    item = 0;

  return item;
}

// ----------------------------------------------------------------------------

void listAllFields( bool lpgbt_v1,
                    FlxUpload *fup,
                    FlxReceiver *frecvr,
                    int linknr, int i2c_addr,
                    bool use_ec, bool debug )
{
  cout << "lpGBTv" << (lpgbt_v1 ? "1" : "0") << " items:" << endl
       << "--------------" << endl;

  uint8_t regs[512];
  if( fup && frecvr )
    {
      // Read all lpGBT registers in one operation
      memset( regs, 0, 512 );
      uint32_t regcount = (lpgbt_v1 ? 494 : 463);
      if( !readIcRegs( fup, frecvr, linknr, i2c_addr, 0, regcount, regs,
                       use_ec, lpgbt_v1, debug, false ) )
        {
          cout << "### readIcRegs() failed" << endl;
          fup = 0; // Skip display of register values below
          return; // ...or return
        }
    }

  const lpgbt_item_t *item;
  if( lpgbt_v1 )
    item = &LPGBTv1_ITEM[0];
  else
    item = &LPGBTv0_ITEM[0];

  while( strlen(item->name) != 0 )
    {
      cout << setw(50) << left
           << item->name << right << setfill( '0' )
           << " 0x" << hex << uppercase
           << setw(3) << item->addr << dec << setfill( ' ' );
      if( item->readonly )
        cout << " R  ";
      else
        cout << " RW ";
      cout << setw(3) << item->nbits << "b";

      cout << " [";
      if( item->nbits > 1 )
        cout << item->bitindex + item->nbits - 1 << ":";
      cout << item->bitindex;
      cout << "]";

      if( fup && frecvr )
        {
          // Display the item's current value
          if( item->nbits == 1 )
            cout << "  ";
          cout << " :   ";

          uint32_t val, width;
          val = (regs[item->addr] >> item->bitindex) & ((1<<item->nbits)-1);
          width = (item->nbits + 3)/4;
          cout << "0x" << hex << setfill('0') << setw(width) << val
               << setfill(' ');
          if( item->nbits <= 4 )
            cout << " ";
          cout << "  (" << dec << val << ")";
        }
      cout << endl;

      ++item;
    }
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "flpgbtconf version " << hex << VERSION_ID << dec << endl <<
    "Read or write an lpGBT register or register bitfield, by address or name.\n"
    "The value of the item is read and displayed, and if requested,\n"
    "written to and re-read and again displayed.\n"
    "Requires the IC channel (or EC-channel) of the FLX device to be enabled.\n"
    "Without options -G and -I some information about the selected register\n"
    "or bitfield is displayed.\n"
    "'flpgbtconf list' displays all known 'item' names plus additional info\n"
    "about each item (for either lpGBTv0 or lpGBTv1).\n"
    "\n"
    "Usage: flpgbtconf [-h|V] [-d<devnr>] [-D<dma>] -G<lnk> -I<i2c> [-1] [-e] "
    "[-X] [-Z]\n"
    "                  <name> [<value>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device number (default: 0).\n"
    "  -D <dma>   : FLX-device DMA controller for receiving (default: 0).\n"
    "  -1         : Assume lpGBTv1 (default: lpGBTv0)\n"
    "  -e         : Use the EC-channel (default: IC-channel).\n"
    "  -G <lnk>   : lpGBT link number.\n"
    "  -I <i2c>   : lpGBT I2C address.\n"
    "  -X         : Debug mode: display bytes of received frames;\n"
    "               also: continue, even when nothing is received "
    "(e.g. with -Z).\n"
    "  -Z         : Do NOT receive and process/display replies.\n"
    " <name>      : Name of register or bitfield, or hex (0x) or decimal address.\n"
    "               (Name 'list' results in output of a list of all known "
    "items,"
    "                including their values if options -G/I are present).\n"
    " <value>     : Value to write to register or bitfield (hex or decimal).\n";
}

// ----------------------------------------------------------------------------
