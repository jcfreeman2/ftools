#include <iostream>
#include <iomanip>
using namespace std;
#include <cstdio>
//#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <sstream>
#include "HptdcString.h"
#include <bitset>
// Maximum number of TDCs
#define MAX_TDCS 18

// Default HPTDC setup string:
// (Received from Alexis Mulski, June 2018)
// FOR PAIR MODE (COMBINED LEADING AND TRAILING)
/*
const unsigned char DEFAULT_HPTDC_SETUP_STRING[] = {
  0xCF, 0xFF, 0x01, 0x00, 0x18, 0xE0, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x90, 0x08,
  0x00, 0x00, 0x40, 0x70, 0x00, 0x00, 0x00, 0x00,
    
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C,
  0x10, 0x01, 0x11, 0x00, 0x00, 0x40, 0xFA, 0x7F,
  0x4B
};
*/
//Alexis Mulski, August 2019
//PAIR MODE, 400 ps WIDTH RESOLUTION
/*
const unsigned char DEFAULT_HPTDC_SETUP_STRING[] = {
  0xCF, 0xFF, 0x01, 0x00, 0x18, 0xE0, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x90, 0x08,
  0x00, 0x00, 0x40, 0x70, 0x00, 0x00, 0x00, 0x00,
    
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14,
  0x10, 0x01, 0x11, 0x00, 0x00, 0x40, 0xFA, 0x7F,
  0x4B
};
*/
//Alexis Mulskim August 2019
//pair mode, 400 ps width resolution, dead time 5 ns (all others have 10 ns deadtime unless otherwise noted)
const unsigned char DEFAULT_HPTDC_SETUP_STRING[] = {
  0xCF, 0xFF, 0x01, 0x00, 0x18, 0xE0, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x90, 0x08,
  0x00, 0x00, 0x40, 0x70, 0x00, 0x00, 0x00, 0x00,
    
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14,
  0x10, 0x00, 0x11, 0x00, 0x00, 0x40, 0xFA, 0x7F,
  0x4B
};


//Alexis Mulski, July 2019
//FOR LEADING EDGE MODE
/*
const unsigned char DEFAULT_HPTDC_SETUP_STRING[] = {
  0xCF, 0xFF, 0x01, 0x00, 0x18, 0xE0, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x90, 0x08,
  0x00, 0x00, 0x40, 0x70, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C,
  0x10, 0x21, 0x11, 0x00, 0x00, 0x40, 0xFA, 0x7F,
  0x4A
};
*/

// (Copy from Sergey's code; use option -D to inspect parameter values)
/*
const unsigned char DEFAULT_HPTDC_SETUP_STRING[] = {
  0xDE, 0xFF, 0x01, 0x84, 0x19, 0xE0, 0x87, 0xBF,
  0x04, 0x32, 0x10, 0x00, 0x00, 0x00, 0x90, 0x39,
  0x00, 0xBC, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
  0x10, 0x30, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xFF,
  0x00
};
*/
// For information only:
// 'phases' used in MDT HPTDC initialization are:
//   PHASE_HPTDC_STARTUP_1,
//   PHASE_HPTDC_SETUP_WR,  PHASE_HPTDC_SETUP_RD,
//   PHASE_HPTDC_STARTUP_2, PHASE_HPTDC_STARTUP_3,
//   PHASE_HPTDC_STARTUP_4, PHASE_HPTDC_STARTUP_5,
//   PHASE_HPTDC_STATUS

// Array of pointers to JTAG strings for (up to 18) HPTDCs
HptdcString  *HptdcSetup[MAX_TDCS];
HptdcString  *HptdcCntrl[MAX_TDCS];

// Combined devices JTAG instruction and data string
JtagString *Instr, *Data;
int        iLen, dLen;

// Array of bits with value one to represent a series of BYPASS instructions
// (max. 256 bits)
unsigned char ALL_BITS_1[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// ----------------------------------------------------------------------------

void usage();
void arg_error( char opt );
void arg_range( char opt, int min, int max );

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x18091100;
//const int VERSION_ID = 0x18042000;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  tdc_count    = 1;
  int  tdc_mask     = 0x00001;
  int  devs_before  = 0;
  int  ibits_before = 0;
  int  devs_after   = 0;
  int  ibits_after  = 0;
  int  dev_instr    = -1;
  bool read_id      = false;
  bool debug        = false;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "c:Dhim:Vx:X:y:Y:z:")) != -1 )
    {
      switch( opt )
        {
        case 'c':
          if( sscanf( optarg, "%d", &tdc_count ) != 1 )
            arg_error( 'c' );
          if( tdc_count <= 0 || tdc_count > MAX_TDCS )
            arg_range( 'c', 1, MAX_TDCS );
          break;
        case 'D':
          debug = true;
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          read_id = true;
          break;
        case 'm':
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'm' );
          tdc_mask = x;
          if( tdc_mask < 0 || tdc_mask > 0x3FFFF )
            arg_range( 'm', 0, 0x3FFFF );
          break;
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec << endl;
          return 0;
        case 'x':
          if( sscanf( optarg, "%d", &devs_before ) != 1 )
            arg_error( 'x' );
          if( devs_before < 1 || devs_before > 32 )
            arg_range( 'x', 1, 32 );
          break;
        case 'X':
          // Total number of 'before' instruction bits, or
          // -with option -z given- the number of instruction bits per device
          if( sscanf( optarg, "%d", &ibits_before ) != 1 )
            arg_error( 'X' );
          if( ibits_before < 1 || ibits_before > 256 )
            arg_range( 'X', 1, 256 );
          break;
        case 'y':
          if( sscanf( optarg, "%d", &devs_after ) != 1 )
            arg_error( 'y' );
          if( devs_after < 1 || devs_after > 32 )
            arg_range( 'y', 1, 32 );
          break;
        case 'Y':
          // Total number of 'after' instruction bits, or
          // -with option -z given- the number of instruction bits per device
          if( sscanf( optarg, "%d", &ibits_after ) != 1 )
            arg_error( 'Y' );
          if( ibits_after < 1 || ibits_after > 256 )
            arg_range( 'Y', 1, 256 );
          break;
        case 'z':
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'z' );
          dev_instr = x;
          if( dev_instr < 0 || dev_instr > 0xFFFF )
            arg_range( 'z', 0, 0xFFFF );
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Check provided mask versus number of HPTDCs
  if( tdc_mask > (1<<tdc_count)-1 )
    {
      cout << "### TDC count/mask mismatch" << endl;
      return 1;
    }

  // Check provided values for other devices in the JTAG chain
  // before and after the HPTDC devices
  if( dev_instr < 0 &&
      (ibits_before < devs_before || ibits_after < devs_after) )
    {
      if( ibits_before < devs_before )
        cout << "### Less instruction bits (" << ibits_before
             << ") than devices (" << devs_before << ") (front)" << endl;
      if( ibits_after < devs_after )
        cout << "### Less instruction bits (" << ibits_after
             << ") than devices (" << devs_after << ") (back)" << endl;
      return 1;
    }
  else if( dev_instr > 0 )
    {
      // A BYPASS instruction for the other devices
      // other than just 1s was provided
      if( (devs_before > 0 && dev_instr > ((1<<ibits_before)-1)) ||
          (devs_after > 0 && dev_instr > ((1<<ibits_after)-1)) )
        {
          cout << "### Instruction does not fit in number of bits" << endl;
          return 1;
        }
    }

  // Allocate space for the composite (multi-device)
  // JTAG instruction and data bit strings
  Instr = new JtagString( 1024 );
  Data  = new JtagString( 16384 );


//Alexis, November 2019
cout << "Which channels would you like disabled? "
     << "Default: all enabled. (format: 0 1 ... 31 done): ";
//const int MAX_CHANNELS = 32;
string new_channels = "11111111111111111111111111111111";
string chan;
cin >> chan;
while(chan != "done") {
//      cout << chan << endl;
//      cout << "iterate" << endl;
        
        if(std::stoi(chan) >= 0 && std::stoi(chan) <= 31) {
                new_channels[std::stoi(chan)] = '0';            
//              cout << "out of if ??" << endl;
        }
//      cout << new_channels << endl;
//      cout << "hey" << endl;
        cin >> chan;
} 
//std::cout << "out of while loop" << endl;
//std::cout << new_channels << std::endl;
unsigned int new_channels_hex = 0xFFFFFFFF;

bitset<32> set(new_channels);
stringstream res;
res << hex << uppercase << set.to_ulong(); 
new_channels_hex = std::stoul(res.str(), nullptr, 16);

//cout << "hex_new_channels: " << res.str() << std::endl; 

  // Instantiate individual HPTDC configuration and control JTAG strings
  int i;
  for( i=0; i<tdc_count; ++i )
    {
      HptdcSetup[i] = new HptdcString( HPTDC_SETUP_STRING_SIZE,
                                       DEFAULT_HPTDC_SETUP_STRING );
      HptdcCntrl[i] = new HptdcString( HPTDC_CNTRL_STRING_SIZE );

      HptdcSetup[i]->setParity();

      HptdcCntrl[i]->setBits( HPTDC_ENABLE_PATTERN_I,
                              HPTDC_ENABLE_PATTERN_BITS,
                              0x04 );
      HptdcCntrl[i]->setBits( HPTDC_ENABLE_CHANNEL_I,
                              HPTDC_ENABLE_CHANNEL_BITS,
                              //0xFFFFFF ); // 24 channels
                              //0xFFFFFFFF ); // 32 channels
                              new_channels_hex); //custom enabled/disabled channels
      HptdcCntrl[i]->setParity();
    }

  if( debug )
    {
      cout << "HPTDC JTAG bits:" << endl;
      cout << "================" << endl;
      HptdcSetup[0]->show();
      cout << endl;
      HptdcCntrl[0]->show();
      cout << endl;
    }

  // Generate instruction and data strings for the given number of TDCs
  // and TDC mask for the selected sequence of instructions
  //int instr_seq_init[] = { HPTDC_CONTROL_1_INSTR,
  //                         HPTDC_SETUP_INSTR,
  //                         HPTDC_CONTROL_2_INSTR,
  //                         HPTDC_CONTROL_3_INSTR,
  //                         HPTDC_CONTROL_4_INSTR,
  //                         HPTDC_CONTROL_5_INSTR };
  //int ninstr_init = 6; // Number of instructions in sequence

  // Alex Mulski's version (June 2018):
  int instr_seq_init[] = { HPTDC_SETUP_INSTR,
                           HPTDC_PLL_DLL_RESET_ON_INSTR,
                           HPTDC_DLL_RESET_ON_INSTR,
                           HPTDC_ALL_RESETS_OFF_INSTR,
                           HPTDC_GLOBAL_RESET_ON_INSTR,
                           HPTDC_ALL_RESETS_OFF_INSTR,
                           HPTDC_PATTERN_5_INSTR };
  int ninstr_init = 7; // Number of instructions in sequence

  int instr_seq_id[] = { IDCODE_INSTR };
  int ninstr_id = 1; // Number of instructions in sequence

  // Select the JTAG sequence: initialization or ID reading
  int ninstr, *instr_seq;
  if( read_id )
    {
      ninstr = ninstr_id;
      instr_seq = instr_seq_id;
    }
  else
    {
      ninstr = ninstr_init;
      instr_seq = instr_seq_init;
    }

  if( debug )
    {
      cout << "Generating the sequence:" << endl;
      cout << "========================" << endl;
    }

  // Show info about the generated strings' HPTDC mask
  cout << "# JTAG for " << tdc_count << " HPTDCs, mask=0x"
       << hex << setfill('0') << setw(5) << tdc_mask << endl
       << dec << setfill(' ');

  // Show info about additional devices in the JTAG chain
  // before and after the HPTDC devices
  if( devs_before > 0 || devs_after > 0 )
    {
      if( devs_before > 0 )
        {
          cout << "# Devices before: " << devs_before << " ";
          if( dev_instr >= 0 )
            cout << "Instr=0x" << hex << dev_instr << dec << " (total bits: "
                 << devs_before*ibits_before << ")" << endl;
          else
            cout << "(total bits: " << ibits_before << ")" << endl;
        }
      if( devs_after > 0 )
        {
          cout << "# Devices after: " << devs_after << ", ";
          if( dev_instr >= 0 )
            cout << "Instr=0x" << hex << dev_instr << dec << " (total bits: "
                 << devs_after*ibits_after << ")" << endl;
          else
            cout << "(total bits: " << ibits_after << ")" << endl;
        }
    }

  if( read_id )
    cout << "# (READ-ID sequence)" << endl;
  else
    cout << "# (INIT sequence)" << endl;

  // Loop over instructions sequentially
  for( i=0; i<ninstr; ++i )
    {
      int instr_id = instr_seq[i];

      iLen = 0;
      dLen = 0;

      // Insert BYPASS for the devices present in the JTAG chain
      // *after* the TDCs
      if( dev_instr >= 0 )
        {
          // Assume the BYPASS instruction is 'dev_instr' for each device
          // with instruction length 'ibits_after'
          for( int dev=devs_after-1; dev>= 0; --dev )
            {
              Instr->setBits( iLen, ibits_after, dev_instr );
              iLen += ibits_after;
              Data->setBits( dLen, 1, 1 ); // BYPASS data bit (=1)
              dLen += 1;
            }
        }
      else
        {
          // Assume the BYPASS instruction is all 1s
          // independent of individual instruction sizes
          // and 'ibits_after' is the *total* number of instruction bits
          Instr->setBits( iLen, ibits_after, 0, ALL_BITS_1 );
          iLen += ibits_after;
          for( int dev=devs_after-1; dev>=0; --dev )
            {
              Data->setBits( dLen, 1, 1 ); // BYPASS data bit (=1)
              dLen += 1;
            }
        }

      // Loop over enabled TDCs sequentially (from last to first in the chain)
      for( int tdc_i=tdc_count-1; tdc_i>= 0; --tdc_i )
        {
          if( tdc_mask & (1<<tdc_i) )
            {
              Instr->setBits( iLen, HPTDC_INSTR_SIZE,
                              HptdcString::instruction( instr_id ) );
              iLen += HPTDC_INSTR_SIZE;
              
              switch( instr_id )
                {
                case IDCODE_INSTR:
                  Data->clearBits( dLen, 32 );
                  dLen += 32;
                  break;

                case HPTDC_SETUP_INSTR:
                  // Additional setting (Alex Mulski, June 2018):
                  HptdcSetup[tdc_i]->setBits( HPTDC_TDC_ID_I,
                                              HPTDC_TDC_ID_BITS,
                                              tdc_i );
                  HptdcSetup[tdc_i]->setParity(); // Recalculate parity

                  Data->setBits( dLen, HPTDC_SETUP_STRING_SIZE,
                                 0, HptdcSetup[tdc_i]->string() );
                  dLen += HPTDC_SETUP_STRING_SIZE;
                  break;

                case HPTDC_STATUS_INSTR:
                  Data->clearBits( dLen, HPTDC_STATUS_STRING_SIZE );
                  dLen += HPTDC_STATUS_STRING_SIZE;
                  break;

                //case HPTDC_CONTROL_1_INSTR:
                //case HPTDC_CONTROL_2_INSTR:
                //case HPTDC_CONTROL_3_INSTR:
                //case HPTDC_CONTROL_4_INSTR:
                //case HPTDC_CONTROL_5_INSTR:
                case HPTDC_DLL_RESET_ON_INSTR:
                case HPTDC_ALL_RESETS_OFF_INSTR:
                case HPTDC_GLOBAL_RESET_ON_INSTR:
                case HPTDC_PATTERN_5_INSTR:
                case HPTDC_PLL_DLL_RESET_ON_INSTR:
                  if( instr_id == HPTDC_ALL_RESETS_ON_INSTR )
                    {
                      HptdcCntrl[tdc_i]->setBits( HPTDC_GLOBAL_RESET, 1, 1 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_DLL_RESET,    1, 1 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_PLL_RESET,    1, 1 );
                    }
                  else if( instr_id == HPTDC_DLL_RESET_ON_INSTR )
                    {
                      HptdcCntrl[tdc_i]->setBits( HPTDC_GLOBAL_RESET, 1, 0 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_DLL_RESET,    1, 1 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_PLL_RESET,    1, 0 );
                    }
                  else if( instr_id == HPTDC_ALL_RESETS_OFF_INSTR )
                    {
                      HptdcCntrl[tdc_i]->setBits( HPTDC_GLOBAL_RESET, 1, 0 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_DLL_RESET,    1, 0 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_PLL_RESET,    1, 0 );
                    }
                  else if( instr_id == HPTDC_GLOBAL_RESET_ON_INSTR )
                    {
                      HptdcCntrl[tdc_i]->setBits( HPTDC_GLOBAL_RESET, 1, 1 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_DLL_RESET,    1, 0 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_PLL_RESET,    1, 0 );
                    }
                  else if( instr_id == HPTDC_PATTERN_5_INSTR )
                    {
                      HptdcCntrl[tdc_i]->setBits( HPTDC_GLOBAL_RESET, 1, 0 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_DLL_RESET,    1, 0 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_PLL_RESET,    1, 0 );
                      // Additional setting (Alex Mulski, June 2018):
                      HptdcCntrl[tdc_i]->setBits( HPTDC_ENABLE_PATTERN_I,
                                                  HPTDC_ENABLE_PATTERN_BITS,
                                                  0x05 );
                    }
                  else if( instr_id == HPTDC_PLL_DLL_RESET_ON_INSTR )
                    {
                      HptdcCntrl[tdc_i]->setBits( HPTDC_GLOBAL_RESET, 1, 0 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_DLL_RESET,    1, 1 );
                      HptdcCntrl[tdc_i]->setBits( HPTDC_PLL_RESET,    1, 1 );
                    }

                  HptdcCntrl[tdc_i]->setParity(); // Recalculate parity
                  Data->setBits( dLen, HPTDC_CNTRL_STRING_SIZE,
                                 0, HptdcCntrl[tdc_i]->string() );
                  dLen += HPTDC_CNTRL_STRING_SIZE;
                  break;

                case BYPASS_INSTR:
                default:
                  Data->setBits( dLen, 1, 1 ); // BYPASS data bit (=1)
                  dLen += 1;
                  break;
                }
            }
          else
            {
              // BYPASS instruction
              Instr->setBits( iLen, HPTDC_INSTR_SIZE,
                              HptdcString::instruction( BYPASS_INSTR ) );
              iLen += HPTDC_INSTR_SIZE;

              // BYPASS data bit
              Data->setBits( dLen, 1, 1 );
              dLen += 1;
            }
        }

      // Insert BYPASS for the devices present in the JTAG chain
      // *before* the TDCs
      if( dev_instr >= 0 )
        {
          // Assume the BYPASS instruction is 'dev_instr' for each device
          // with instruction length 'ibits_before'
          for( int dev=devs_before-1; dev>= 0; --dev )
            {
              Instr->setBits( iLen, ibits_before, dev_instr );
              iLen += ibits_before;
              Data->setBits( dLen, 1, 1 ); // BYPASS data bit (=1)
              dLen += 1;
            }
        }
      else
        {
          // Assume the BYPASS instruction is all 1s
          // independent of individual instruction sizes
          // and 'ibits_after' is the total number of instruction bits
          Instr->setBits( iLen, ibits_before, 0, ALL_BITS_1 );
          iLen += ibits_before;
          for( int dev=devs_before-1; dev>=0; --dev )
            {
              Data->setBits( dLen, 1, 1 ); // BYPASS data bit (=1)
              dLen += 1;
            }
        }

      // Display compiled instruction and data strings
      if( debug )
        {
          // For visual inspection
          cout << "=> JTAG pair #" << i << ":" << endl;
          cout << "Instr-len=" << iLen << endl;
          Instr->show( iLen );
          cout << "Data-len=" << dLen << endl;
          Data->show( dLen );
          cout << endl;
        }
      else
        {
          // In the 'fscajfile' file format
          int len;
          unsigned char *str;

          str = Instr->string();
          cout << dec << setfill( ' ' );
          cout << "I " << setw(4) << iLen;
          cout << hex << setfill( '0' );
          for( int i=0; i<(iLen+7)/8; ++i )
            cout << " " << setw(2) << (unsigned int) str[i];
          cout << endl << dec;

          str = Data->string();
          cout << dec << setfill( ' ' );
          cout << "D " << setw(4) << dLen;
          cout << hex << setfill( '0' );
          for( int i=0; i<(dLen+7)/8; ++i )
            cout << " " << setw(2) << (unsigned int) str[i];
          cout << endl << dec;
        }
    }
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "bis78-jtag Version " << hex << VERSION_ID << dec << endl <<
    "Tool to generate JTAG strings for BIS78 HPTDC initialization\n"
    "in a format suitable as input to 'fscajfile'.\n"
    "Usage:\n"
    " bis78-jtag [-h|V] [-c <tdccount>] [-m <tdcmask>]\n"
    "            [-x <devs> -X <ibits>] [-y <devs> -Y <ibits>] [-z <instr>]\n"
    "  -h           : Show this help text.\n"
    "  -V           : Show version number (datetime).\n"
    "  -c <tdccount>: Number of HPTDCs in JTAG chain (default: 1).\n"
    "  -m <tdcmask> : Mask of HPTDCs to be initialized, in hexadecimal "
    "(default: 0x00001).\n"
    "  -i           : Generate JTAG bits for reading HPTDC ID(s)\n"
    "                 (default: generate for initialization).\n"
    "  -D           : Enable debug mode: display bitstrings verbose.\n"
    "                 (i.e. output not suitable as input for fscajfile)\n"
    "  -x <devs>    : Number of devices preceeding the HPTDCs in JTAG chain.\n"
    "  -X <ibits>   : Total number of preceeding BYPASS instruction bits, or\n"
    "                 (with option -z) the number of instruction bits "
    "per device.\n"
    "  -y <devs>    : Number of devices trailing the HPTDCs in the JTAG chain.\n"
    "  -Y <ibits>   : Total number of trailing BYPASS instruction bits, or\n"
    "                 (with option -z) the number of instruction bits "
    "per device.\n"
    "  -z <instr>   : The BYPASS instruction value for each of the preceeding\n"
    "                 and trailing devices (when unequal to only '1'-bits).\n";
}

// ----------------------------------------------------------------------------

void arg_error( char opt )
{
  cout << "### -" << opt << ": error in argument" << endl;
  usage();
  exit( 0 );
}

// ----------------------------------------------------------------------------

void arg_range( char opt, int min, int max )
{
  cout << "### -" << opt << ": argument not in range ("
       << min << ".." << max << ")" << endl;
  exit( 0 );
}

// ----------------------------------------------------------------------------
