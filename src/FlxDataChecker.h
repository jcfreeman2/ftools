#ifndef FLXDATACHECKER_H
#define FLXDATACHECKER_H

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

#include "flxdefs.h"
class FlxReceiver;

class FlxDataChecker
{
 public:
  FlxDataChecker();

  void setReceiver      ( FlxReceiver *r );
  void startRecvTime    ( ) {_startRecvTime = std::chrono::system_clock::now();}

  void checkBlocks      ( uint64_t startAddress,
                          uint64_t numberOfBytes,
                          int      check_level,
                          bool     dump_on_error,
                          int      elink_filter   = -1,
                          int      blocknr_offs   = 0,
                          int      blocks_to_skip = 0,
                          bool     report_te      = true,
                          bool     report_crc     = true );
  bool checkChunks      ( uint8_t *block, int blocknr,
                          int elinknr, bool not_first_block,
                          int check_level, bool report_te, bool report_crc,
                          bool phase2 );
  bool checkChunkData   ( uint8_t *chunk, uint32_t length,
                          uint32_t type, int elinknr, int blocknr,
                          int index, bool first_chunk_check,
                          int *l1id_next, bool phase2 );
  bool checkTtcChunkData( uint8_t *chunk, uint32_t length,
                          uint32_t type, int elinknr, int blocknr,
                          int index, bool first_chunk_check,
                          int *l1id_next );
  void displaySummary   ( );

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

  uint64_t truncCount   ( ) { uint64_t e=_truncCount; _truncCount=0; return e; }
  uint64_t errCount     ( ) { uint64_t e=_errCount; _errCount=0; return e; }
  uint64_t crcerrCount  ( ) { uint64_t e=_crcerrCount; _crcerrCount=0; return e; }

  int scaCrcErrors      ( ) { int e=_scaCrcErrors; _scaCrcErrors=0; return e; }
  int scaCmdErrors      ( ) { int e=_scaCmdErrors; _scaCmdErrors=0; return e; }

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

  int  maxL1id[FLX_MAX_ELINK_NR+1];
  bool firstL1id[FLX_MAX_ELINK_NR+1];
  int  prevBlockL1id[FLX_MAX_ELINK_NR+1];
  int  eLinkWidths[FLX_MAX_ELINK_NR+1];
  long eLinkBlocks[FLX_MAX_ELINK_NR+1];
  long _undefBlocks;
  long totalSubchunkBytes[FLX_MAX_ELINK_NR+1];
  int  seqNrExpected[FLX_MAX_ELINK_NR+1];
  bool seqNrFirst[FLX_MAX_ELINK_NR+1];
  long numberOfChunks[FLX_MAX_ELINK_NR+1];
  long subchunksPerType[FLX_SUBCHNK_TYPES][FLX_MAX_ELINK_NR+1];
  long maxChunkLength[FLX_MAX_ELINK_NR+1];
  long minChunkLength[FLX_MAX_ELINK_NR+1];
  int  runningLengthSum[FLX_MAX_ELINK_NR+1][2];
  int  firstTypeCounter[FLX_MAX_ELINK_NR+1];
  bool _isPhase2[FLX_MAX_ELINK_NR+1];
  int  errorIndex;

  uint64_t _truncCount, _errCount, _crcerrCount;

  // GBT-SCA stuff
  int  _scaCrcErrors;
  int  _scaCmdErrors;

  // Parsing data blocks for chunks
  bool setBlockToHandle( uint8_t *block,
                         int      elink_filter = -1 );
  bool nextChunkFromBlock( uint8_t **chunkdata,
                           uint32_t *size,
                           int      *errbits = 0,
                           int      *elinknr = 0 );
  uint8_t *_block; // Remember the pointer to the current 1K data block
  int      _blockElinknr;
  std::vector<chunk_desc_t> _chunkInfo;
  int _chunkIndex;

  // Temporary storage for FIRST+MIDDLE subchunks for each individual E-link
  std::vector<uint8_t> _chunk[FLX_MAX_ELINK_NR+1];
  int                  _errBits[FLX_MAX_ELINK_NR+1];

  // Receiving data
  FlxReceiver *_flxReceiver;
  int          _blockSize, _blockSizeCode;
  uint64_t     _blocksToHandle;
  uint64_t     _blocksHandled;
  std::chrono::system_clock::time_point _startRecvTime;
  FlxTrailerFormat _trailerFmt;

  // Writing (binary) data
  FILE *_fpOut;
};

#endif // FLXDATACHECKER_H
