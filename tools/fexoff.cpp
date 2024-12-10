#include <iostream>
#include <iomanip>
#include <cstdint>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxdefs.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x22012700; // Add more stuff, options -C/-L/-H
//const int VERSION_ID = 0x21091300; // Check firmware type
//const int VERSION_ID = 0x18042400;
//const int VERSION_ID = 0x17121100;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr    = 0;
  int  lnknr     = -1;
  int  set       = -1;
  int  thresh_lo = -1;
  int  thresh_hi = -1;
  bool clear     = false;
  string operation;

  // Parse the options
  while( (opt = getopt(argc, argv, "Cd:G:hH:L:V")) != -1 )
    {
      switch( opt )
        {
        case 'C':
          clear = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'G':
          // Link number
          if( sscanf( optarg, "%d", &lnknr ) != 1 )
            arg_error( 'G' );
          if( lnknr < 0 || lnknr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'H':
          // FIFO threshold High
          if( sscanf( optarg, "%d", &thresh_hi ) != 1 )
            arg_error( 'H' );
          if( thresh_hi < 1 || thresh_hi > 15 ) // 4 bits
            arg_range( 'H', 1, 15 );
          break;
        case 'L':
          // FIFO threshold Low
          if( sscanf( optarg, "%d", &thresh_lo ) != 1 )
            arg_error( 'L' );
          if( thresh_lo < 1 || thresh_lo > 15 ) // 4 bits
            arg_range( 'L', 1, 15 );
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

  // String defining the operation to perform?
  if( optind < argc )
    {
      operation = string( argv[optind] );
      if( operation == string("set") )
        set = 1;
      else if( operation == string("reset") )
        set = 0;
      else
        {
          cout << "### Unknown qualifier: " << operation << endl;
          usage();
          return 1;
        }
    }

  // Open FELIX FLX-card
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

  uint64_t fw_mode = bar2->FIRMWARE_MODE;
  if( fw_mode != FIRMW_FULL )
    cout << "### WARNING: not FULLMODE firmware" << endl;

  uint64_t chans = bar2->NUM_OF_CHANNELS;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  int max_lnknr;
  if( lnknr == -1 )
    {
      lnknr = 0;
      max_lnknr = chans;
    }
  else if( lnknr >= 0 && lnknr < (int) chans )
    {
      max_lnknr = lnknr + 1;
    }
  else
    {
      cout << "### Invalid Link number [0.." << chans-1
           << "] for this FLX-device" << endl;
      flx->card_close();
      return 1;
    }

  if( thresh_hi != -1 && thresh_lo != -1 )
    {
      if( thresh_lo <= thresh_hi )
        {
          bar2->XOFF_FM_CH_FIFO_THRESH_LOW  = thresh_lo;
          bar2->XOFF_FM_CH_FIFO_THRESH_HIGH = thresh_hi;
        }
      else
        {
          cout << "### High threshold smaller than low: not set" << endl;
        }
    }
  else if( thresh_lo != -1 )
    {
      thresh_hi = bar2->XOFF_FM_CH_FIFO_THRESH_HIGH;
      if( thresh_lo <= thresh_hi )
        bar2->XOFF_FM_CH_FIFO_THRESH_LOW = thresh_lo;
      else
        cout << "### High threshold smaller than low: low not set" << endl;
    }
  else if( thresh_hi != -1 )
    {
      thresh_lo = bar2->XOFF_FM_CH_FIFO_THRESH_LOW;
      if( thresh_lo <= thresh_hi )
        bar2->XOFF_FM_CH_FIFO_THRESH_HIGH = thresh_hi;
      else
        cout << "### High threshold smaller than low: high not set" << endl;
    }

  if( !(set == 1 || set == 0) || thresh_hi != -1 || thresh_lo != -1 )
    {
      // Display general XOFF settings
      cout << "FIFO threshold Low=" << bar2->XOFF_FM_CH_FIFO_THRESH_LOW
           << " High=" << bar2->XOFF_FM_CH_FIFO_THRESH_HIGH
           << " (4 MSBs, i.e. in 1/16ths of the FIFO size)" << endl;
    }

  // Setting thresholds is an operation we do separately from set/reset
  if( thresh_hi != -1 || thresh_lo != -1 )
    return 0;

  uint64_t xoff_ena = bar2->XOFF_ENABLE; // Get the current setting
  flxcard_xoff_statistics_t *xoff_stat = bar2->XOFF_STATISTICS;
  for( int lnk=lnknr; lnk<max_lnknr; ++lnk )
    {
      if( set == 1 )
        xoff_ena |= (1 << lnk);
      else if( set == 0 )
        xoff_ena &= ~(1 << lnk);

      cout << "Link " << setw(2) << lnk << " XOFF: ";
      if( xoff_ena & (1 << lnk) )
        cout << "ENABLED ";
      else
        cout << "disabled";

      cout << "  THRESH-X: L=" << ((bar2->XOFF_FM_LOW_THRESH_CROSSED>>lnk) & 1)
           << " H=" << ((bar2->XOFF_FM_HIGH_THRESH.CROSSED >> lnk) & 1)
           << " (latched="
           << ((bar2->XOFF_FM_HIGH_THRESH.CROSS_LATCHED >> lnk) & 1)
           << ")";
      
      cout << "  STATS: count=" << xoff_stat[lnk].XOFF_COUNT
           << " duration[25ns] total=" << xoff_stat[lnk].XOFF_TOTAL_DURATION
           << " peak=" << xoff_stat[lnk].XOFF_PEAK_DURATION;
      
      cout << endl;
    }

  if( clear && bar2->XOFF_FM_HIGH_THRESH.CROSS_LATCHED != 0 )
    {
      flx->cfg_set_option( BF_XOFF_FM_HIGH_THRESH_CLEAR_LATCH, 1 );
      cout << "Cleared latched threshold-cross bits" << endl;
    }

  if( set == 1 || set == 0 )
    bar2->XOFF_ENABLE = xoff_ena;

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
  cout << "fexoff version " << hex << VERSION_ID << dec << endl <<
    "Enable, disable and display XOFF feature settings for FLX-device links,"
    "for FULL-mode firmware.\n"
    "Also displays info about FIFO thresholds crossed (THRESH-X), as well as"
    " XON statistics (STATS).\n"
    "Without keyword '(re)set' the current setting is displayed.\n\n"
    "Usage: fexoff [-h|V] [-d <devnr>] [-C] [-L <low>] [-H <high>] "
    "[-G <lnk>] [set|reset]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -C         : Clear (latched) XOFF high-threshold-crossed bits.\n"
    "  -G <lnk>   : Link number (default: all).\n"
    "  -L <low>   : Set FIFO XOFF low threshold [1..15], in 1/16ths of size.\n"
    "  -H <high>  : Set FIFO XOFF high threshold [1..15], in 1/16ths of size.\n"
    "               (options -L/H can not be combined with a set/reset op)\n"
    "  set        : Enable link XOFF.\n"
    "  reset      : Disable link XOFF.\n";
}

// ----------------------------------------------------------------------------
