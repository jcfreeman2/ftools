#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
using namespace std;
#include <unistd.h>
#include <random>

#include "version.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x19030700;

bool read_cfg( const string &filename, vector<uint64_t> &cfg );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int      opt;
  int      cardnr      = 0;
  bool     display     = false;
  bool     display_hex = false;
  bool     random_gbt  = false;
  uint64_t link_mask   = 0x00FFFFFF; // 24 links
  string   filename;

  // Parse the options
  while( (opt = getopt(argc, argv, "c:d:DhRVX")) != -1 )
    {
      switch( opt )
        {
        case 'c':
          // Link select mask
          unsigned int x;
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          link_mask = x;
          if( link_mask > 0x00FFFFFF )
            arg_range( 'c', 0, 0x00FFFFFF );
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          display = true;
          break;
        case 'h':
          usage();
          return 0;
        case 'R':
          random_gbt = true;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'X':
          display = true;
          display_hex = true;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // ------------------------------------------
  // ###SPECIAL: load garbage into GBT emulator
  if( random_gbt )
    {
      // Open FELIX (emulator) card
      FlxCard *flx = new FlxCard;
      try {
        flx->card_open( cardnr, LOCK_NONE );
      }
      catch( FlxException &ex ) {
        cout << "### FlxCard open: " << ex.what() << endl;
        return 1;
      }

      uint64_t bar2_addr = flx->openBackDoor( 2 );
      flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

      // Check firmware version
      uint64_t fw_mode = bar2->FIRMWARE_MODE;
      if( fw_mode != 0 )
        cout << "### WARNING: not a GBT firmware (0) (FW mode = "
             << fw_mode << ")" << endl;

      // Write completely *random* emulator data into the FLX device
      std::uniform_int_distribution<uint64_t> distr( 0, 0xFFFF );
      std::random_device randev;
      for( uint32_t egroup=0; egroup<5; ++egroup ) // 5 Egroups
        {
          uint64_t egroup_bitmask = (1 << egroup);
          uint64_t addr = 0;
#if REGMAP_VERSION < 0x500
          const uint64_t EMU_CONFIG_ADDR_SHIFT = 32;
          for( uint64_t i=0; i<16384; ++i, addr+=(1ULL<<EMU_CONFIG_ADDR_SHIFT) )
            {
              uint64_t addr_plus_data = distr( randev ) | addr;
              flx->cfg_set_reg( REG_GBT_EMU_CONFIG, addr_plus_data );
              flx->cfg_set_reg( REG_GBT_EMU_CONFIG_WE_ARRAY, egroup_bitmask );
              flx->cfg_set_reg( REG_GBT_EMU_CONFIG_WE_ARRAY, 0 );
#else
          for( uint64_t i=0; i<16384; ++i, addr+=1ULL )
            {
              flx->cfg_set_option( BF_FE_EMU_CONFIG_WRADDR, addr );
              flx->cfg_set_option( BF_FE_EMU_CONFIG_WRDATA, distr( randev ) );
              flx->cfg_set_option( BF_FE_EMU_CONFIG_WE, egroup_bitmask );
              flx->cfg_set_option( BF_FE_EMU_CONFIG_WE, 0 );
#endif
            }
        }
      cout << "Random emulator data written: done\n";

      // Close the FLX-card
      try {
        flx->card_close();
      }
      catch( FlxException &ex ) {
        cout << "FlxCard close: " << ex.what() << endl;
      }
      return 0;
    }
  // ------------------------------------------

  // Name of file with 'RANDOM-RAM' configuration to write to FMEMU/FELIG-card
  if( optind < argc )
    filename = string( argv[optind] );
  if( filename.empty() )
    {
      cout << "### Missing configuration file name" << endl;
      return 1;
    }

  // Read the configuration file
  vector<uint64_t> cfg;
  if( !read_cfg( filename, cfg ) )
    return 1;

  // Optionally display contents read from the configuration file
  if( display )
    {
      if( display_hex )
        cout << hex << setfill('0') << uppercase;
      for( unsigned int i=0; i<cfg.size(); ++i )
        {
          cout << setw(4) << cfg[i] << " ";
          if( ((i+1) & 0xF) == 0x0 )
            cout << endl;
        }
      cout << dec << endl;
    }

  if( cfg.size() != 1024 )
    cout << "### WARNING: " << cfg.size() << " values, but expected 1024\n";

  // Open FELIX (emulator) card
  FlxCard *flx = new FlxCard;
  try {
    flx->card_open( cardnr, LOCK_NONE );
  }
  catch( FlxException &ex ) {
    cout << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }

  uint64_t bar2_addr = flx->openBackDoor( 2 );
  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) bar2_addr;

  // Check firmware version
  uint64_t fw_mode = bar2->FIRMWARE_MODE;
  if( !(fw_mode == FIRMW_FELIG || fw_mode == FIRMW_FMEMU) )
    cout << "### WARNING: not a FELIG(6) or FMEMU(7) (FW mode = "
         << fw_mode << ")" << endl;

  // Write the data to the emulator 'random ram'
  bar2->FMEMU_RANDOM_RAM.CHANNEL_SELECT = link_mask;
  uint64_t addr = 0;
  for( uint64_t v: cfg )
    {
      bar2->FMEMU_RANDOM_RAM_ADDR = addr;
      bar2->FMEMU_RANDOM_RAM.DATA = v;
      ++addr;
    }

  cout << "Data written to ";
  if( fw_mode == 6 )
    cout << "FELIG ";
  else if( fw_mode == 7 )
    cout << "FMEMU ";
  else
    cout << "FLX ";
  cout << "device, using link select mask 0x"
       << hex << link_mask << endl;

  // Close the FLX-card
  try {
    flx->card_close();
  }
  catch( FlxException &ex ) {
    cout << "FlxCard close: " << ex.what() << endl;
  }

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "femuran version " << hex << VERSION_ID << dec << endl <<
    "Configure FELIG or FMEMU 'random ram' memory with '.coe' file contents.\n"
    "Usage: femuran [-h|V] [-d <devnr>] [-c <mask>] [-D|X] <filename>\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -c <mask>  : Link select mask (hex) to use (default: FFFFFF)\n"
    "  -D         : Display the values read from the file.\n"
    "  -X         : Display the values read from the file, hexadecimal.\n"
    " <filename>  : Name of (.coe) file with 'random ram' configuration.\n";
}

// ----------------------------------------------------------------------------

bool read_cfg( const string &filename, vector<uint64_t> &cfg )
{
  ifstream fin( filename );
  if( !fin.is_open() )
    {
      cout << "###Failed to open file: " << filename << endl;
      return false;
    }

  string        line, s;
  char          ch;
  int           line_nr = 0;
  uint64_t      val;
  istringstream iss;
  iss >> skipws; // Skip whitespace
  iss >> hex;    // Read hexadecimal values
  while( !fin.eof() )
    {
      getline( fin, line );
      if( fin.eof() ) break;
      ++line_nr;

      // Going to process this line:
      iss.clear(); // Clear error bits
      iss.str( line );
      iss >> s;
      if( iss.fail() || s[0] == ';' || s[0] == '\n' )
        continue; // Skip this (comment or empty) line

      // Skip line(s) containing "radix"
      size_t pos = line.find( "radix" );
      if( pos != string::npos )
        continue; // Skip this line

      // Search for potential occurrence of '=',
      // and skip line contents until *after* the '='
      pos = line.find_first_of( "=" );
      if( pos != string::npos )
        line = line.substr( pos+1 );

      iss.clear(); // Clear error bits
      iss.str( line );
      while( !iss.eof() )
        {
          // Read values one-by-one, separated by space or comma
          iss >> val;
          if( !iss.fail() )
            {
              cfg.push_back( val ); // Store value
            }
          else
            {
              iss.clear(); // Clear error bits
              iss >> ch;
              if( ch != ',' ) // Allow comma separator
                break; // Not a comma? Bail out...
            }
        }
    }
  cout << dec << "Read " << cfg.size() << " values\n";
  return true;
}

// ----------------------------------------------------------------------------
