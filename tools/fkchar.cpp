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
const int VERSION_ID = 0x17061500;

// 8b10b control symbols or K-characters:
// value, RD- value, RD+ value
const uint64_t KCHAR_CONFIG[][3] = {
  { 0x1C, 0x0F4, 0x30B }, // K28.0
  { 0x3C, 0x0F9, 0x306 }, // K28.1
  { 0x5C, 0x0F5, 0x30A }, // K28.2
  { 0x7C, 0x0F3, 0x30C }, // K28.3
  { 0x9C, 0x0F2, 0x30D }, // K28.4
  { 0xBC, 0x0FA, 0x305 }, // K28.5
  { 0xDC, 0x0F6, 0x309 }, // K28.6
  { 0xFC, 0x0F8, 0x307 }, // K28.7
  { 0xF7, 0x3A8, 0x057 }, // K23.7
  { 0xFB, 0x368, 0x097 }, // K27.7
  { 0xFD, 0x2E8, 0x117 }, // K29.7
  { 0xFE, 0x1E8, 0x217 }  // K30.7
};

const string KCHAR_STR[] = {
  "K28.0",
  "K28.1",
  "K28.2",
  "K28.3",
  "K28.4",
  "K28.5",
  "K28.6",
  "K28.7",
  "K23.7",
  "K27.7",
  "K29.7",
  "K30.7" };

// ----------------------------------------------------------------------------

string kchar_str( uint64_t byte, uint64_t n, uint64_t p )
{
  for( int i=0; i<12; ++i )
    if( byte == KCHAR_CONFIG[i][0] &&
	n    == KCHAR_CONFIG[i][1] &&
	p    == KCHAR_CONFIG[i][2] )
      return KCHAR_STR[i];
  return string( "???" );
}

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int    opt;
  int    cardnr   = 0;
  int    kchar    = -1;
  bool   do_comma = false;
  bool   do_soc   = false;
  bool   do_eoc   = false;
  bool   do_sob   = false;
  bool   do_eob   = false;
  string operation;

  // Parse the options
  while( (opt = getopt(argc, argv, "d:hV")) != -1 )
    {
      switch( opt )
        {
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'h':
          usage();
          return 0;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        default: // '?'
          usage();
          return 1;
        }
    }

  // String defining the K-CHAR to configure
  if( optind < argc )
    {
      operation = string( argv[optind] );
      if( operation == string("comma") )
        do_comma = true;
      else if( operation == string("soc") )
        do_soc = true;
      else if( operation == string("eoc") )
        do_eoc = true;
      else if( operation == string("sob") )
        do_sob = true;
      else if( operation == string("eob") )
        do_eob = true;
      else
        {
          cout << "### Unknown qualifier: " << operation << endl;
          usage();
          return 1;
        }
    }

  // The K-character 'index' to set
  ++optind;
  if( optind < argc )
    {
      if( sscanf( argv[optind], "%d", &kchar ) == 1 )
	{
	  if( kchar < 0 || kchar > 11 )
	    {
	      cout << "### K-char index out-of-range 0..11" << endl;
	      return 1;
	    }
	}
      else
	{
	  cout << "### Invalid K-char index" << endl;
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

  if( kchar != -1 )
    {
      cout << "### KCHARs are not (yet) configurable unfortunately!\n";

      if( do_comma )
	{
	  bar2->COMMA_CHAR.BYTE = KCHAR_CONFIG[kchar][0];
	  bar2->COMMA_CHAR.N    = KCHAR_CONFIG[kchar][1];
	  bar2->COMMA_CHAR.P    = KCHAR_CONFIG[kchar][2];
	}
      if( do_soc )
	{
	  bar2->SOC_CHAR.BYTE = KCHAR_CONFIG[kchar][0];
	  bar2->SOC_CHAR.N    = KCHAR_CONFIG[kchar][1];
	  bar2->SOC_CHAR.P    = KCHAR_CONFIG[kchar][2];
	}
      if( do_eoc )
	{
	  bar2->EOC_CHAR.BYTE = KCHAR_CONFIG[kchar][0];
	  bar2->EOC_CHAR.N    = KCHAR_CONFIG[kchar][1];
	  bar2->EOC_CHAR.P    = KCHAR_CONFIG[kchar][2];
	}
      if( do_sob )
	{
	  bar2->SOB_CHAR.BYTE = KCHAR_CONFIG[kchar][0];
	  bar2->SOB_CHAR.N    = KCHAR_CONFIG[kchar][1];
	  bar2->SOB_CHAR.P    = KCHAR_CONFIG[kchar][2];
	}
      if( do_eob )
	{
	  bar2->EOB_CHAR.BYTE = KCHAR_CONFIG[kchar][0];
	  bar2->EOB_CHAR.N    = KCHAR_CONFIG[kchar][1];
	  bar2->EOB_CHAR.P    = KCHAR_CONFIG[kchar][2];
	}
    }

  if( !(do_comma || do_soc || do_eoc || do_sob || do_eob) )
    {
      do_comma = true; do_soc = true; do_eoc = true;
      do_sob = true; do_eob = true;
    }

  uint64_t byte, n, p;
  cout << hex << setfill('0');
  if( do_comma )
    {
      byte = bar2->COMMA_CHAR.BYTE;
      n    = bar2->COMMA_CHAR.N;
      p    = bar2->COMMA_CHAR.P;
      cout << "COMMA: " << setw(3) << byte << " "
	   << setw(3) << n << " " << setw(3) << p
	   << " (" << kchar_str(byte,n,p) << ")" << endl;
    }
  if( do_soc )
    {
      byte = bar2->SOC_CHAR.BYTE;
      n    = bar2->SOC_CHAR.N;
      p    = bar2->SOC_CHAR.P;
      cout << "SOC  : " << setw(3) << byte << " "
	   << setw(3) << n << " " << setw(3) << p
	   << " (" << kchar_str(byte,n,p) << ")" << endl;
    }
  if( do_eoc )
    {
      byte = bar2->EOC_CHAR.BYTE;
      n    = bar2->EOC_CHAR.N;
      p    = bar2->EOC_CHAR.P;
      cout << "EOC  : " << setw(3) << byte << " "
	   << setw(3) << n << " " << setw(3) << p
	   << " (" << kchar_str(byte,n,p) << ")" << endl;
    }
  if( do_sob )
    {
      byte = bar2->SOB_CHAR.BYTE;
      n    = bar2->SOB_CHAR.N;
      p    = bar2->SOB_CHAR.P;
      cout << "SOB  : " << setw(3) << byte << " "
	   << setw(3) << n << " " << setw(3) << p
	   << " (" << kchar_str(byte,n,p) << ")" << endl;
    }
  if( do_eob )
    {
      byte = bar2->EOB_CHAR.BYTE;
      n    = bar2->EOB_CHAR.N;
      p    = bar2->EOB_CHAR.P;
      cout << "EOB  : " << setw(3) << byte << " "
	   << setw(3) << n << " " << setw(3) << p
	   << " (" << kchar_str(byte,n,p) << ")" << endl;
    }

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
  cout << "fkchar version " << hex << VERSION_ID << dec << endl <<
    "Configure or display the K-characters used by the FLX-device.\n"
    "Usage: fkchar [-h|V] [-d <devnr>] [comma|soc|eoc|sob|eob] [<index>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  comma      : IDLE character.\n"
    "  soc        : Start-of-Chunk character.\n"
    "  eoc        : End-of-Chunk character.\n"
    "  sob        : Start-of-BUSY character.\n"
    "  eob        : End-of-BUSY character.\n"
    "  <index>    : K-character to set to (*NOT IMPLEMENTED*)\n"
    "               (0-7: K28.0-7, 8: K23.7, 9: K27.7, 10: K29.7, 11: K30.7)"
    "\n";
}

// ----------------------------------------------------------------------------
