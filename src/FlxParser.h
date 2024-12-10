#ifndef FLXPARSER_H
#define FLXPARSER_H

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

#include "flxdefs.h"
class FlxReceiver;

class FlxParser
{
public:
  FlxParser();

  void setReceiver      ( FlxReceiver *r );
  void startRecvTime    ( ) {_startRecvTime = std::chrono::system_clock::now();}

  int  displayBlock     ( uint8_t *block,
                          int  blocknr        = -1,
                          int  nr_of_bytes    = 0,
                          int  start_offset   = 0,
                          int  elink_filter   = -1,
                          int  word_size      = 1 );
  int  displayChunks    ( uint8_t *block,
                          int  blocknr        = -1,
                          bool display_size   = true,
                          bool display_timeout = true,
                          bool display_timeout_chunk = true,
                          bool display_sca    = false,
                          bool chunkdata_only = false,
                          int  elink_filter   = -1,
                          int  word_size      = 1,
                          bool is_sca_cmd     = false );
  int  displayDataChunks( uint8_t *block,
                          int  blocknr        = -1,
                          bool display_sca    = false,
                          int  elink_filter   = -1,
                          int  word_size      = 1,
                          bool is_sca_cmd     = false );
  void displayDataWords ( uint8_t *bytes, uint32_t size, int word_size,
                          bool display_index = false,
                          uint32_t initial_index = 0 );
  void displayScaFrame  ( uint8_t *sca_frame,
                          uint32_t nbytes,
                          bool     is_cmd     = false,
                          bool     standalone = false );
  void displayTtcFrame  ( uint8_t *ttc_frame );
  void displayIcFrame   ( uint8_t *ic_frame );

  void setBlockSize     ( int size ) {
    _blockSize     = size;
    _blockSizeCode = (_blockSize/1024-1) << 8;
  }
  void setTrailerIs32Bit( bool b )   { _trailerFmt.setTrailerIs32Bit( b ); }

  int  displayDataRecvd ( FlxReceiver *recvr,
                          int          timeout_us,
                          int          elink_filter  = -1,
                          bool         display_sca   = false,
                          int          expected_trid = -1,
                          bool         is_sca_cmd    = false );
  bool nextChunkRecvd   ( uint8_t **chunkdata,
                          uint32_t *size,
                          int       timeout_us   = 0,
                          int       elink_filter = -1,
                          int      *elinknr      = 0,
                          int      *errbits      = 0 );

  void openOutputFile   ( std::string filename );
  void closeOutputFile  (  );
  int  writeDataChunks  ( uint8_t *block,
                          int  blocknr       = -1,
                          int  elink_filter  = -1 );

  int  blockElinkNr     ( uint8_t *block );
  bool blockIsPhase2    ( uint8_t *block );
  uint64_t blocksHandled( ) { return _blocksHandled; }

  // Mapping for EC, IC, AUX (for RM5)
  int mapElink( int elinknr );

  // Parsing data blocks for chunks
  bool setBlockToHandle( uint8_t *block,
                         int      elink_filter = -1 );
  bool nextChunkFromBlock( uint8_t **chunkdata,
                           uint32_t *size,
                           int      *errbits = 0,
                           int      *elinknr = 0 );

  // Other
  int scaCrcErrors( ) { int e=_scaCrcErrors; _scaCrcErrors=0; return e; }
  int scaCmdErrors( ) { int e=_scaCmdErrors; _scaCmdErrors=0; return e; }

protected:
  // Receiving data
  FlxReceiver *_flxReceiver;
  int          _blockSize, _blockSizeCode;
  uint64_t     _blocksToHandle;
  uint64_t     _blocksHandled;
  std::chrono::system_clock::time_point _startRecvTime;
  FlxTrailerFormat _trailerFmt;

private:
  uint8_t *_block; // Remember the pointer to the current 1K data block
  int      _blockElinknr;
  std::vector<chunk_desc_t> _chunkInfo;
  int _chunkIndex;

  // Temporary storage for FIRST+MIDDLE subchunks for each individual E-link
  std::vector<uint8_t> _chunk[FLX_MAX_ELINK_NR+1];
  int                  _errBits[FLX_MAX_ELINK_NR+1];

  // Writing (binary) data
  FILE *_fpOut;

  // GBT-SCA stuff
  int  _scaCrcErrors;
  int  _scaCmdErrors;
};

#endif // FLXPARSER_H
