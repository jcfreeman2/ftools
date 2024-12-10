#ifndef JTAGPORT_H
#define JTAGPORT_H

#include <vector>
#include <cstdint>

#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"

class JtagPort
{
 public:

  // States of the JTAG statemachine
  typedef enum {
    SELECT_DR_SCAN = 0,
    CAPTURE_DR,
    SHIFT_DR,
    EXIT1_DR,
    PAUSE_DR,
    EXIT2_DR,
    UPDATE_DR,

    TEST_LOGIC_RESET,
    RUN_TEST_IDLE,

    SELECT_IR_SCAN,
    CAPTURE_IR,
    SHIFT_IR,
    EXIT1_IR,
    PAUSE_IR,
    EXIT2_IR,
    UPDATE_IR
  } state_t;

  JtagPort( FlxUpload &fup, FlxReceiver *frecv, int elinknr );
  ~JtagPort();

 public:
  void    configure     ( int freq_mhz );
  void    gotoState     ( state_t s );
  state_t state         ( )         { return _state; }

  void    shiftIr       ( int nbits, uint8_t *tdo,
                          bool read_tdi = false );
  void    shiftIr       ( int irlen, int instr,
                          int devs_before, int ibits_before,
                          int devs_after,  int ibits_after,
                          int dev_instr = -1,
                          bool read_tdi = false );
  void    shiftDr       ( int nbits, uint8_t *tdo,
                          bool read_tdi = false );
  void    shiftDr       ( int nbits, uint8_t *tdo, bool read_tdi,
                          int devs_before, int devs_after );
  void    shift         ( state_t state, int nbits,
                          uint8_t *tdo  = 0,
                          bool read_tdi = false,
                          bool final    = true ); // Applies to SHIFT_IR/DR only
  void    shift         ( int nbits,
                          uint8_t *tms, uint8_t *tdo, uint8_t *tdi );

  void    resetTrid     ( )         { _trid = 1; }
  void    showProgress  ( bool b )  { _progress = b; }

  int     getTdi        ( uint8_t **tdi, int timeout_us = 0 );
  void    clearTdi      ( );
  void    displayReplies( std::string title = std::string() );

  int     repliesExpected() { int r=_expReplies; _expReplies=0; return r; }
  int     repliesReceived() { int r=_recvReplies; _recvReplies=0; return r; }

  int     scaCrcErrors  ( ) { int e=_scaCrcErrors; _scaCrcErrors=0; return e; }
  int     scaCmdErrors  ( ) { int e=_scaCmdErrors; _scaCmdErrors=0; return e; }
  int     scaRejErrors  ( ) { int e=_scaRejErrors; _scaRejErrors=0; return e; }

  void    setVerbose    ( bool b ) { _verbose = b; }

 private:
  int     setState      ( state_t s, uint32_t *tms );

  void    addResetTdoTms( );

  void    mapInt2Bytes  ( uint32_t i, uint8_t *bytes );

  void    displayPercentage( int *percentage,
                             int  bytes_done,
                             int  bytes_todo );
 private:
  // For JTAG
  state_t         _state;
  bool            _progress; // Whether or not to display progress in shiftDr()

  // For GBT-SCA via FLX
  int             _elinknr;
  int             _trid;
  uint32_t        _ctrlReg;
  int             _goDelayFactor;
  int             _expReplies, _recvReplies;
  int             _scaCrcErrors, _scaCmdErrors, _scaRejErrors;
  FlxUpload      &_fup;
  FlxReceiver    *_frecv;
  FlxDataChecker *_fchecker;

  // Used inside function shift(int, uint8_t*, uint8_t*)
  int             _instr, _instrLen;
  bool            _readTdi;
  bool            _verbose;

  // Storage for received JTAG data bits
  int _tdiBits;
  std::vector<uint8_t> _tdiData;
};

#endif // JTAGPORT_H
