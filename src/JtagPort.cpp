#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <string.h>
#include "JtagPort.h"
#include "crc.h"
using namespace std;

const int JTAG_WR[] = { SCA_JTAG_WR_TDO0, SCA_JTAG_WR_TDO1,
                        SCA_JTAG_WR_TDO2, SCA_JTAG_WR_TDO3 };
const int JTAG_RD[] = { SCA_JTAG_RD_TDI0, SCA_JTAG_RD_TDI1,
                        SCA_JTAG_RD_TDI2, SCA_JTAG_RD_TDI3 };
const int JTAG_WR_TMS[] = { SCA_JTAG_WR_TMS0, SCA_JTAG_WR_TMS1,
                            SCA_JTAG_WR_TMS2, SCA_JTAG_WR_TMS3 };

// To map GBT-SCA bytes LSB to MSB
const int SCA_BYTE_MAP[] = { 2, 3, 0, 1 };

// ----------------------------------------------------------------------------

JtagPort::JtagPort( FlxUpload &fup, FlxReceiver *frecv, int elinknr )
  : _state( TEST_LOGIC_RESET ),
    _progress( false ),
    _elinknr( elinknr ),
    _trid( 1 ),
    _ctrlReg( 0x0000 ),
    _goDelayFactor( 20 ), // Matches 1MHz JTAG clock
    _expReplies( 0 ),
    _recvReplies( 0 ),
    _scaCrcErrors( 0 ),
    _scaCmdErrors( 0 ),
    _scaRejErrors( 0 ),
    _fup( fup ),
    _frecv( frecv ),
    _instr( 0 ),
    _instrLen( 0 ),
    _readTdi( true ),
    _verbose( false ),
    _tdiBits( 0 )
{
  // NB:
  // Control Register bit 11 'MSB/LSB' (0x0800) here (above) initialised to 0;
  // GBT-SCA docu (v8.2-draft) says "bits transmitted msb to lsb" if 0,
  // but the opposite is true (Henk B, 23 Nov 2017)

  _fchecker = new FlxDataChecker;
  _fchecker->setReceiver( frecv );
}

// ----------------------------------------------------------------------------

JtagPort::~JtagPort()
{
}

// ----------------------------------------------------------------------------

void JtagPort::configure( int freq_mhz )
{
  uint8_t data[4];

  // ###NB: Should read and modify the current channel enable mask
  // ...

  // Enable GBT-SCA JTAG channel
  data[0] = 0x00;
  data[1] = 1 << (SCA_DEV_JTAG & 7);
  _fup.uploadScaFrame( _elinknr, &_trid, SCA_DEV_CONFIG, 2,
                       SCA_CONFIG_WR_D, data );

  // Configure GBT-SCA JTAG channel (standard; LEN=128 (represented by value 0)
  _ctrlReg &= ~0x007F;
  mapInt2Bytes( _ctrlReg, data );
  _fup.uploadScaFrame( _elinknr, &_trid, SCA_DEV_JTAG, 4,
                       SCA_JTAG_WR_CTRL, data );
  // Frequency of 20MHz: f=2*10000000/(DIV+1) --> DIV=0
  // (NB: LSByte is in data[2] and MSByte in data[3])
  uint8_t div = 19; // Default 1MHz
  if( freq_mhz == 20 )
    div = 0;
  else if( freq_mhz == 10 )
    div = 1;
  else if( freq_mhz == 5 )
    div = 3;
  else if( freq_mhz == 4 )
    div = 4;
  else if( freq_mhz == 2 )
    div = 9;
  else if( freq_mhz == 1 )
    div = 19;
  else
    freq_mhz = 1;
  _goDelayFactor = 20/freq_mhz;
  memset( data, 0, 4 );
  data[2] = div;
  _fup.uploadScaFrame( _elinknr, &_trid, SCA_DEV_JTAG, 4,
                       SCA_JTAG_WR_FREQ, data );
  usleep( 1000 );
  _expReplies += 3;

  _fup.resetScaFrames();

  // Reset GBT-SCA JTAG TDO and TMS registers
  addResetTdoTms();

  _fup.uploadScaFrames( _elinknr );
}

// ----------------------------------------------------------------------------

void JtagPort::gotoState( state_t s )
{
  uint32_t tms;
  int      nbits = setState( s, &tms );
  //std::cout << "state " << s << ": "
  //          << nbits << " bits, tms=" << tms << std::endl;
  if( nbits <= 0 ) return;

  _fup.resetScaFrames();

  // Configure the number of bits
  uint8_t data[4];
  _ctrlReg &= ~0x007F;
  _ctrlReg |= (nbits & 0x7F);
  mapInt2Bytes( _ctrlReg, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_CTRL, data );

  // Configure TDO and TMS
  memset( data, 0, 4 );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO0, data );
  mapInt2Bytes( tms, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS0, data );

  // Shift the bits
  memset( data, 0, 4 );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 2, SCA_JTAG_GO, data );
  _fup.addScaDelayUs( (_goDelayFactor*8)/4 );// Div by 4: not more than
                                             // 32 bits instead of 128
  _fup.uploadScaFrames( _elinknr );
  _expReplies += 4;
}

// ----------------------------------------------------------------------------

//void JtagPort::shiftIr( int instr, int irlen )
//{
  // SPECIAL function: assume there is a single device in the JTAG chain
//  uint8_t tdo = (uint8_t) (instr & 0xFF);
//  shift( SHIFT_IR, irlen, &tdo );

  // INFO FOR POTENTIAL EXTENSION TO MULTIPLE DEVICES:
  // In case of multiple devices ('numDevices') in chain put 'instr'
  // in selected device 'deviceIndex' and select BYPASS for all other devices;
  // also take that into account in shiftDr():
  // a single bit to add per device in BYPASS.
  //int pre=0; // Number of 1-bits to shift *before* 'instr'
  //for(int dev=deviceIndex+1; dev<numDevices; dev++)
  //  pre+=devices[dev].irlen;  // Calculate number of pre BYPASS bits
  //int post=0; // Number of 1-bits to shift *after* 'instr'
  //for(int dev=0; dev<deviceIndex; dev++)
  //  post+=devices[dev].irlen; // Calculate number of post BYPASS bits
//}

// ----------------------------------------------------------------------------

void JtagPort::shiftIr( int nbits, uint8_t *tdo, bool read_tdi )
{
  shift( SHIFT_IR, nbits, tdo, read_tdi );
}

// ----------------------------------------------------------------------------

void JtagPort::shiftDr( int nbits, uint8_t *tdo, bool read_tdi )
{
  shift( SHIFT_DR, nbits, tdo, read_tdi );
}

// ----------------------------------------------------------------------------

void JtagPort::shiftIr( int irlen,       int instr,
                        int devs_before, int ibits_before,
                        int devs_after,  int ibits_after,
                        int dev_instr,   bool read_tdi )
{
  bool final = false;
  if( devs_after > 0 && ibits_after > 0 )
    {
      // Insert BYPASS instruction for the devices present in the JTAG chain
      // *after* the selected device
      if( dev_instr >= 0 )
        {
          // SPECIAL:
          // assume the BYPASS instruction is 'dev_instr' for each device
          // with instruction length 'ibits_after'
          std::vector<uint8_t> bypass( (devs_after*ibits_after+7)/8, 0x00 );
          int bit = 0;
          for( int dev=devs_after-1; dev>= 0; --dev )
            {
              for( int i=0; i<ibits_after; ++i, ++bit )
                if( dev_instr & (1<<i) )
                  bypass[bit/8] |= (1 << (bit & 0x7));
            }
          shift( SHIFT_IR, devs_after*ibits_after, bypass.data(),
                 false, final );
        }
      else
        {
          // Assume the BYPASS instruction is all 1s
          // independent of individual instruction sizes
          // and 'ibits_after' is the *total* number of instruction bits
          std::vector<uint8_t> bypass( (ibits_after+7)/8, 0xFF );
          shift( SHIFT_IR, ibits_after, bypass.data(), false, final );
        }
    }

  // Shift instruction into the selected device
  final = (devs_before <= 0);
  uint8_t *tdo = (uint8_t *) &instr;
  shift( SHIFT_IR, irlen, tdo, read_tdi, final );

  if( devs_before > 0 && ibits_before > 0 )
    {
      // Insert BYPASS instruction for the devices present in the JTAG chain
      // *before* the selected device
      final = true;
      if( dev_instr >= 0 )
        {
          // SPECIAL:
          // assume the BYPASS instruction is 'dev_instr' for each device
          // with instruction length 'ibits_before'
          std::vector<uint8_t> bypass( (devs_before*ibits_before+7)/8, 0x00 );
          int bit = 0;
          for( int dev=devs_before-1; dev>= 0; --dev )
            {
              for( int i=0; i<ibits_before; ++i, ++bit )
                if( dev_instr & (1<<i) )
                  bypass[bit/8] |= (1 << (bit & 0x7));
            }
          shift( SHIFT_IR, devs_before*ibits_before, bypass.data(),
                 false, final );
        }
      else
        {
          // Assume the BYPASS instruction is all 1s
          // independent of individual instruction sizes
          // and 'ibits_after' is the *total* number of instruction bits
          std::vector<uint8_t> bypass( (ibits_before+7)/8, 0xFF );
          shift( SHIFT_IR, ibits_before, bypass.data(), false, final );
        }
    }
}

// ----------------------------------------------------------------------------

void JtagPort::shiftDr( int nbits, uint8_t *tdo, bool read_tdi,
                        int devs_before, int devs_after )
{
  bool final = false;
  if( devs_after > 0 )
    {
      // BYPASS 1-bit per device for the devices *after* the selected device
      std::vector<uint8_t> bypass( (devs_after+7)/8, 0xFF );
      shift( SHIFT_DR, devs_after, bypass.data(), false, final );
    }

  // Shift data into the selected device(s)
  final = (devs_before <= 0);
  shift( SHIFT_DR, nbits, tdo, read_tdi, final );

  if( devs_before > 0 )
    {
      // BYPASS 1-bit per device for the devices *before* the selected device
      std::vector<uint8_t> bypass( (devs_before+7)/8, 0xFF );
      final = true;
      shift( SHIFT_DR, devs_before, bypass.data(), false, final );
    }
}

// ----------------------------------------------------------------------------

void JtagPort::shift( state_t state, int nbits, uint8_t *tdo,
                      bool read_tdi, bool final )
{
  if( nbits <= 0 ) return;

  //if( state == SHIFT_IR || state == SHIFT_DR )
  gotoState( state );

  _fup.resetScaFrames();

  // Reset GBT-SCA JTAG TDO and TMS registers
  addResetTdoTms();

  uint8_t data[4];
  if( nbits > 128 )
    {
      // Configure the number of bits to shift: 128
      _ctrlReg &= ~0x007F;
      mapInt2Bytes( _ctrlReg, data );
      _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_CTRL, data );
      ++_expReplies;
    }

  _fup.uploadScaFrames( _elinknr );

  int bytes_todo = nbits/8;
  int bytes_done = 0;
  int percentage = 0;
  if( _progress && state == SHIFT_DR )
    std::cout << "Uploading...     ";

  int index = 0;
  while( nbits > 128 )
    {
      _fup.resetScaFrames();

      // Accumulate up to 'max_cnt' groups of commands into a single DMA
      int max_cnt = 100, cnt = 0;
      while( cnt < max_cnt && nbits > 128 )
        {
          if( tdo )
            {
              for( int i=0; i<4; ++i )
                {
                  data[2] = tdo[index++];
                  data[3] = tdo[index++];
                  data[0] = tdo[index++];
                  data[1] = tdo[index++];
                  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4,
                                    JTAG_WR[i], data );
                }
              _expReplies += 4;
            }

          // Shift the bits
          memset( data, 0, 4 );
          _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 2, SCA_JTAG_GO, data );
          _fup.addScaDelayUs( _goDelayFactor*8 );
          ++_expReplies;

          if( read_tdi )
            {
              for( int i=0; i<4; ++i )
                {
                  int id = 254; // Transaction-ID to filter on
                  _fup.addScaFrame( &id, SCA_DEV_JTAG, 0, JTAG_RD[i], data );
                  _fup.addScaDelayUs( 1 ); // Required!
                }
              _expReplies += 4;
            }

          nbits -= 128;
          bytes_done += 16;
          ++cnt;
        }

      _fup.uploadScaFrames( _elinknr );

      if( _progress && state == SHIFT_DR )
        {
          // Display percentage done
          int p = percentage;
          displayPercentage( &percentage, bytes_done, bytes_todo );
          // Prevent replies piling up in the receive buffer,
          // potentially filling it up to overflow:
          // process received data every percent point increase
          if( percentage != p && percentage != 100 )
            getTdi( 0 );
        }
    }

  _fup.resetScaFrames();

  // Final bits to shift (128 bits or less)
  _ctrlReg &= ~0x007F;
  if( nbits < 128 )
    _ctrlReg |= (nbits & 0x7F);
  mapInt2Bytes( _ctrlReg, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_CTRL, data );
  ++_expReplies;

  if( tdo )
    {
      for( int i=0; i<4; ++i )
        if( nbits > i*32 )
          {
            data[2] = tdo[index++];
            data[3] = tdo[index++];
            data[0] = tdo[index++];
            data[1] = tdo[index++];
            _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, JTAG_WR[i], data );
            ++_expReplies;
          }
    }

  // Configure TMS: exit SHIFT_DR or SHIFT_IR state on the final bit;
  // all other states are done with TMS=0 only
  uint32_t tms = 0;
  if( final && (state == SHIFT_IR || state == SHIFT_DR) )
    tms = (1 << ((nbits-1) & 0x1F));
  if( tms != 0 )
    {
      mapInt2Bytes( tms, data );
      if( nbits <= 32 )
        _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS0, data );
      else if( nbits <= 64 )
        _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS1, data );
      else if( nbits <= 96 )
        _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS2, data );
      else
        _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS3, data );
      ++_expReplies;
    }

  // Shift the bits (add delay as required for 128 bits)
  memset( data, 0, 4 );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 2, SCA_JTAG_GO, data );
  _fup.addScaDelayUs( _goDelayFactor*8 );
  ++_expReplies;

  if( read_tdi )
    {
      for( int i=0; i<4; ++i )
        if( nbits > i*32 )
          {
            // Delay of 1 us required !
            int id = 254; // Transaction-ID to filter on
            _fup.addScaFrame( &id, SCA_DEV_JTAG, 0, JTAG_RD[i], data );
            _fup.addScaDelayUs( 1 ); // Required!
            ++_expReplies;
          }
    }

  _fup.uploadScaFrames( _elinknr );

  if( _progress && bytes_todo > 0 && state == SHIFT_DR )
    {
      bytes_done += (nbits+7)/8;
      // Display percentage done
      displayPercentage( &percentage, bytes_done, bytes_todo );
    }

  if( final && (state == SHIFT_IR || state == SHIFT_DR) )
    {
      // We left the SHIFT_DR or SHIFT_IR state
      if( state == SHIFT_DR )
        _state = EXIT1_DR;
      else
        _state = EXIT1_IR;
      // Continue to state RUN_TEST_IDLE
      gotoState( RUN_TEST_IDLE );
    }
}

// ----------------------------------------------------------------------------

// Xilinx VIRTEX7 JTAG instructions
#define V7_CFG_OUT      0x04
#define V7_CFG_IN       0x05
#define V7_IDCODE       0x09
#define V7_JPROGRAM     0x0B
#define V7_JSTART       0x0C
#define V7_JSHUTDOWN    0x0D
#define V7_ISC_ENABLE   0x10
#define V7_ISC_PROGRAM  0x11
#define V7_ISC_DISABLE  0x16
#define V7_BYPASS       0x3F

void JtagPort::shift( int nbits, uint8_t *tms, uint8_t *tdo, uint8_t *tdi )
{
  // This function has been added to support the Xilinx XVC protocol
  // (basically remote low-level JTAG) whose primary command
  // for transmitting data, is, according to the protocol description:
  // "shift:[number of bits][TMS vector][TDI vector]"
  // NB: TDI is the data shifted *out*, which we name *TDO* in this project!
  // At the same time TDI data is read out and stored in 'tdi'.
  //
  // While transmitting TMS+TDO bits we keep track of the current JTAG state,
  // also to be able to *disable* TDI read-back for performance reasons,
  // in particular when a Xilinx FPGA is being configured,
  // when TDI read-back is not significant.

  static const state_t JtagNextState[][2] =
    {
     /*[SELECT_DR_SCAN]   =*/ {CAPTURE_DR, SELECT_IR_SCAN},
     /*[CAPTURE_DR]       =*/ {SHIFT_DR, EXIT1_DR},
     /*[SHIFT_DR]         =*/ {SHIFT_DR, EXIT1_DR},
     /*[EXIT1_DR]         =*/ {PAUSE_DR, UPDATE_DR},
     /*[PAUSE_DR]         =*/ {PAUSE_DR, EXIT2_DR},
     /*[EXIT2_DR]         =*/ {SHIFT_DR, UPDATE_DR},
     /*[UPDATE_DR]        =*/ {RUN_TEST_IDLE, SELECT_DR_SCAN},

     /*[TEST_LOGIC_RESET] =*/ {RUN_TEST_IDLE, TEST_LOGIC_RESET},
     /*[RUN_TEST_IDLE]    =*/ {RUN_TEST_IDLE, SELECT_DR_SCAN},

     /*[SELECT_IR_SCAN]   =*/ {CAPTURE_IR, TEST_LOGIC_RESET},
     /*[CAPTURE_IR]       =*/ {SHIFT_IR, EXIT1_IR},
     /*[SHIFT_IR]         =*/ {SHIFT_IR, EXIT1_IR},
     /*[EXIT1_IR]         =*/ {PAUSE_IR, UPDATE_IR},
     /*[PAUSE_IR]         =*/ {PAUSE_IR, EXIT2_IR},
     /*[EXIT2_IR]         =*/ {SHIFT_IR, UPDATE_IR},
     /*[UPDATE_IR]        =*/ {RUN_TEST_IDLE, SELECT_DR_SCAN}
    };

  if( nbits <= 0 ) return;

  // Keep track of the JTAG state for this sequence of TMS bits,
  // and at the same time extract the JTAG instruction given
  // (NB: *entering* state SHIFT_IR does not carry the first instruction bit,
  //      while *leaving the state carries the last instruction bit,
  //      so in the loop below the *next* state is determined only at the end);
  // configure _readTdi accordingly, i.e. no need to read back TDI
  // when uploading a Xilinx FPGA configuration -> performance!
  int i, bit;
  //int cnt = 0;
  for( i=0; i<nbits; ++i )
    {
      if( _state == CAPTURE_IR )
        {
          // Going to receive an instruction: initialize
          _instr    = 0x00000000;
          _instrLen = 0;
        }
      else if( _state == SHIFT_IR )
        {
          // Extract and add an instruction bit
          bit = ((tdo[i/8] & (1 << (i&7))) != 0) ? 1 : 0;
          if( bit == 1 )
            _instr |= (1 << _instrLen);
          ++_instrLen;
        }
      else if( _state == UPDATE_IR )
        {
          if( _verbose )
            cout << "Instr=" << _instr << ", len=" << _instrLen << endl;

          if( _instr == V7_CFG_IN )
            {
              // Going to execute JTAG instruction '5': disable TDI reading
              _readTdi = false;
              if( _verbose )
                cout << "TDI -> Off\n";
            }
        }
      else if( _state == UPDATE_DR )
        {
          if( _instr == V7_CFG_IN )
            {
              // Leaving state in which JTAG instruction '5' was executed:
              // re-enable TDI reading
              _readTdi = true;
              if( _verbose )
                cout << "TDI -> On\n";
            }
        }

      // Next JTAG state
      bit = ((tms[i/8] & (1 << (i&7))) != 0) ? 1 : 0;

      // DEBUG: display state transitions
      /*
      if( i == 0 )
        {
          std::cout << "States: " << JtagNextState[_state][bit];
          cnt = 1;
        }
      else
        {
          if( JtagNextState[_state][bit] != _state )
            {
              if( cnt > 1 )
                // Previous state occurred multiple times sequentially: display
                std::cout << "(" << std::dec << cnt << ") " << std::hex;
              else
                std::cout << " ";
              std::cout << JtagNextState[_state][bit];
              cnt = 1;
            }
          else
            {
              // Keep track of a state that is maintained multiple clock ticks
              // without displaying each individually: keep count
              ++cnt;
            }
        }
        */
      _state = JtagNextState[_state][bit];
    }
  /*
  // DEBUG: display state transitions
  if( cnt > 1 )
    // Current state occurred multiple times sequentially
    std::cout << "(" << std::dec << cnt << ") " << std::hex;
  std::cout << std::endl;
  */

  this->clearTdi();

  _fup.resetScaFrames();

  uint8_t data[4];
  if( nbits > 128 )
    {
      // Configure the number of bits to shift: 128
      _ctrlReg &= ~0x007F;
      mapInt2Bytes( _ctrlReg, data );
      _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_CTRL, data );
      ++_expReplies;
    }

  _fup.uploadScaFrames( _elinknr );

  uint32_t index = 0;
  while( nbits > 128 )
    {
      _fup.resetScaFrames();

      // Accumulate up to 'max_cnt' groups of commands into a single DMA
      int max_cnt = 100, cnt = 0;
      while( cnt < max_cnt && nbits > 128 )
        {
          for( int i=0; i<4; ++i )
            {
              data[2] = tms[index+0];
              data[3] = tms[index+1];
              data[0] = tms[index+2];
              data[1] = tms[index+3];
              _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, JTAG_WR_TMS[i], data );

              data[2] = tdo[index+0];
              data[3] = tdo[index+1];
              data[0] = tdo[index+2];
              data[1] = tdo[index+3];
              _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, JTAG_WR[i], data );

              index += 4;
            }
          _expReplies += 8;

          // Shift the bits (add delay as required for 128 bits)
          memset( data, 0, 4 );
          _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 2, SCA_JTAG_GO, data );
          _fup.addScaDelayUs( _goDelayFactor*8 );
          ++_expReplies;

          if( _readTdi )
            {
              for( int i=0; i<4; ++i )
                {
                  // Delay of 1 us required !
                  int id = 254; // Transaction-ID to filter on
                  _fup.addScaFrame( &id, SCA_DEV_JTAG, 0, JTAG_RD[i], data );
                  _fup.addScaDelayUs( 1 ); // Required!
                }
              _expReplies += 4;
            }

          nbits -= 128;
          ++cnt;
        }

      _fup.uploadScaFrames( _elinknr );
    }

  _fup.resetScaFrames();

  // Final bits to shift (128 bits or less)
  _ctrlReg &= ~0x007F;
  if( nbits < 128 )
    _ctrlReg |= (nbits & 0x7F);
  mapInt2Bytes( _ctrlReg, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_CTRL, data );
  ++_expReplies;

  for( int i=0; i<4; ++i )
    if( nbits > i*32 )
      {
        data[2] = tms[index + 0];
        data[3] = tms[index + 1];
        data[0] = tms[index + 2];
        data[1] = tms[index + 3];
        _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, JTAG_WR_TMS[i], data );

        data[2] = tdo[index + 0];
        data[3] = tdo[index + 1];
        data[0] = tdo[index + 2];
        data[1] = tdo[index + 3];
        _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, JTAG_WR[i], data );

        index += 4;
        _expReplies += 2;
      }

  // Shift the bits (add delay as required for 128 bits)
  memset( data, 0, 4 );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 2, SCA_JTAG_GO, data );
  _fup.addScaDelayUs( _goDelayFactor*8 );
  ++_expReplies;

  if( _readTdi )
    {
      for( int i=0; i<4; ++i )
        if( nbits > i*32 )
          {
            // Delay of 1 us required !
            int id = 254; // Transaction-ID to filter on
            _fup.addScaFrame( &id, SCA_DEV_JTAG, 0, JTAG_RD[i], data );
            _fup.addScaDelayUs( 1 ); // Required!
            ++_expReplies;
          }
    }

  _fup.uploadScaFrames( _elinknr );

  // Get the requested TDI bits
  //nbits = 0;
  if( _readTdi )
    nbits = getTdi( 0, 2000 );
  else
    nbits = getTdi( 0 );
  if( nbits > 0 )
    memcpy( (void *) tdi, (void *) _tdiData.data(), (nbits + 7)/8 );
}

// ----------------------------------------------------------------------------

//void JtagPort::shift( int ncycles, bool read_tdi )
//{
  // Generate 'ncycles' clock cycles in the state we're in
  // (i.e. most likely: RUN_TEST_IDLE).
  // Function shift() intentionally changes the state
  // for SHIFT_IR and SHIFT_DR only!
//  shift( RUN_TEST_IDLE, ncycles, 0, read_tdi );
//}

// ----------------------------------------------------------------------------

int JtagPort::setState( state_t s, uint32_t *tms )
{
  // Do nothing for invalid state
  if( s > UPDATE_IR )
    return 0;

  int nbits = 0;
  *tms = 0x0;

  // Reset (from any state in at most 5 clock cycles with TMS=1)
  if( s == TEST_LOGIC_RESET )
    {
      *tms = 0x1F;
      _state = TEST_LOGIC_RESET;
      nbits = 5;
    }

  // Go to the requested TAP state step by step,
  // using the shortest possible path in the state diagram
  while( _state != s )
    {
      if( _state == TEST_LOGIC_RESET && _state != s )
        {
          // TMS=0
          ++nbits; 
          _state = RUN_TEST_IDLE;
        }
      if( _state == RUN_TEST_IDLE && _state != s )
        {
          *tms |= (1 << nbits); // TMS=1
          ++nbits; 
          _state = SELECT_DR_SCAN;
        }
      if( _state == SELECT_DR_SCAN && _state != s )
        {
          if( s < TEST_LOGIC_RESET )
            {
              // TMS=0
              _state = CAPTURE_DR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = SELECT_IR_SCAN;
            }
          ++nbits;
        }
      if( _state == CAPTURE_DR && _state != s )
        {
          if( s == SHIFT_DR )
            {
              // TMS=0
              _state = SHIFT_DR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = EXIT1_DR;
            }
          ++nbits;
        }
      if( _state == SHIFT_DR && _state != s )
        {
          *tms |= (1 << nbits); // TMS=1
          ++nbits;
          _state = EXIT1_DR;
        }
      if( _state == EXIT1_DR && _state != s )
        {
          if( s == PAUSE_DR ||
              s == EXIT2_DR )
            {
              // TMS=0
              _state = PAUSE_DR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = UPDATE_DR;
            }
          ++nbits;
        }
      if( _state == PAUSE_DR && _state != s )
        {
          *tms |= (1 << nbits); // TMS=1
          ++nbits;
          _state = EXIT2_DR;
        }
      if( _state == EXIT2_DR && _state != s )
        {
          if( s == SHIFT_DR ||
              s == EXIT1_DR ||
              s == PAUSE_DR )
            {
              // TMS=0
              _state = SHIFT_DR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = UPDATE_DR;
            }
          ++nbits;
        }
      if( _state == UPDATE_DR && _state != s )
        {
          if( s == RUN_TEST_IDLE )
            {
              // TMS=0
              _state = RUN_TEST_IDLE;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = SELECT_DR_SCAN;
            }
          ++nbits;
        }
      if( _state == SELECT_IR_SCAN && _state != s )
        {
          if( s != TEST_LOGIC_RESET )
            {
              // TMS=0
              _state = CAPTURE_IR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = TEST_LOGIC_RESET;
            }
          ++nbits;
        }
      if( _state == CAPTURE_IR && _state != s )
        {
          if( s == SHIFT_IR )
            {
              // TMS=0
              _state = SHIFT_IR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = EXIT1_IR;
            }
          ++nbits;
        }
      if( _state == SHIFT_IR && _state != s )
        {
          *tms |= (1 << nbits); // TMS=1
          ++nbits;
          _state = EXIT1_IR;
        }
      if( _state == EXIT1_IR && _state != s )
        {
          if( s == PAUSE_IR ||
              s == EXIT2_IR )
            {
              // TMS=0
              _state = PAUSE_IR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = UPDATE_IR;
            }
          ++nbits;
        }
      if( _state == PAUSE_IR && _state != s )
        {
          *tms |= (1 << nbits); // TMS=1
          ++nbits; 
          _state = EXIT2_IR;
        }
      if( _state == EXIT2_IR && _state != s )
        {
          if( s == SHIFT_IR  ||
              s == EXIT1_IR  ||
              s == PAUSE_IR )
            {
              // TMS=0
              _state = SHIFT_IR;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = UPDATE_IR;
            }
          ++nbits;
        }
      if( _state == UPDATE_IR && _state != s )
        {
          if( s == RUN_TEST_IDLE )
            {
              // TMS=0
              _state = RUN_TEST_IDLE;
            }
          else
            {
              *tms |= (1 << nbits); // TMS=1
              _state = SELECT_DR_SCAN;
            }
          ++nbits;
        }
    }
  return nbits;
}

// ----------------------------------------------------------------------------
/*
void JtagPort::nextState( int tms )
{
  switch( _state )
    {
    case SELECT_DR_SCAN:
      if( tms & 1 )
        _state = SELECT_IR_SCAN;
      else
        _state = CAPTURE_DR;
      break;
    case CAPTURE_DR:
      if( tms & 1 )
        _state = EXIT1_DR;
      else
        _state = SHIFT_DR;
      break;
    case SHIFT_DR:
      if( tms & 1 )
        _state = EXIT1_DR;
      else
        _state = SHIFT_DR;
      break;
    case EXIT1_DR:
      if( tms & 1 )
        _state = UPDATE_DR;
      else
        _state = PAUSE_DR;
      break;
    case PAUSE_DR:
      if( tms & 1 )
        _state = EXIT2_DR;
      else
        _state = PAUSE_DR;
      break;
    case EXIT2_DR:
      if( tms & 1 )
        _state = UPDATE_DR;
      else
        _state = SHIFT_DR;
      break;
    case UPDATE_DR:
      if( tms & 1 )
        _state = SELECT_DR_SCAN;
      else
        _state = RUN_TEST_IDLE;
      break;

    case TEST_LOGIC_RESET:
      if( tms & 1 )
        _state = TEST_LOGIC_RESET;
      else
        _state = RUN_TEST_IDLE;
      break;
    case RUN_TEST_IDLE:
      if( tms & 1 )
        _state = SELECT_DR_SCAN;
      else
        _state = RUN_TEST_IDLE;
      break;

    case SELECT_IR_SCAN:
      if( tms & 1 )
        _state = TEST_LOGIC_RESET;
      else
        _state = CAPTURE_IR;
      break;
    case CAPTURE_IR:
      if( tms & 1 )
        _state = EXIT1_IR;
      else
        _state = SHIFT_IR;
      break;
    case SHIFT_IR:
      if( tms & 1 )
        _state = EXIT1_IR;
      else
        _state = SHIFT_IR;
      break;
    case EXIT1_IR:
      if( tms & 1 )
        _state = UPDATE_IR;
      else
        _state = PAUSE_IR;
      break;
    case PAUSE_IR:
      if( tms & 1 )
        _state = EXIT2_IR;
      else
        _state = PAUSE_IR;
      break;
    case EXIT2_IR:
      if( tms & 1 )
        _state = UPDATE_IR;
      else
        _state = SHIFT_IR;
      break;
    case UPDATE_IR:
      if( tms & 1 )
        _state = SELECT_DR_SCAN;
      else
        _state = RUN_TEST_IDLE;
      break;
    default:
      break;
    }
}
*/
// ----------------------------------------------------------------------------

void JtagPort::addResetTdoTms()
{
  // Reset GBT-SCA JTAG TDO and TMS registers
  uint8_t data[4] = { 0, 0, 0, 0 };
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO0, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO1, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO2, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TDO3, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS0, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS1, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS2, data );
  _fup.addScaFrame( &_trid, SCA_DEV_JTAG, 4, SCA_JTAG_WR_TMS3, data );
  _expReplies += 8;
}

// ----------------------------------------------------------------------------

void JtagPort::mapInt2Bytes( uint32_t i, uint8_t *bytes )
{
  bytes[2] = (uint8_t) ((i >>  0) & 0xFF);
  bytes[3] = (uint8_t) ((i >>  8) & 0xFF);
  bytes[0] = (uint8_t) ((i >> 16) & 0xFF);
  bytes[1] = (uint8_t) ((i >> 24) & 0xFF);
}

// ----------------------------------------------------------------------------

void JtagPort::displayPercentage( int *percentage,
                                  int  bytes_done,
                                  int  bytes_todo )
{
  // Display percentage done (on change of integer value)
  int p = *percentage;
  double pf = ((double) 100.0*bytes_done)/(double) bytes_todo;
  if( (int) pf != p )
    {
      p = (int) pf;
      // Overwrite currently displayed percentage value
      std::cout << "\b\b\b\b\b    \b\b\b\b" << std::setw(3) << p << "% ";
      std::cout.flush();
      *percentage = p;
    }
}

// ----------------------------------------------------------------------------

int JtagPort::getTdi( uint8_t **data, int timeout_us )
{
  uint8_t *chunkdata = 0;
  uint32_t size;
  _fchecker->startRecvTime(); // Reset time-out on receiving data non-stop
  while( _fchecker->nextChunkRecvd( &chunkdata, &size, timeout_us, _elinknr ) )
    {
      if( size == 0 ) continue;
      ++_recvReplies;
      if( size != 12 ) continue;

      // Extract chunks with SCA replies containing
      // our dedicated Transaction IDs

      // GBT-SCA reply parameters
      uint32_t ctrl = (uint32_t) chunkdata[1];
      uint32_t trid = (uint32_t) chunkdata[2];
      uint32_t chan = (uint32_t) chunkdata[3];
      uint32_t err  = (uint32_t) chunkdata[4];
      //uint32_t len  = (uint32_t) chunkdata[5];

      // Check the reply for various errors
      if( err != 0 )
        ++_scaCmdErrors;
      if( ctrl & HDLC_CTRL_SFORMAT )
        {
          uint32_t type = ctrl & HDLC_CTRL_TYPE_MASK;
          if( type == HDLC_CTRL_REJECT || type == HDLC_CTRL_SELREJECT )
            ++_scaRejErrors;
        }
      if( crc16( chunkdata, size ) != (uint16_t) 0 )
        ++_scaCrcErrors;

      // Extract requested returned TDI data
      if( trid == 0xFE && chan == SCA_DEV_JTAG )
        {
          // Store the data
          uint8_t *data = &chunkdata[6];
          _tdiData.push_back( data[SCA_BYTE_MAP[0]] );
          _tdiData.push_back( data[SCA_BYTE_MAP[1]] );
          _tdiData.push_back( data[SCA_BYTE_MAP[2]] );
          _tdiData.push_back( data[SCA_BYTE_MAP[3]] );
          _tdiBits += 32;
        }
    }

  if( data != 0 )
    *data = _tdiData.data();
  return _tdiBits;
}

// ----------------------------------------------------------------------------

void JtagPort::clearTdi()
{
  _tdiBits = 0;
  _tdiData.clear();
}

// ----------------------------------------------------------------------------

void JtagPort::displayReplies( std::string title )
{
  // Optionally have a title for the GBT-SCA replies displayed
  cout << endl;
  if( title.length() )
    cout << ">> " << title << " <<" << endl;

  // Go through the chunks of each block (skipping any TIMEOUT/NULL/OOB chunks)
  // displaying the data bytes, interpreting chunks as GBT-SCA replies,
  // counting the number of chunks
  FlxDataChecker flxChecker;
  int  timeout_us  = 4000;
  bool display_sca = true;
  int  chunks      = 0;
  while( _frecv->hasData() )
    chunks += flxChecker.displayDataRecvd( _frecv, timeout_us,
                                           _elinknr, display_sca );

  _recvReplies += chunks;
}

// ----------------------------------------------------------------------------
