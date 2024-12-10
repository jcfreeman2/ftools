#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "flxdefs.h"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x22013100; // Some items Endpoint 0 only
//const int VERSION_ID = 0x20101300; // Added option -X/x (DMA thresholds)
                                     // and invert meaning of -T
//const int VERSION_ID = 0x20100700; // Added option -R (reset TTC decoder)
                                     // and -B (B-channel disable)
//const int VERSION_ID = 0x20093000; // BUSY-by-DMA thresholds displayed
//const int VERSION_ID = 0x20062600; // BUSY-by-OUTPUT-FIFO info
//const int VERSION_ID = 0x20061700; // BUSY-output info and config options
//const int VERSION_ID = 0x18013000;

/* Excerpt from JIRA FLX-1795 on BUSY:
   items per endpoint, or global ('card-wide') via endpoint 0 only:

   BUSY related registers that are configured per endpoint
   (in both endpoints):

   * BUSY_MAIN_OUTPUT_FIFO_THRESH.LOW
   * DMA_BUSY_STATUS_CLEAR_LATCH
   * DMA_BUSY_STATUS_TOHOST_BUSY_LATCHED
   * DMA_BUSY_STATUS_TOHOST_BUSY
   * BUSY_MAIN_OUTPUT_FIFO_THRESH_BUSY_ENABLE
   * BUSY_MAIN_OUTPUT_FIFO_THRESH_LOW
   * BUSY_MAIN_OUTPUT_FIFO_THRESH_HIGH
   * BUSY_MAIN_OUTPUT_FIFO_STATUS_CLEAR_LATCHED
   * BUSY_MAIN_OUTPUT_FIFO_STATUS_HIGH_THRESH_CROSSED_LATCHED
   * BUSY_MAIN_OUTPUT_FIFO_STATUS_HIGH_THRESH_CROSSED
   * BUSY_MAIN_OUTPUT_FIFO_STATUS_LOW_THRESH_CROSSED
   * All the addresses and status regarding DMA BUSY in BAR0

   BUSY related registers that are configured only through endpoint 0
   and are global for the card:

   * TTC_BUSY_CLEAR
   * TTC_DEC_CTRL_BUSY_OUTPUT_INHIBIT
   * TTC_DEC_CTRL_MASTER_BUSY
   * DMA_BUSY_STATUS_ENABLE
   * TTC_BUSY_ACCEPTED00 - TTC_BUSY_ACCEPTED23
   * TTC_DEC_CTRL_BUSY_OUTPUT_STATUS
   * ELINK_BUSY_ENABLE00
   * TTC_BUSY_TIMING_...
*/

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr     = 0;
  int  gbtnr      = -1;
  int  width      = -1;
  int  prescale   = -1;
  int  limit      = -1;
  bool clear      = false;
  bool debug      = false;
  bool skip_ttc   = true;
  bool reset      = false;
  // BUSY output
  int  master     = -1;
  int  inhibit    = -1;
  int  busy_ena   = -1;
  int  bchan_ena  = -1;
  int  thresh_on  = -1; // BUSY-by-DMA assert value
  int  thresh_d   = -1; // BUSY-by-DMA deassert difference with assert value

  // Parse the options
  while( (opt = getopt(argc, argv, "b:B:Cd:DG:hi:l:m:p:RTVw:X:x:")) != -1 )
    {
      switch( opt )
        {
        case 'b':
          // Enable/disable BUSY sources (DMA and FIFO)
          if( sscanf( optarg, "%d", &busy_ena ) != 1 )
            arg_error( 'b' );
          if( busy_ena < 0 || busy_ena > 1 )
            arg_range( 'b', 0, 1 );
          break;
        case 'B':
          // Enable/disable B-channel (limits L1A rate to ca. 200KHz)
          if( sscanf( optarg, "%d", &bchan_ena ) != 1 )
            arg_error( 'B' );
          if( bchan_ena < 0 || bchan_ena > 1 )
            arg_range( 'B', 0, 1 );
          break;
        case 'C':
          clear = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          debug = true;
          break;
        case 'G':
          // GBT link number
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          // Set/clear BUSY inhibit
          if( sscanf( optarg, "%d", &inhibit ) != 1 )
            arg_error( 'i' );
          if( inhibit < 0 || inhibit > 1 )
            arg_range( 'i', 0, 1 );
          break;
        case 'l':
          // Minimum required BUSY width
          if( sscanf( optarg, "%d", &limit ) != 1 )
            arg_error( 'l' );
          if( limit < 0 || limit > 65535 ) // 16 bits
            arg_range( 'l', 0, 0xFFFF );
          break;
        case 'm':
          // Set/clear Master BUSY
          if( sscanf( optarg, "%d", &master ) != 1 )
            arg_error( 'm' );
          if( master < 0 || master > 1 )
            arg_range( 'm', 0, 1 );
          break;
        case 'p':
          // 40MHz clock prescale
          if( sscanf( optarg, "%d", &prescale ) != 1 )
            arg_error( 'p' );
          if( prescale < 0 || prescale > 1048575 ) // 20 bits
            arg_range( 'p', 0, 0xFFFFF );
          break;
        case 'R':
          reset = true;
          break;
        case 'T':
          skip_ttc = false;
          break;
        case 'w':
          // Minimum asserted BUSY width
          if( sscanf( optarg, "%d", &width ) != 1 )
            arg_error( 'w' );
          if( width < 0 || width > 65535 ) // 16 bits
            arg_range( 'w', 0, 0xFFFF );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'X':
          // DMA BUSY assert value (in MB)
          if( sscanf( optarg, "%d", &thresh_on ) != 1 )
            arg_error( 'X' );
          if( thresh_on < 0 || thresh_on > 4096 )
            arg_range( 'X', 0, 4096 );
          break;
        case 'x':
          // DMA BUSY difference assert/deassert value (in MB)
          if( sscanf( optarg, "%d", &thresh_d ) != 1 )
            arg_error( 'x' );
          if( thresh_d < 0 || thresh_d > 4096 )
            arg_range( 'x', 0, 4096 );
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // If just displaying the settings never mind the firmware version
  bool ignore_version = (limit == -1 && clear == false && reset == false &&
                         master == -1 && inhibit == -1 && busy_ena == -1 &&
                         bchan_ena == -1 && thresh_on == -1 && thresh_d == -1);

  // Open FELIX FLX-card
  FlxCard *flx = new FlxCard;
  try {
    flx->card_open( cardnr, LOCK_NONE, ignore_version );
  }
  catch( FlxException &ex ) {
    cout << "### FlxCard open: " << ex.what() << endl;
    return 1;
  }

  flxcard_bar2_regs_t *bar2 = (flxcard_bar2_regs_t *) flx->bar2Address();

  uint64_t *bar0 = (uint64_t *) flx->bar0Address();

  uint64_t chans = bar2->NUM_OF_CHANNELS;
  //if( bar2->CARD_TYPE == 711 || bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  int max_gbtnr;
  if( gbtnr == -1 )
    {
      gbtnr = 0;
      // Used for global settings via endpoint 0, so covers *all* links
      max_gbtnr = chans * bar2->NUMBER_OF_PCIE_ENDPOINTS;
    }
  else if( gbtnr >= 0 && gbtnr < (int) chans )
    {
      max_gbtnr = gbtnr + 1;
    }
  else
    {
      cout << "### Invalid GBT number [0.." << chans-1
           << "] for this FLX-device" << endl;
      flx->card_close();
      return 1;
    }

  // Display and optionally set BUSY parameters
  // (NB: ### do NOT use the struct, because of byte-aligned bitfields
  //      and associated overwriting other bytes (within 32-bit word?)
  if( limit != -1 || !skip_ttc )
    cout << "TTC-BUSY timing: "
         << "*Prescale = " << bar2->TTC_BUSY_TIMING_CTRL.PRESCALE;
  if( prescale != -1 )
    {
      if( debug )
        bar2->TTC_BUSY_TIMING_CTRL.PRESCALE = prescale;
      else
        flx->cfg_set_option( BF_TTC_BUSY_TIMING_CTRL_PRESCALE, prescale );
      cout << " (set to: " << prescale << ") ";
    }
  if( limit != -1 || !skip_ttc )
    cout << " *Width = " << bar2->TTC_BUSY_TIMING_CTRL.BUSY_WIDTH;
  if( width != -1 )
    {
      if( debug )
        bar2->TTC_BUSY_TIMING_CTRL.BUSY_WIDTH = width;
      else
        flx->cfg_set_option( BF_TTC_BUSY_TIMING_CTRL_BUSY_WIDTH, width );
      cout << " (set to: " << width << ") ";
    }
  if( limit != -1 || !skip_ttc )
    cout << " *Limit-time = " << bar2->TTC_BUSY_TIMING_CTRL.LIMIT_TIME;
  if( limit != -1 )
    {
      if( debug )
        bar2->TTC_BUSY_TIMING_CTRL.LIMIT_TIME = limit;
      else
        flx->cfg_set_option( BF_TTC_BUSY_TIMING_CTRL_LIMIT_TIME, limit );
      cout << " (set to: " << limit << ") ";
    }
  if( limit != -1 || !skip_ttc )
    cout << endl;

  // ###TEST: byte-aligned multi-byte bitfields writing overwrites other bytes
  /*
  if( debug )
    {
      cout << " PACKAGE_COUNT = " << bar2->GEN_FM_CONTROL1.PACKAGE_COUNT;
      if( limit != -1 )
        {
          bar2->GEN_FM_CONTROL1.PACKAGE_COUNT = limit;
          //flx->cfg_set_option( BF_GEN_FM_CONTROL1_PACKAGE_COUNT, limit );
          cout << " (set to: " << limit << ") ";
        }
      cout << " PACKAGE_LENGTH = " << bar2->GEN_FM_CONTROL1.PACKAGE_LENGTH;
      if( width != -1 )
        {
          bar2->GEN_FM_CONTROL1.PACKAGE_LENGTH = width;
          //flx->cfg_set_option( BF_GEN_FM_CONTROL1_PACKAGE_LENGTH, width );
          cout << " (set to: " << width << ") ";
        }
      cout << endl;
    }
  */
  /*
  // ###TEST
  typedef struct test_it
  {
    uint32_t part1 : 16;
    uint32_t part2 : 16;
    uint32_t part3 : 16;
    uint32_t part4 : 16;
  } test_it_t;
  test_it_t tmp = { 12345,12345,12345,12345 };
  if( limit != -1 )
    {
      tmp.part1 = limit;
      cout << " (tmp1 set to: " << limit << ") ";
    }
  if( width != -1 )
    {
      tmp.part2 = width;
      cout << " (tmp2 set to: " << limit << ") ";
    }
  cout << tmp.part1 << " " << tmp.part2 << " "
       << tmp.part3 << " " << tmp.part4;
  cout << endl;
  */

  cout << setfill( '0' ) << uppercase;
  if( !debug && !skip_ttc )
    {
      // Display E-link BUSY-accepted and BUSY-enable registers and E-links
      uint64_t busy_acc, busy_ena;
      cout << "*E-link TTC-BUSY status (latched BUSY requests and "
           << "enables for BUSY output):" << endl;
      for( int gbt=gbtnr; gbt<max_gbtnr; ++gbt )
        {
          busy_acc = bar2->TTC_BUSY_ACCEPTED_G[gbt].TTC_BUSY_ACCEPTED;
          busy_ena = bar2->ELINK_BUSY_ENABLE[gbt].ELINK_BUSY_ENABLE;
          cout << "GBT #" << setw(2) << gbt << ": TTC-BUSY="
               << hex << setw((57+3)/4) << busy_acc
               << "  BUSY-ENA="
               << hex << setw((57+3)/4) << busy_ena
               << dec << endl;
          uint64_t busy_mask = 1;
          for( int i=0; i<57; ++i, busy_mask<<=1 )
            if( busy_acc & busy_mask )
              {
                int egroup = i/8, epath = (i & 7);
                if( i == 56 ) epath = 7; // For EC
                int elinknr = ((gbt    << BLOCK_GBT_SHIFT) |
                               (egroup << BLOCK_EGROUP_SHIFT) |
                               (epath  << BLOCK_EPATH_SHIFT));
                cout << hex << "  E = " << setw(3) << elinknr << " = "
                     << dec << gbt << "-" << egroup << "-" << epath;
                if( i == 56 ) cout << " (EC)";
                if( (busy_ena & busy_mask) == 0 )
                  cout << " (not enabled)";
                cout << endl;
              }
        }
    }

  // Optionally clear BUSY-accepted bits
  if( clear )
    {
      bar2->TTC_BUSY_CLEAR = 1;
      cout << "=> TTC-BUSYs CLEARED <=" << endl;
    }

  // Enable or disable BUSY-by-DMA and BUSY-by-FIFO
  if( busy_ena >= 0 )
    {
      bar2->DMA_BUSY_STATUS.ENABLE = busy_ena;
      //bar2->BUSY_MAIN_OUTPUT_FIFO_THRESH.BUSY_ENABLE = busy_ena;
      cout << "=> BUSY-by-DMA configured <=" << endl;
    }

  // BUSY-by-DMA
  cout << "BUSY-by-DMA  : *enabled=" << bar2->DMA_BUSY_STATUS.ENABLE << endl
       << "               ToHost=" << bar2->DMA_BUSY_STATUS.TOHOST_BUSY
       << " (BAR0:" << (bar0[0x490/8] & 0x1) << ")"
       << " (latched=" << bar2->DMA_BUSY_STATUS.TOHOST_BUSY_LATCHED
       << ")" << endl;
  if( thresh_on != -1 || thresh_d != -1 )
    {
      // Configure thresholds
      if( thresh_on == -1 )
        thresh_on = bar0[0x470/8]/0x100000;

      if( thresh_d == -1 )
        thresh_d = (bar0[0x480/8] - bar0[0x470/8])/0x100000;

      // Assert threshold
      bar0[0x470/8] = thresh_on*0x100000;

      // Deassert threshold
      bar0[0x480/8] = (thresh_on + thresh_d)*0x100000;

      cout << "               => Configured:" << endl;
    }
  cout << "               buffer free space: "
       << dec << "assert=" << bar0[0x470/8]/0x100000 << "MiB"
       << hex << " (" << bar0[0x470/8] << ")"
       << dec << " deassert=" << bar0[0x480/8]/0x100000 << "MiB"
       << hex << " (" << bar0[0x480/8] << ")"
       << dec << endl;
  if( clear && bar2->DMA_BUSY_STATUS.TOHOST_BUSY_LATCHED != 0 )
    {
      uint64_t c = bar2->DMA_BUSY_STATUS.ENABLE;
      bar2->DMA_BUSY_STATUS.ENABLE = c;
      cout << "=> CLEARED latched <=" << endl;
    }

  // BUSY-by-OUTPUT-FIFO
  cout << "BUSY-by-FIFO : enabled="
       << bar2->BUSY_MAIN_OUTPUT_FIFO_THRESH.BUSY_ENABLE << endl
       << hex << setw(3)
       << "               thresh: deassert="
       << bar2->BUSY_MAIN_OUTPUT_FIFO_THRESH.LOW
       << setw(3) << " assert="
       << bar2->BUSY_MAIN_OUTPUT_FIFO_THRESH.HIGH << endl
       << "               status: low_crossed="
       << bar2->BUSY_MAIN_OUTPUT_FIFO_STATUS.LOW_THRESH_CROSSED
       << " high_crossed="
       << bar2->BUSY_MAIN_OUTPUT_FIFO_STATUS.HIGH_THRESH_CROSSED
       << " (latched="
       << bar2->BUSY_MAIN_OUTPUT_FIFO_STATUS.HIGH_THRESH_CROSSED_LATCHED
       << ")" << dec << endl;
  if( clear &&
      bar2->BUSY_MAIN_OUTPUT_FIFO_STATUS.HIGH_THRESH_CROSSED_LATCHED != 0 )
    {
      uint64_t c = bar2->BUSY_MAIN_OUTPUT_FIFO_STATUS.HIGH_THRESH_CROSSED_LATCHED;
      bar2->BUSY_MAIN_OUTPUT_FIFO_STATUS.HIGH_THRESH_CROSSED_LATCHED = c;
      cout << "=> CLEARED latched <=" << endl;
    }

  // BUSY for FULLMODE
  cout << "BUSY FullMode: busy=" << hex << setw(6)
       << bar2->FM_BUSY_CHANNEL_STATUS.BUSY
       << " (latched=" << setw(6) << bar2->FM_BUSY_CHANNEL_STATUS.BUSY_LATCHED
       << ")" << endl;
  if( clear && bar2->FM_BUSY_CHANNEL_STATUS.BUSY_LATCHED != 0 )
    {
      uint64_t c = bar2->FM_BUSY_CHANNEL_STATUS.BUSY;
      bar2->FM_BUSY_CHANNEL_STATUS.BUSY = c;
      cout << "=> CLEARED latched <=" << endl;
    }

  // TTC B-channel / TriggerType
  cout << "TTC Bch/TType: *"
       << bar2->TTC_DEC_CTRL.TT_BCH_EN << endl;
  if( bchan_ena >= 0 )
    {
      bar2->TTC_DEC_CTRL.TT_BCH_EN = bchan_ena;
      cout << "=> Configured: "
           << "bchan-ena=" << bar2->TTC_DEC_CTRL.TT_BCH_EN << endl;
    }

  // Display BUSY output info and configure if required
  cout << "BUSY output  : *status=" << bar2->TTC_DEC_CTRL.BUSY_OUTPUT_STATUS
       << ", *inhibit=" << bar2->TTC_DEC_CTRL.BUSY_OUTPUT_INHIBIT
       << ", *master=" << bar2->TTC_DEC_CTRL.MASTER_BUSY << endl;
  if( master >= 0 || inhibit >= 0 )
    {
      if( master >= 0 )
        bar2->TTC_DEC_CTRL.MASTER_BUSY = master;
      if( inhibit >= 0 )
        bar2->TTC_DEC_CTRL.BUSY_OUTPUT_INHIBIT = inhibit;
      cout << "=> Configured: "
           << "status=" << bar2->TTC_DEC_CTRL.BUSY_OUTPUT_STATUS
           << ", inhibit=" << bar2->TTC_DEC_CTRL.BUSY_OUTPUT_INHIBIT
           << ", master=" << bar2->TTC_DEC_CTRL.MASTER_BUSY << endl;
    }

  if( reset )
    {
      // Reset the TTC decoder
      bar2->TTC_DEC_CTRL.TOHOST_RST = 1;
      bar2->TTC_DEC_CTRL.TOHOST_RST = 0;

      cout << "=> TTC decoder reset: done <=" << endl;
    }

  cout << "(NB: items marked by * are global, "
       << "access via card endpoint 0 only!)\n";

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
  cout << "fttcbusy version " << hex << VERSION_ID << dec << endl <<
    "Displays BUSY-related settings and optionally E-link TTC BUSY status "
    "and enables,\n"
    "optionally clearing (latched) E-link BUSY bits.\n"
    "With option -T the 'TTC BUSY accepted' register contents are displayed, "
    "as well as\n"
    "the corresponding E-link numbers, while option -C clears these registers "
    "after being displayed.\n"
    "Also the tool may be used to configure the TTC-BUSY signal settings "
    "(limit, prescale, width)\n"
    "and BUSY output settings (master, inhibit, B-channel, DMA treshold).\n"
    "Option -R resets the TTC decoder.\n"
    "NB: several items only read and set via FLX-card device 0\n"
    "    (in the output indicated by a '*').\n\n"
    "Usage: fttcbusy [-h|V] [-d <devnr>] [-G <gbt>] [-C] [-R] [-T]\n"
    "                [-l <limit>] [-p <prescale>] [-w <width>]\n"
    "                [-m <b>] [-i <b>] [-b <b>] [-B <b>] "
    "[-X <thresh>] [-x <diff>]\n"
    "  -h           : Show this help text.\n"
    "  -V           : Show version.\n"
    "  -d <devnr>   : FLX-device number (default: 0).\n"
    "  -G <gbt>     : GBT-link number (default: all).\n"
    "  -C           : Clear (latched) TTC-BUSY register bits.\n"
    "  -R           : Reset TTC decoder.\n"
    "  -T           : Display per-(E)link TTC-BUSY info.\n"
    "  -l <limit>   : Set TTC BUSY limit time parameter (16-bit).\n"
    "  -p <prescale>: Set TTC BUSY prescale parameter (20-bit).\n"
    "  -w <width>   : Set TTC BUSY width parameter (16-bit).\n"
    "  -m <b>       : Set (1) or clear (0) Master BUSY.\n"
    "  -i <b>       : Set (1) or clear (0) BUSY Inhibit (= BUSY off).\n"
    "  -b <b>       : Enable(1) or disable(0) BUSY-by-DMA\n"
    "                 (does not apply to BUSY-by-FIFO for the time being).\n"
    "  -B <b>       : Enable(1) or disable(0) TTC B-channel/TriggerType.\n"
    "                 (NB: limits TTCtoHost rate to ca. 200KHz max)\n"
    "  -X <thresh>  : Set BUSY-by-DMA assert threshold (in MiB).\n"
    "  -x <diff>    : Set BUSY-by-DMA difference assert/deassert thresholds "
    "(in MiB).\n";
}

// ----------------------------------------------------------------------------
