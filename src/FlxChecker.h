#ifndef FLXCHECKER_H
#define FLXCHECKER_H

#include <cstdint>
#include <string>

#include "flxdefs.h"
#include "FlxParser.h"

class FlxChecker: public FlxParser
{
public:
  FlxChecker();

  void reset            ( );
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

  uint64_t truncCount   ( ) { uint64_t e=_truncCount; _truncCount=0; return e; }
  uint64_t errCount     ( ) { uint64_t e=_errCount; _errCount=0; return e; }
  uint64_t crcerrCount  ( ) { uint64_t e=_crcerrCount; _crcerrCount=0; return e; }

  uint64_t blocksChecked( ) { return _blocksChecked; }

private:
  int  maxL1id[FLX_MAX_ELINK_NR+1];
  bool firstL1id[FLX_MAX_ELINK_NR+1];
  int  prevBlockL1id[FLX_MAX_ELINK_NR+1];
  int  eLinkWidths[FLX_MAX_ELINK_NR+1];
  long eLinkBlocks[FLX_MAX_ELINK_NR+1];
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

  uint64_t _blocksChecked;
  int      errorIndex;
  long     _undefBlocks;

  uint64_t _truncCount;
  uint64_t _errCount;
  uint64_t _crcerrCount;
};

#endif // FLXCHECKER_H
