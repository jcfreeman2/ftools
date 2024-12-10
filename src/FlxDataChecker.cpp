#include <cstdio>
#include <cstdlib>
#include <unistd.h> // For usleep()
#include <sstream> // For ostringstream
#include <iomanip>

#include "FlxDataChecker.h"
#include "FlxReceiver.h"
#include "crc.h"

// ----------------------------------------------------------------------------

FlxDataChecker::FlxDataChecker()
{
  int i;
  for( i=0; i<=FLX_MAX_ELINK_NR; ++i )
    {
      maxL1id[i]                  = 0;
      firstL1id[i]                = true;
      prevBlockL1id[i]            = -1;
      eLinkWidths[i]              = 0;
      eLinkBlocks[i]              = 0;
      totalSubchunkBytes[i]       = 0;
      for( int j=0; j<FLX_SUBCHNK_TYPES; ++j )
        subchunksPerType[j][i] = 0;
      // seqNrExpected[] is incremented for each block,
      // and is 0 for the first block:
      seqNrExpected[i]            = -1;
      seqNrFirst[i]               = true;
      numberOfChunks[i]           = 0;
      maxChunkLength[i]           = 0;
      minChunkLength[i]           = 0;
      runningLengthSum[i][0]      = 0;
      runningLengthSum[i][1]      = 0;
      firstTypeCounter[i]         = 0;
      _isPhase2[i]                = false;
      _errBits[i]                 = 0;
    }
  _blockSize      = 1024;
  _blockSizeCode  = (_blockSize/1024-1) << 8;
  errorIndex      = -1;
  _undefBlocks    = 0;

  _truncCount     = 0;
  _errCount       = 0;
  _crcerrCount    = 0;

  _scaCrcErrors   = 0;
  _scaCmdErrors   = 0;

  _block          = 0;
  _blockElinknr   = -1;
  _chunkIndex     = -1;
  _flxReceiver    = 0;
  _blocksToHandle = 0;
  _blocksHandled  = 0;
  _fpOut          = 0;
}

// ----------------------------------------------------------------------------

void FlxDataChecker::setReceiver( FlxReceiver *r )
{
  _flxReceiver = r;

  if( _flxReceiver )
    {
      _blockSize = _flxReceiver->blockSize();
      _blockSizeCode = (_blockSize/1024-1) << 8;
      _trailerFmt.setTrailerIs32Bit( _flxReceiver->trailerIs32Bit() );
    }

  startRecvTime();
}

// ----------------------------------------------------------------------------

void FlxDataChecker::checkBlocks( uint64_t startAddress,
                                  uint64_t numberOfBytes,
                                  int  check_level,
                                  bool dump_on_error, int elink_filter,
                                  int  blocknr_offs, int blocks_to_skip,
                                  bool report_te, bool report_crc )
{
  u_short *block1k_s = (u_short *) startAddress;
  uint8_t *block1k   = (uint8_t *) startAddress;
  int      eLinkNr;
  int      seqNumber;
  uint8_t *blockWithError = 0;
  bool     errorFlag = false;
  int      errorCount = 0;
  bool     not_first_block;
  int      blocks_skipped = 0;

  elink_filter = mapElink( elink_filter );

  int blocknr;
  for( blocknr=blocknr_offs;
       blocknr<blocknr_offs + (int)(numberOfBytes/_blockSize); ++blocknr )
    {
      if( blocks_skipped < blocks_to_skip )
        {
          ++blocks_skipped;
          block1k_s = block1k_s + _blockSize/2;
          block1k   = block1k   + _blockSize;
          continue;
        }

      if ( !(block1k_s[1] == BLOCK_ID ||
             block1k_s[1] == _trailerFmt.blockId ||
             block1k_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode)) )
        // || block1k_s[_blockSize/2-1]==0 )//End-of-block can be NULL trailer!
        {
          ++_undefBlocks;
          eLinkNr   = -1;
          seqNumber = -1;
          //continue;
        }
      else
        {
          eLinkNr = block1k_s[0] & BLOCK_ELINK_MASK;
          ++eLinkBlocks[eLinkNr];
          seqNumber = (block1k_s[0] & BLOCK_SEQNR_MASK) >> BLOCK_SEQNR_SHIFT;
          seqNrExpected[eLinkNr] = (seqNrExpected[eLinkNr] + 1) & BLOCK_SEQNR_MAX;
          if( (elink_filter >= 0 && eLinkNr != elink_filter) ||
              errorCount > 1000 )
            {
              block1k_s = block1k_s + _blockSize/2;
              block1k   = block1k   + _blockSize;
              continue;
            }

          if( block1k_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode) )
            _isPhase2[eLinkNr] = true;
        }

      if( check_level == 0 )
        {
          if ( !(block1k_s[1] == BLOCK_ID ||
                 block1k_s[1] == _trailerFmt.blockId ||
                 block1k_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode)) )
            // || block1k_s[_blockSize/2-1]==0 )//End-of-block can be NULL trailer!
            {
              printf("ERROR ");
              errorFlag = true;
              if (blockWithError == 0) blockWithError= block1k;

              // Skip further checks: next block...
              block1k_s = block1k_s + _blockSize/2;
              block1k   = block1k   + _blockSize;
              ++_blocksHandled;
              continue;
            }
          else
            {
              printf("----> ");
            }

          printf("Blk %4d header: %02X %02X %02X %02X, trailer: ", blocknr,
                 block1k[0], block1k[1], block1k[2], block1k[3]);
          for( uint32_t i=0; i<_trailerFmt.nbytes; ++i )
            printf( "%02X ", block1k[_blockSize-_trailerFmt.nbytes+i] );
          printf("eLinkNr 0x%03X, seqNumber %3d\n", eLinkNr, seqNumber);
        }
      else if( check_level == 1 )
        {
          errorFlag = false;
          if ( !(block1k_s[1] == BLOCK_ID ||
                 block1k_s[1] == _trailerFmt.blockId ||
                 block1k_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode)) )
            //|| block1k_s[_blockSize/2-1]==0 )//End-of-block can be NULL trlr!
            {
              printf("ERROR ");
              errorFlag = true;
              if (blockWithError == 0) blockWithError= block1k;

              printf("Blk %4d header: %02X %02X %02X %02X, trailer: ",
                     blocknr,
                     block1k[0], block1k[1], block1k[2], block1k[3]);
              for( uint32_t i=0; i<_trailerFmt.nbytes; ++i )
                printf( "%02X ", block1k[_blockSize-_trailerFmt.nbytes+i] );
              printf( "\n" );
              //printf("eLinkNr 0x%03X, seqNumber %3d\n", eLinkNr, seqNumber);

              // Skip further checks: next block...
              block1k_s = block1k_s + _blockSize/2;
              block1k   = block1k   + _blockSize;
              ++_blocksHandled;
              continue;
            }
        }
      else if( check_level >= 2 )
        {
          if( !(block1k_s[1] == BLOCK_ID ||
                block1k_s[1] == _trailerFmt.blockId ||
                block1k_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode)) )
            {
              printf("[BLCK] %4d header: %02X %02X %02X %02X, trailer: ",
                     blocknr,
                     block1k[0], block1k[1], block1k[2], block1k[3]);
              for( uint32_t i=0; i<_trailerFmt.nbytes; ++i )
                printf( "%02X ", block1k[_blockSize-_trailerFmt.nbytes+i] );
              printf( "\n" );
              errorFlag = true;
              errorIndex = 0;
              if (blockWithError == 0) blockWithError= block1k;

              // Skip further checks: next block...
              block1k_s = block1k_s + _blockSize/2;
              block1k   = block1k   + _blockSize;
              ++_blocksHandled;
              continue;
            }
          //if( block1k_s[_blockSize/2-1] == 0 ) NULL chunk is no error anymore
          if( ((block1k_s[_blockSize/2-1] & _trailerFmt.typeMsk) >>
               _trailerFmt.typeShift) == CHUNKTYPE_UNDEFINED6 )
            {
              printf("[BLCK] %4d trailer: ", blocknr);
              for( uint32_t i=0; i<_trailerFmt.nbytes; ++i )
                printf( "%02X ", block1k[_blockSize-_trailerFmt.nbytes+i] );
              printf(", header: %02X %02X %02X %02X\n",
                     block1k[0], block1k[1], block1k[2], block1k[3]);
              errorFlag = true;
              errorIndex = 1020;
              if (blockWithError == 0) blockWithError= block1k;

              // Skip further checks: next block...
              block1k_s = block1k_s + _blockSize/2;
              block1k   = block1k   + _blockSize;
              ++_blocksHandled;
              continue;
            }
        }

      // Check sequence number
      // (no error on very first sequence number)
      not_first_block = true;
      if( seqNrFirst[eLinkNr] )
        {
          seqNrFirst[eLinkNr] = false;
          // Make this the current sequence number..
          seqNrExpected[eLinkNr] = seqNumber;

          // To use in checkChunks(): no check for unexpected LAST chunk:
          not_first_block = false;
        }
      else if( seqNumber != seqNrExpected[eLinkNr] )
        {
          //  Unexpected sequence number
          if( check_level == 0 )
            printf( "### " ); // To stand out from 'normal' output lines
          printf( "[BLCK] E=%03X Blk=%d: seqnr %d, expected %d\n",
                  eLinkNr, blocknr, seqNumber, seqNrExpected[eLinkNr] );
          // Make this the current sequence number..
          seqNrExpected[eLinkNr] = seqNumber;

          // To use in checkChunks(): no check for unexpected LAST chunk:
          not_first_block = false;
          firstTypeCounter[eLinkNr] = 0; // Forget about previous FIRST chunk
        }

      // For IDs 0 and 1 these were all the checks, so skip to next block..
      if( check_level == 0 || check_level == 1 )
        {
          block1k_s = block1k_s + _blockSize/2;
          block1k   = block1k   + _blockSize;
          ++_blocksHandled;
          continue;
        }

      if( !checkChunks( block1k, blocknr, eLinkNr,
                        not_first_block, check_level,
                        report_te, report_crc, _isPhase2[eLinkNr] ) )
        /*{
          // For check_level > 2 check the payload (emulator) data
          if( check_level > 2 &&
              !parseChunkData( block1k, blocknr, eLinkNr, check_level ) )
            errorFlag = true;
        }
        else*/
        {
          errorFlag = true;
        }

      if( errorFlag )
        {
          ++errorCount;
          if (blockWithError == 0) blockWithError = block1k;
        }

      if( errorFlag && dump_on_error )
        {
          if( blockWithError == 0 ) blockWithError = block1k;

          if( errorIndex > 32 )
            errorIndex -= 32;
          else
            errorIndex = 0;
          this->displayBlock( block1k, blocknr, 64, errorIndex );
        }
      errorFlag = false;

      if( errorCount > 1000 )
        {
          printf( "### Over 1000 errors in this file block -> "
                  "skip check of remainder\n" );
          //break;
          // Clear stored intermediate results in order to not interfere
          // when starting check of the next batch of data blocks
          for( int i=0; i<=FLX_MAX_ELINK_NR; ++i )
            {
              totalSubchunkBytes[i]  = 0;
              runningLengthSum[i][0] = 0;
              runningLengthSum[i][1] = 0;
              firstTypeCounter[i]    = 0;
              seqNrFirst[i]          = true;
            }
        }

      block1k_s = block1k_s + _blockSize/2;
      block1k   = block1k   + _blockSize;
      ++_blocksHandled;
    }

  //if( blockWithError )
  //  {
  //    printf( "\n----> First block with error" );
  //    this->displayBlock( blockWithError );
  //    printf( "<---- First block with error\n" );
  // }
}

// ----------------------------------------------------------------------------

bool FlxDataChecker::checkChunks( uint8_t *block, int blocknr, int elinknr,
                                  bool not_first_block, int check_level,
                                  bool report_te, bool report_crc,
                                  bool phase2 )
{
  bool success = true;
  uint32_t trailer, length, type, truncbit, errbit, crcbit;
  bool first_chunk_check = true;
  int  l1id_expected = -1, prevblock_l1id = -1;
  bool firsttype_seen = false;

  int gbt   = (elinknr & BLOCK_GBT_MASK)    >> BLOCK_GBT_SHIFT;
  int grp   = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
  int epath = (elinknr & BLOCK_EPATH_MASK)  >> BLOCK_EPATH_SHIFT;
  int eindex= ((elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) >>
               BLOCK_EPATH_SHIFT);

  std::ostringstream info;
  info << "E=" << std::hex << std::uppercase << std::setfill('0')
       << std::setw(3) << elinknr << std::dec << std::setfill(' ')
       << "=" << std::setw(2) << gbt << "-";
  if( phase2 )
    info << eindex;
  else
    info << grp << "-" << epath;
  info << " Blk=" << blocknr;

  // Loop through the chunks in the block, from *end-of-block* to *begin*
  int index = _blockSize - _trailerFmt.nbytes;
  while( index > BLOCKHEADER_BYTES-1 )
    {
      trailer = 0;
      for( uint32_t i=0; i<_trailerFmt.nbytes; ++i )
        trailer |= (block[index+i] << (i*8));
      length   = (trailer & _trailerFmt.lengthMsk);
      type     = (trailer & _trailerFmt.typeMsk)   >> _trailerFmt.typeShift;
      truncbit = (trailer & _trailerFmt.truncMsk)  >> _trailerFmt.truncShift;
      errbit   = (trailer & _trailerFmt.errorMsk)  >> _trailerFmt.errorShift;
      crcbit   = (trailer & _trailerFmt.crcerrMsk) >> _trailerFmt.crcerrShift;

      // Keep track of number of error flags occurrences
      if( truncbit )  ++_truncCount;
      if( errbit )    ++_errCount;
      if( crcbit )    ++_crcerrCount;

      // Out-Of-Band or Null chunk trailer implies: no payload data
      // (Null chunk trailer length bits may be used for debug purposes!)
      if( type == CHUNKTYPE_OUTOFBAND || type == CHUNKTYPE_NULLFILL )
        length = 0;

      // Keep track of number of subchunk types
      ++subchunksPerType[type][elinknr];

      // Check for invalid chunk types
      if( type == CHUNKTYPE_UNDEFINED6 )
        {
          printf( "[CHNK] %s i=%4d: undefined chunktype %u\n",
                  info.str().c_str(), index, type );
          errorIndex = index;
          success = false;
          index -= (length + _trailerFmt.nbytes +
                    (length & (_trailerFmt.nbytes-1)));
          continue;
        }

      // For DEBUG purposes only (effect of additional info in NULL trailer):
      //if( type == CHUNKTYPE_NULLFILL )
      //  {
      //    printf( "[CHNK] %s i=%4d: "
      //            "trunc=%d err=%d crc=%d trailer=%x\n",
      //            info.str().c_str(), index,
      //            truncbit, errbit, crcbit, trailer );
      //  }

      // Check for error or truncation
      if( (report_te && (truncbit == 1 || errbit == 1)) ||
          (report_crc && crcbit == 1) )
        {
          printf( "[CHNK] %s i=%4d: trunc=%u err=%u crc=%u\n",
                  info.str().c_str(), index, truncbit, errbit, crcbit );
          errorIndex = index;
          success = false;
        }

      // Keep track of total chunk length and minimum/maximum chunk sizes
      if( type == CHUNKTYPE_FIRST )
        {
          firsttype_seen = true;
          if( firstTypeCounter[elinknr] <= 1 )
            {
              // Start (a second) length sum
              runningLengthSum[elinknr][firstTypeCounter[elinknr]] = length;
              ++firstTypeCounter[elinknr];
            }
          else
            {
              // Error: have seen 2 FIRST so far (in this block at end,
              // and in previous block at end to be matched with
              // expected LAST in this block at start):
              // there shouldn't be another one!
              printf( "[CHNK] %s i=%4d: multiple FIRST subchunks\n",
                      info.str().c_str(), index );
              errorIndex = index;
              success = false;
              // Restart (second) running length
              runningLengthSum[elinknr][1] = length;
            }
        }
      else if( type == CHUNKTYPE_MIDDLE )
        {
          // 'MIDDLE' is only possible with a 'FIRST' in the previous block
          // and no 'FIRST' in this block
          if( !not_first_block )
            {
              runningLengthSum[elinknr][0] += length;
            }
          else if( firstTypeCounter[elinknr] == 1 )
            {
              runningLengthSum[elinknr][0] += length;
            }
          // Added 'if' to handle initial multi-MIDDLE chunks (7 Nov 2017):
          else if( runningLengthSum[elinknr][0] == 0 )
            {
              // Error
              printf( "[CHNK] %s i=%4d: "
                      "unexpected MIDDLE subchunk (FIRSTs received: %d)\n",
                      info.str().c_str(), index, firstTypeCounter[elinknr] );
              errorIndex = index;
              success = false;
            }
        }
      // First block may start with a LAST-type chunk,
      // for which we don't have a FIRST..
      else if( type == CHUNKTYPE_LAST && not_first_block )
        {
          if( firstTypeCounter[elinknr] > 0 )
            {
              if( !(firsttype_seen && firstTypeCounter[elinknr] == 1) )
                {
                  int total_len = runningLengthSum[elinknr][0] + length;

                  // Move second running length sum-in-progress,
                  // if any, to first position
                  if( firstTypeCounter[elinknr] == 2 )
                    {
                      runningLengthSum[elinknr][0] = runningLengthSum[elinknr][1];
                      runningLengthSum[elinknr][1] = 0;
                    }
                  else
                    {
                      runningLengthSum[elinknr][0] = 0;
                    }

                  // A 'FIRST' has been combined with this 'LAST'
                  --firstTypeCounter[elinknr];

                  // Update min and max chunk size if necessary
                  if( total_len > maxChunkLength[elinknr] )
                    maxChunkLength[elinknr] = total_len;
                  if( total_len < minChunkLength[elinknr] ||
                      minChunkLength[elinknr] == 0 )
                    minChunkLength[elinknr] = total_len;

                  ++numberOfChunks[elinknr];
                }
              // Added 'if' to handle initial multi-MIDDLE chunks (7 Nov 2017):
              else if( runningLengthSum[elinknr][0] == 0 )
                {
                  // Error: only unmatched FIRST is at the end of this block
                  printf( "[CHNK] %s i=%4d: unexpected LAST subchunk\n",
                          info.str().c_str(), index );
                  errorIndex = index;
                  success = false;
                }
            }
          else
            {
              // Error: we have no unmatched FIRST at all
              printf( "[CHNK] %s i=%4d: "
                      "unexpected LAST subchunk (no unmatched FIRST)\n",
                      info.str().c_str(), index );
              errorIndex = index;
              success = false;
            }
        }
      else if( type == CHUNKTYPE_BOTH )//||
               //(type == CHUNKTYPE_TIMEOUT && truncbit) )
        {
          // Update minimum and maximum chunk size if necessary
          if( length > maxChunkLength[elinknr] )
            maxChunkLength[elinknr] = length;
          if( length < minChunkLength[elinknr] || minChunkLength[elinknr] == 0 )
            minChunkLength[elinknr] = length;

          ++numberOfChunks[elinknr];
        }
      else if( type == CHUNKTYPE_TIMEOUT && truncbit )
        {
          // This may be a truncated chunk that should be added
          // to a previous FIRST chunk
          if( firstTypeCounter[elinknr] > 0 &&
              (index - length + (length & 1)) == BLOCKHEADER_BYTES )
            {
              if( !(firsttype_seen && firstTypeCounter[elinknr] == 1) )
                {
                  int total_len = runningLengthSum[elinknr][0] + length;

                  // Move second running length sum-in-progress,
                  // if any, to first position
                  if( firstTypeCounter[elinknr] == 2 )
                    {
                      runningLengthSum[elinknr][0] = runningLengthSum[elinknr][1];
                      runningLengthSum[elinknr][1] = 0;
                    }
                  else
                    {
                      runningLengthSum[elinknr][0] = 0;
                    }

                  // A 'FIRST' has been combined with this 'LAST'
                  --firstTypeCounter[elinknr];

                  // Update min and max chunk size if necessary
                  if( total_len > maxChunkLength[elinknr] )
                    maxChunkLength[elinknr] = total_len;
                  if( total_len < minChunkLength[elinknr] ||
                      minChunkLength[elinknr] == 0 )
                    minChunkLength[elinknr] = total_len;

                  ++numberOfChunks[elinknr];
                }
              // Added 'if' to handle initial multi-MIDDLE chunks (7 Nov 2017):
              else if( runningLengthSum[elinknr][0] == 0 )
                {
                  // Error: only unmatched FIRST is at the end of this block
                  printf( "[CHNK] %s i=%4d: unexpected LAST subchunk\n",
                          info.str().c_str(), index );
                  errorIndex = index;
                  success = false;
                }
            }
          else
            {
              // Treat the truncated chunk as a BOTH type chunk:
              // Update minimum and maximum chunk size if necessary
              if( length > maxChunkLength[elinknr] )
                maxChunkLength[elinknr] = length;
              if( length < minChunkLength[elinknr] || minChunkLength[elinknr] == 0 )
                minChunkLength[elinknr] = length;

              ++numberOfChunks[elinknr];
            }
        }
      else if( type == CHUNKTYPE_OUTOFBAND )
        {
          // Nothing to be done..?
          // Or: report error
          printf( "[CHNK] %s i=%4d: OUT-OF-BAND trailer\n",
                  info.str().c_str(), index );
        }
      else if( type == CHUNKTYPE_NULLFILL )
        {
          // Nothing to be done..
        }

      // Keep track of the number of significant data bytes
      totalSubchunkBytes[elinknr] += length;

      // Go to start of this chunk; account for possible padding bytes
      if( (length & (_trailerFmt.nbytes-1)) != 0 )
        index -= (length + _trailerFmt.nbytes -
                  (length & (_trailerFmt.nbytes-1)));
      else
        index -= length;

      if( index >= BLOCKHEADER_BYTES )
        {
          if( check_level > 2 && length > 0 &&
              (type == CHUNKTYPE_FIRST || type == CHUNKTYPE_BOTH ||
               (type == CHUNKTYPE_TIMEOUT && truncbit)) )
            // Don't check a LAST subchunk
            {
              if( (elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) ==
                  ((7 << BLOCK_EGROUP_SHIFT) | (3 << BLOCK_EPATH_SHIFT)) )
                {
                  // Do checks on the (TTC-to-Host) data
                  if( !this->checkTtcChunkData( &block[index], length,
                                                type, elinknr, blocknr, index,
                                                first_chunk_check,
                                                &l1id_expected ) )
                    success = false;
                }
              else
                {
                  // Do checks on the (emulator) data
                  if( !this->checkChunkData( &block[index], length,
                                             type, elinknr, blocknr, index,
                                             first_chunk_check,
                                             &l1id_expected, phase2 ) )
                    success = false;
                }
              if( first_chunk_check && length > CHUNK_CNTR_I )
                {
                  first_chunk_check = false;
                  // Remember this block's *last* L1ID
                  // (checked as first chunk going from end to start of block)
                  prevblock_l1id = l1id_expected + 1;
                  if( prevblock_l1id > maxL1id[elinknr] )
                    prevblock_l1id = 0;
                }
            }
        }
      else
        {
          printf( "[CHNK] %s i=%4u: invalid chunk length %u\n",
                  info.str().c_str(),
                  index + (length + (length & (_trailerFmt.nbytes-1))),
                  length );
          errorIndex = index + (length + (length & (_trailerFmt.nbytes-1)));
          success = false;
        }

      // Move to the preceeding trailer
      index -= _trailerFmt.nbytes;
    }

  // Check at this point if the L1ID (l1id_expected = next expected L1ID,
  // counting down from end- to start-of-block) matches the *last* L1ID
  // of the *previous* block from this elink
  // (there could be a wrap-around and could be off by 1 because
  //  we don't check FIRST chunk if it is too short to extract an L1ID)
  if( prevBlockL1id[elinknr] >= 0 && l1id_expected > 0 &&
      l1id_expected != 255 && // Only 1 byte for l1id: wrap-around occurs
      !(prevBlockL1id[elinknr] == l1id_expected ||
        prevBlockL1id[elinknr] == l1id_expected-1) )
    {
      printf( "[CHNK] %s i=%4d: first-1 L1ID=%d, expected %d\n",
              info.str().c_str(), BLOCKHEADER_BYTES,
              l1id_expected, prevBlockL1id[elinknr] );
      errorIndex = index;
      success = false;
    }
  // Remember this block's last L1ID (at end-of-block) for the next block
  if( prevblock_l1id >= 0 )
    prevBlockL1id[elinknr] = prevblock_l1id;

  // Expect at this point to have arrived at the block header bytes
  // (i.e. at start-of-block)
  index += _trailerFmt.nbytes;
  if( index != BLOCKHEADER_BYTES )
    {
      printf( "[CHNK] %s i=%4d: arrived at invalid start-of-block index\n",
              info.str().c_str(), index - BLOCKHEADER_BYTES );
      errorIndex = index;
      success = false;
    }

  return success;
}

// ----------------------------------------------------------------------------

bool FlxDataChecker::checkChunkData( uint8_t *chunk, uint32_t length,
                                     uint32_t type, int elinknr, int blocknr,
                                     int index, bool first_chunk_check,
                                     int *l1id_expected, bool phase2 )
{
  // The following only really 'works' for emulator-type data...
  bool success = true;

  int gbt   = (elinknr & BLOCK_GBT_MASK)    >> BLOCK_GBT_SHIFT;
  int grp   = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
  int epath = (elinknr & BLOCK_EPATH_MASK)  >> BLOCK_EPATH_SHIFT;
  int eindex= ((elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) >>
               BLOCK_EPATH_SHIFT);

  std::ostringstream info;
  info << "E=" << std::hex << std::uppercase << std::setfill('0')
       << std::setw(3) << elinknr << std::dec << std::setfill(' ')
       << "=" << std::setw(2) << gbt << "-";
  if( phase2 )
    info << eindex;
  else
    info << grp << "-" << epath;
  info << " Blk=" << blocknr;

  if( chunk[CHUNK_ID_I] != CHUNK_ID )
    {
      printf( "[DATA] %s i=%4d: ID=0x%X expected at %d, got 0x%X\n",
              info.str().c_str(), index,
              CHUNK_ID, index+CHUNK_ID_I, chunk[CHUNK_ID_I] );
      errorIndex = index;
      success = false;
    }
  else if( length > CHUNK_AA_I &&
           (chunk[CHUNK_BB_I] != 0xBB || chunk[CHUNK_AA_I] != 0xAA) )
    {
      printf( "[DATA] %s i=%4d: 0xAABB expected at %d, got 0x%02X%02X\n",
              info.str().c_str(), index, index+CHUNK_BB_I,
              chunk[CHUNK_AA_I], chunk[CHUNK_BB_I] );
      errorIndex = index;
      success = false;
    }
  else if( length > CHUNK_CNTR_I )
    {
      // Emulator data chunk: extract 'L1ID', elink 'id' (egroup+epath),
      // chunk size and width in bits of this elink
      //int l1id  = chunk[CHUNK_CNTR_LO_I] + (chunk[+CHUNK_CNTR_HI_I] << 8);
      int l1id    = chunk[CHUNK_CNTR_I];

      // Check socalled 'L1ID',
      // but not if it's the first chunk check done on this block
      if( !first_chunk_check &&
          *l1id_expected != 255 && // Only 1 byte for l1id: wrap-around occurs
          l1id != *l1id_expected &&
          l1id <= maxL1id[elinknr] ) // Max L1ID may still not have been
                                     // determined so don't consider
                                     // a larger l1id an error...
        {
          errorIndex = index;
          success = false;
          printf( "[DATA] %s i=%4d: "
                  "L1ID expected %d, got %d (0x%X) (block end->start)\n",
                  info.str().c_str(), index, *l1id_expected, l1id, l1id );
        }

      // Update maximum L1ID found sofar if required
      if( l1id > maxL1id[elinknr] )
        maxL1id[elinknr] = l1id;

      // Next expected L1ID (NB: going from block end to block start,
      // so 'l1id_expected' counts down!)
      if( l1id == 0 )
        l1id = maxL1id[elinknr] + 1; // Wrap-around
      *l1id_expected = (l1id - 1) & 0xFFFF;

      // If this subchunk contains the rest of the emulator chunk info too,
      // continue, otherwise bail out here
      if( length <= CHUNK_ELINKBITS_I )
        return success;

      int eid     = chunk[CHUNK_EID_I];
      int datasz  = chunk[CHUNK_SZ_LO_I] + (chunk[CHUNK_SZ_HI_I] << 8);
      int ewidth  = chunk[CHUNK_ELINKBITS_I];

      // Check Elink width
      if( eLinkWidths[elinknr] == 0 )
        {
          eLinkWidths[elinknr] = ewidth;
          if( !(ewidth == 2 || ewidth == 4 || ewidth == 8 || ewidth == 16) )
            {
              printf( "[DATA] %s i=%4d: invalid ewidth %d\n",
                      info.str().c_str(), index+CHUNK_ELINKBITS_I, ewidth );
              errorIndex = index+CHUNK_ELINKBITS_I;
              success = false;
            }
        }
      else
        {
          if( eLinkWidths[elinknr] != ewidth )
            {
              printf( "[DATA] %s i=%4d: ewidth %d, expected %d\n",
                      info.str().c_str(), index+CHUNK_ELINKBITS_I,
                      ewidth, eLinkWidths[elinknr]);
              errorIndex = index+CHUNK_ELINKBITS_I;
              success = false;
              // Adjust it...
              eLinkWidths[elinknr] = ewidth;
            }
        }

      // Check Elink 'ID' (just epath and egroup bits)
      if( eid != (elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) )
        {
          printf( "[DATA] %s i=%4d: e-id %02X, expected %02X\n",
                  info.str().c_str(), index+CHUNK_EID_I,
                  eid, elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK) );
        }

      // Check payload
      int      errcntr = 0;
      uint32_t data_index = index + CHUNKHDR_SIZE;
      uint8_t *payload = &chunk[CHUNKHDR_SIZE];
      if( datasz > (int)(length - CHUNKHDR_SIZE) )
        // Guard against checking past the end of a (FIRST-type) chunk
        // or past the end of a truncated chunk
        datasz = length - CHUNKHDR_SIZE;
      for( int i=0; i<datasz; ++i, ++data_index )
        {
          // Guard against checking past the end of the block
          if( data_index >= _blockSize - _trailerFmt.nbytes )
            break;

          // Expect an incrementing byte counter
          if( payload[i] != (i & 0xFF) )
            {
              printf( "[DATA] %s i=%4d: data expected 0x%02X, got 0x%02X\n",
                      info.str().c_str(), data_index, i & 0xFF, payload[i] );
              ++errcntr;
              if( errcntr > 4 )
                break;
            }
        }
      if( errcntr > 0 )
        {
          errorIndex = data_index;
          success = false;
        }
    }
  return success;
}

// ----------------------------------------------------------------------------

bool FlxDataChecker::checkTtcChunkData( uint8_t *chunk, uint32_t length,
                                        uint32_t type, int elinknr, int blocknr,
                                        int index, bool first_chunk_check,
                                        int *l1id_expected )
{
  bool success = true;

  if( !((chunk[0] == 1 && chunk[1] == 20) ||
        (chunk[0] == 2 && chunk[1] == 26)) )
    {
      printf( "[TTC2H] E=%03X Blk=%d i=%4d: "
              "Format '1'+length=20 or '2'+26 expected at %d, "
              "got 0x%X + %d\n",
              elinknr, blocknr, index, index+0, chunk[0], chunk[1] );
      errorIndex = index;
      success = false;
    }
  //else if( length > 2+2+4 )
  else if( length == 20 || length == 26 )
    {
      // Event counter in bytes 4-6 and ECR-counter in byte 7 (together: L1ID)
      int l1id = ((((int) chunk[4] & 0xFF) << 0) |
                  (((int) chunk[5] & 0xFF) << 8) |
                  (((int) chunk[6] & 0xFF) << 16));
      int l0id = ((((int) chunk[16] & 0xFF) << 0) |
                  (((int) chunk[17] & 0xFF) << 8) |
                  (((int) chunk[18] & 0xFF) << 16));
      // ###FOR THE TIME BEING: in byte 5-7
      /*
      int l1id = ((((int) chunk[5] & 0xFF) << 0) |
                  (((int) chunk[6] & 0xFF) << 8) |
                  (((int) chunk[7] & 0xFF) << 16));
      int l0id = ((((int) chunk[17] & 0xFF) << 0) |
                  (((int) chunk[18] & 0xFF) << 8) |
                  (((int) chunk[19] & 0xFF) << 16));
      */

      // Check socalled 'L1ID',
      // but not if it's the first chunk check done on this block
      if( !first_chunk_check &&
          l1id != *l1id_expected )
        {
          errorIndex = index;
          success = false;
          printf( "[TTC2H] E=%03X Blk=%d i=%4d: "
                  "L1ID expected %d, got %d (0x%X) (block end->start)\n",
                  elinknr, blocknr, index, *l1id_expected, l1id, l1id );
        }

      // Check socalled 'L0ID'
      if( l0id != l1id )
        {
          errorIndex = index;
          success = false;
          printf( "[TTC2H] E=%03X Blk=%d i=%4d: "
                  "L0ID expected %d, got %d (0x%X)\n",
                  elinknr, blocknr, index, l1id, l0id, l0id );
        }

      // Update maximum L1ID found sofar if required
      if( l1id > maxL1id[elinknr] )
        maxL1id[elinknr] = l1id;

      // Next expected L1ID (NB: going from block end to block start,
      // so 'l1id_expected' counts down!)
      *l1id_expected = (l1id - 1) & 0x00FFFFFF;
    }

  return success;
}

// ----------------------------------------------------------------------------

void FlxDataChecker::displaySummary()
{
  // Display a list of Elinks and info about their (sub)chunks (type, size)
  // compiled while running a data blocks check
  int  elinks_2bit = 0, elinks_4bit = 0, elinks_8bit = 0;//,elinks_16bit = 0;
  int  elinks_24bit = 0, elinks_28bit = 0;
  int  elinks_ttc = 0, elinks_aux = 0, elinks_ic = 0, elinks_ec = 0;
  int  elinks_total = 0;
  bool may_be_fullmode = false;

  bool something_to_display = false;
  for( int elinknr=0; elinknr<=BLOCK_ELINK_MAX; ++elinknr )
    if( eLinkBlocks[elinknr] != 0 )
      {
        something_to_display = true;
        break;
      }

  if( something_to_display )
    printf("\nElink  G-g-p  Max L1Id   Blocks  Width   Bytes      "
           "null   first    last    both   middle timeout   oob   nchunks  "
           "minSize  maxSize\n");

  long blocks_total = 0;
  for( int elinknr=0; elinknr<=BLOCK_ELINK_MAX; ++elinknr )
    {
      if( eLinkBlocks[elinknr] != 0 )
        {
          ++elinks_total;
          blocks_total += eLinkBlocks[elinknr];
          printf( "0x%03X", elinknr );
          int gbt   = elinknr >> BLOCK_GBT_SHIFT;
          int grp   = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
          int epath = elinknr & BLOCK_EPATH_MASK;
          int eindex= ((elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) >>
                       BLOCK_EPATH_SHIFT);
          if( _isPhase2[elinknr] )
            printf(" %2d-%2d  %6d %10ld %6d %9ld",
                   gbt, eindex, maxL1id[elinknr], eLinkBlocks[elinknr],
                   eLinkWidths[elinknr], totalSubchunkBytes[elinknr]);
          else
            printf(" %2d-%d-%d %6d %10ld %6d %9ld",
                   gbt, grp, epath, maxL1id[elinknr], eLinkBlocks[elinknr],
                   eLinkWidths[elinknr], totalSubchunkBytes[elinknr]);

          bool display_all = false;
          for( int ntype=0; ntype<5; ++ntype )
            if( subchunksPerType[ntype][elinknr] != 0 )
              display_all = true;
          if( !display_all && (numberOfChunks[elinknr] != 0 ||
                               minChunkLength[elinknr] != 0 ||
                               maxChunkLength[elinknr] != 0) )
            display_all = true;

          if( display_all )
            {
              printf("%9ld ", subchunksPerType[CHUNKTYPE_NULLFILL][elinknr]);
              for( int ntype=1; ntype<6; ++ntype )
                printf("%7ld ", subchunksPerType[ntype][elinknr]);
              printf("%6ld ", subchunksPerType[CHUNKTYPE_OUTOFBAND][elinknr]);
              printf("%9ld %7ld %7ld", numberOfChunks[elinknr],
                     minChunkLength[elinknr], maxChunkLength[elinknr]);
            }
          printf( "\n" );

          if( eLinkWidths[elinknr] == 2 ) ++elinks_2bit;
          else if( eLinkWidths[elinknr] ==  4 ) ++elinks_4bit;
          else if( eLinkWidths[elinknr] ==  8 ) ++elinks_8bit;
          //else if( eLinkWidths[elinknr] == 16 ) ++elinks_16bit;
          else //if( eLinkWidths[elinknr] == 0 )
            {
              // Classify the E-link width or type,
              // partly based on the presence of data from other E-links
              long *b = &eLinkBlocks[elinknr];
              switch( epath )
                {
                case 0:
                case 4:
                  if( b[1] != 0 )
                    ++elinks_2bit;
                  else
                    {
                      ++elinks_24bit;
                      if( grp == 0 )
                        {
                          // If any other E-link other than 0 is present
                          // on any of the links we're probably not
                          // dealing with a FULLMODE 'E-link' here...
                          bool fullmode = true;
                          for( int lnk=0; lnk<24; ++lnk )
                            {
                              b = &eLinkBlocks[lnk*64];
                              for( int e=1; e<7*8; ++e )
                                if( b[e] != 0 )
                                  fullmode = false;
                            }
                          if( fullmode )
                            may_be_fullmode = true;
                        }
                    }
                  break;
                case 1:
                case 5:
                  if( grp != 7 )
                    {
                      if( *(b-1) != 0 || *(b+1) != 0 || *(b+2) != 0 )
                        ++elinks_2bit;
                      else
                        ++elinks_28bit;
                    }
                  else
                    ++elinks_aux;
                  break;
                case 2:
                case 6:
                  if( grp != 7 )
                    {
                      if( b[1] != 0 )
                        ++elinks_2bit;
                      else
                        ++elinks_24bit;
                    }
                  else
                    ++elinks_ic;
                  break;
                case 3:
                  if( grp != 7 )
                    ++elinks_2bit;
                  else
                    ++elinks_ttc;
                  break;
                case 7:
                  if( grp != 7 )
                    ++elinks_2bit;
                  else
                    ++elinks_ec;
                  break;
                default:
                  break;
                }
            }
        }
    }
  printf( "Total blocks: %ld", blocks_total );
  if( _undefBlocks > 0 )
    {
      printf( " (+UNDEF %ld)\n", blocks_total + _undefBlocks );
      printf( "### UNDEFINED-HEADER BLOCKS: %ld\n", _undefBlocks );
    }
  else
    {
      printf( "\n" );
    }

  // Display the E-links widths/types as accurate as possible
  printf( "Total number of Elinks %d: width/type", elinks_total );
  if( elinks_2bit > 0 )
    printf( " 2bit:%d",   elinks_2bit );
  if( elinks_4bit > 0 )
    printf( " 4bit:%d",   elinks_4bit );
  if( elinks_8bit > 0 )
    printf( " 8bit:%d",   elinks_8bit );
  if( elinks_28bit > 0 )
    printf( " 2bit|8bit:%d", elinks_28bit );
  if( elinks_24bit > 0 )
    {
      printf( " 2bit|4bit" );
      if( may_be_fullmode )
        printf( "|FULL" );
      printf( ":%d", elinks_24bit );
    }
  if( elinks_ec > 0 )
    printf( " EC:%d", elinks_ec );
  if( elinks_ic > 0 )
    printf( " IC:%d", elinks_ic );
  if( elinks_aux > 0 )
    printf( " AUX:%d", elinks_aux );
  if( elinks_ttc > 0 )
    printf( " TTC2HOST:%d", elinks_ttc );

  int tot = (elinks_2bit + elinks_4bit + elinks_8bit +
             elinks_24bit + elinks_28bit +
             elinks_ttc + elinks_aux + elinks_ic + elinks_ec);
  if( elinks_total - tot != 0 )
    printf( " ###UNKNOWN:%d", elinks_total - tot );
  printf( "\n" );

  if( _truncCount > 0 || _errCount > 0 || _crcerrCount > 0 )
    printf( "Totals error flags: trunc=%ld, err=%ld, crc=%ld\n",
            truncCount(), errCount(), crcerrCount() );
}

// ----------------------------------------------------------------------------

int FlxDataChecker::displayBlock( uint8_t *block, int blocknr,
                                  int      nr_of_bytes,
                                  int      start_offset,
                                  int      elink_filter,
                                  int      word_size )
{
  u_short *block_s = (u_short *) block;
  int elinkseqnr = (int) block_s[0];
  int gbt   = (elinkseqnr & BLOCK_GBT_MASK)    >> BLOCK_GBT_SHIFT;
  int grp   = (elinkseqnr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
  int epath = (elinkseqnr & BLOCK_EPATH_MASK)  >> BLOCK_EPATH_SHIFT;
  int seqnr = (elinkseqnr & BLOCK_SEQNR_MASK)  >> BLOCK_SEQNR_SHIFT;
  int eindex= ((elinkseqnr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) >>
               BLOCK_EPATH_SHIFT);

  elink_filter = mapElink( elink_filter );

  // Apply Elink filter when appropriate
  if( elink_filter >= 0 && (elinkseqnr & BLOCK_ELINK_MASK) != elink_filter )
    return 0;

  printf( "==> BLOCK " );
  if( blocknr >= 0 ) printf( "%d ", blocknr );
  // Valid block header?
  if( block_s[1] == BLOCK_ID ||
      block_s[1] == _trailerFmt.blockId )
    {
      printf( "(E=%03X=%d-%d-%d seq=%d):",
              (elinkseqnr & BLOCK_ELINK_MASK), gbt, grp, epath, seqnr );
    }
  else if( block_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode) )
    {
      printf( "(E=%03X=%d-%d seq=%d):",
              (elinkseqnr & BLOCK_ELINK_MASK), gbt, eindex, seqnr );
    }
  else
    {
      printf( "###INVALID HEADER (%02X %02X %02X %02X)",
              block[0], block[1], block[2], block[3] );
    }

  // Round start offset to nearest lower 16-byte multiple, larger than 0
  if( start_offset < 0 )
    start_offset = 0;
  start_offset = (start_offset/16) * 16;
  if( start_offset > _blockSize )
    start_offset = 0;

  // Adjust number of bytes to display if necessary
  int nr_of_bytes_from_start = 0;
  if( nr_of_bytes < 1 || nr_of_bytes > _blockSize )
    nr_of_bytes = _blockSize;
  // More bytes requested than remaining in this block?
  // -> it means we want to see also the beginning of the block
  if( start_offset + nr_of_bytes > _blockSize )
    {
      // Display part of the beginning of the block
      // (to see end of previous block combined with start of this block)
      nr_of_bytes_from_start = start_offset + nr_of_bytes - _blockSize;
      // Round off to a multiple of 16
      nr_of_bytes_from_start = ((nr_of_bytes_from_start+15)/16)*16;

      // Display from the start of the block
      this->displayDataWords( block, nr_of_bytes_from_start, word_size, true );
      printf( "\n...." );

      // Next, display until end of block starting at the given offset
      nr_of_bytes = _blockSize - start_offset;
    }

  this->displayDataWords( &block[start_offset], nr_of_bytes, word_size,
                          true, start_offset );
  printf( "\n" );

  return 1;
}

// ----------------------------------------------------------------------------

int FlxDataChecker::displayChunks( uint8_t *block, int blocknr,
                                   bool     display_size,
                                   bool     display_timeout,
                                   bool     display_timeout_chunk,
                                   bool     display_sca,
                                   bool     chunkdata_only,
                                   int      elink_filter,
                                   int      word_size,
                                   bool     is_sca_cmd )
{
  u_short *block_s = (u_short *) block;
  int elinkseqnr = (int) block_s[0];
  int gbt   = (elinkseqnr & BLOCK_GBT_MASK)    >> BLOCK_GBT_SHIFT;
  int grp   = (elinkseqnr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
  int epath = (elinkseqnr & BLOCK_EPATH_MASK)  >> BLOCK_EPATH_SHIFT;
  int seqnr = (elinkseqnr & BLOCK_SEQNR_MASK)  >> BLOCK_SEQNR_SHIFT;
  int eindex= ((elinkseqnr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) >>
               BLOCK_EPATH_SHIFT);

  elink_filter = mapElink( elink_filter );

  // Apply Elink filter on request
  if( elink_filter >= 0 && (elinkseqnr & BLOCK_ELINK_MASK) != elink_filter )
    return 0;

  if( !chunkdata_only )
    {
      printf( "==> BLOCK " );
      if( blocknr >= 0 ) printf( "%d ", blocknr );
      // Valid block header?
      if( block_s[1] == BLOCK_ID ||
          block_s[1] == _trailerFmt.blockId )
        {
          printf( "(E=%03X=%d-%d-%d seq=%d):",
                  (elinkseqnr & BLOCK_ELINK_MASK), gbt, grp, epath, seqnr );
        }
      else if( block_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode) )
        {
          printf( "(E=%03X=%d-%d seq=%d):",
                  (elinkseqnr & BLOCK_ELINK_MASK), gbt, eindex, seqnr );
        }
      else
        {
          printf( "###INVALID HEADER (%02X %02X %02X %02X)",
                  block[0], block[1], block[2], block[3] );
        }
    }

  // Go through the chunks in the block, from *end-of-block* to *begin*,
  // collecting info about each for subsequent display from *start-of-block*
  uint32_t trailer;
  std::vector<chunk_desc_t> chunks;
  chunk_desc_t chnk;
  int index = _blockSize - _trailerFmt.nbytes;
  while( index > BLOCKHEADER_BYTES-1 )
    {
      trailer = 0;
      for( uint32_t i=0; i<_trailerFmt.nbytes; ++i )
        trailer |= (block[index+i] << (i*8));
      chnk.length = trailer & _trailerFmt.lengthMsk;
      chnk.type   = ((trailer & _trailerFmt.typeMsk) >> _trailerFmt.typeShift);
      chnk.trunc  = ((trailer & _trailerFmt.truncMsk) != 0);
      chnk.err    = ((trailer & _trailerFmt.errorMsk) != 0);
      chnk.crcerr = ((trailer & _trailerFmt.crcerrMsk)!= 0);

      // Out-Of-Band or Null chunk trailer implies: no payload data
      if( chnk.type == CHUNKTYPE_OUTOFBAND || chnk.type == CHUNKTYPE_NULLFILL )
        {
          chnk.length = 0;
          chnk.trunc  = false;
          chnk.err    = false;
          chnk.crcerr = false;
        }

      // The start of this chunk; account for possible padding bytes
      if( (chnk.length & (_trailerFmt.nbytes-1)) != 0 )
        index -= (chnk.length + _trailerFmt.nbytes -
                  (chnk.length & (_trailerFmt.nbytes-1)));
      else
        index -= chnk.length;
      chnk.index = index;

      // Is resulting index valid ?
      if( index > BLOCKHEADER_BYTES-1 )
        {
          chnk.invalid_sz = -1;
        }
      else
        {
          // Length can't be correct
          chnk.invalid_sz = chnk.length;
          // Adjust for display...
          chnk.index = BLOCKHEADER_BYTES;
          chnk.length -= (BLOCKHEADER_BYTES -
                          (index + (chnk.length & (_trailerFmt.nbytes-1))));
        }
      chunks.push_back( chnk );

      // Move to the preceeding trailer
      index -= _trailerFmt.nbytes;
    }

  if( !chunkdata_only )
    {
      // Display total number of data bytes in block
      unsigned int c, total_len = 0;
      for( c=0; c<chunks.size(); ++c )
        total_len += chunks[c].length;
      printf( " %u databytes", total_len );
    }

  // Display chunk-by-chunk
  int c, oob_cntr = 0, null_cntr = 0;
  unsigned int type;
  for( c=chunks.size()-1; c>=0; --c )
    {
      type = chunks[c].type;
      if( !chunkdata_only )
        {
          if( oob_cntr != 0 && type != CHUNKTYPE_OUTOFBAND )
            {
              if( oob_cntr == 1 )
                printf( "\n ##" );
              else
                printf( "\n ## (%dx)", oob_cntr );
              oob_cntr = 0;
            }
          if( null_cntr != 0 && type != CHUNKTYPE_NULLFILL )
            {
              if( null_cntr == 1 )
                printf( "\n @@" );
              else
                printf( "\n @@ (%dx)", null_cntr );
              null_cntr = 0;
            }
        }

      if( !(type == CHUNKTYPE_TIMEOUT &&
            (display_timeout == false || display_timeout_chunk == false) &&
            chunks[c].trunc == false ) ) // Truncation indicates data is inside
        this->displayDataWords( &block[chunks[c].index],
                                chunks[c].length, word_size );

      if( chunks[c].length == 0 &&
          (type != CHUNKTYPE_OUTOFBAND && type != CHUNKTYPE_NULLFILL) )
        printf( "\n " );
      else if( (type == CHUNKTYPE_TIMEOUT &&
                (display_timeout == true || display_timeout_chunk == false) &&
                chunks[c].trunc == false) &&
               chunks[c].length > 0 )
        printf( "\n " );

      if( !chunkdata_only )
        {
          if( type == CHUNKTYPE_FIRST )
            printf( "++" );
          else if( type == CHUNKTYPE_BOTH )
            printf( "<<" );
          else if( type == CHUNKTYPE_LAST )
            printf( "&&" );
          else if( type == CHUNKTYPE_MIDDLE )
            printf( "==" );
          else if( type == CHUNKTYPE_TIMEOUT && display_timeout )
            printf( "]]" );
          else if( type == CHUNKTYPE_OUTOFBAND )
            {
              if( chunks[c].length == 0 )
                ++oob_cntr; // Often there are many consecutive: display count
              else
                printf( "##" );
            }
          else if( type == CHUNKTYPE_NULLFILL )
            {
              if( chunks[c].length == 0 )
                ++null_cntr; // Often there are many consecutive: display count
              else
                printf( "@@" );
            }
          else if( type == CHUNKTYPE_UNDEFINED6 )
            {
              printf( "??" );
            }

          if( chunks[c].trunc )
            printf( "T" );
          if( chunks[c].err )
            printf( "E" );
          if( chunks[c].crcerr )
            printf( "C" );
          if( chunks[c].invalid_sz >= 0 )
            printf( " ###CHUNKSIZE###(%d)", chunks[c].invalid_sz );

          if( display_size &&
              (type == CHUNKTYPE_BOTH ||
               type == CHUNKTYPE_FIRST ||
               type == CHUNKTYPE_LAST ||
               (type == CHUNKTYPE_TIMEOUT && display_timeout) ||
               type == CHUNKTYPE_MIDDLE) )
            printf( " (sz=%u)", chunks[c].length );

          // See if the chunk could be interpreted as an SCA frame,
          // then display some more info about it;
          // SCA-frame (address, control, 2xFCS) = 4 bytes + payload
          if( display_sca && type == CHUNKTYPE_BOTH &&
              //(chunks[c].length <= (4+8) && chunks[c].length >= (4+4)) )
              chunks[c].length <= (4+8) )
            {
              uint8_t *sca_frame = &block[chunks[c].index];
              uint32_t sca_bytes = chunks[c].length;
              if( sca_frame[0] == 0x00 || sca_frame[0] == 0xFF )
                this->displayScaFrame( sca_frame, sca_bytes, is_sca_cmd );
            }
          else if( display_sca && chunks[c].length == 26 &&
                   grp == 7 && epath == 3 )
            {
              this->displayTtcFrame( &block[chunks[c].index] );
            }
        }
    }
  if( oob_cntr != 0 )
    printf( "\n ## (%dx)", oob_cntr );
  if( null_cntr != 0 )
    printf( "\n @@ (%dx)", null_cntr );
  printf( "\n" );

  return 1;
}

// ----------------------------------------------------------------------------

int FlxDataChecker::displayDataChunks( uint8_t *block,
                                       int      blocknr,
                                       bool     display_sca,
                                       int      elink_filter,
                                       int      word_size,
                                       bool     is_sca_cmd )
{
  elink_filter = mapElink( elink_filter );

  // Going to go through the chunks in this block
  bool phase2 = this->blockIsPhase2( block );
  int elinknr = this->blockElinkNr( block );
  if( this->setBlockToHandle( block, elink_filter ) )
    {
      // Apply Elink filter?
      if( elink_filter == -1 || elink_filter == elinknr )
        printf( "***Block %d\n", blocknr );
      else
        return 0;
    }
  else
    {
      printf( "### Invalid block" );
      if( blocknr >= 0 )
        printf( ", Block %d", blocknr );
      printf( "\n" );
      return 0;
    }

  int gbt   = (elinknr & BLOCK_GBT_MASK)    >> BLOCK_GBT_SHIFT;
  int grp   = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
  int epath = (elinknr & BLOCK_EPATH_MASK)  >> BLOCK_EPATH_SHIFT;
  int eindex= ((elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) >>
               BLOCK_EPATH_SHIFT);
  uint8_t  *framedata;
  uint32_t  framesize;
  int       errbits;
  while( this->nextChunkFromBlock( &framedata, &framesize, &errbits ) )
    {
      if( framesize == 0 && framedata == 0 ) continue; // Not complete chunk yet

      // Framesize 0 is also taken into account
      if( phase2 )
        printf( "=> E=%03X=%d-%d size=%u",
                elinknr, gbt, eindex, framesize );
      else
        printf( "=> E=%03X=%d-%d-%d size=%u",
                elinknr, gbt, grp, epath, framesize );
      if( errbits != 0 ) printf( " ERR" );
      printf( ":" );
      this->displayDataWords( framedata, framesize, word_size );

      // See if the chunk could be interpreted as an SCA frame,
      // then display some more info about it;
      // SCA-frame (address, control, 2xFCS) = 4 bytes + payload
      if( display_sca && framesize <= (4+8) &&
          (framedata[0] == 0x00 || framedata[0] == 0xFF) )
        {
          this->displayScaFrame( framedata, framesize, is_sca_cmd );
          printf( "\n" );
        }
      else if( display_sca && framesize == 26 && grp == 7 && epath == 3 )
        {
          this->displayTtcFrame( framedata );
        }
      else
        {
          if( (framesize & 0x1F) != 1 )
            printf( "\n" );
        }
    }
  return 1;
}

// ----------------------------------------------------------------------------

void FlxDataChecker::displayDataWords( uint8_t *bytes,
                                       uint32_t size,
                                       int      word_size,
                                       bool     display_index,
                                       uint32_t initial_index )
{
  uint8_t *byt = bytes;
  uint32_t i = initial_index;
  if( word_size == 1 )
    {
      // Display byte-by-byte
      for( ; i<size; ++i, ++byt )
        {
          if( (i & 0x1F) == 0 )
            {
              if( display_index )
                printf( "\n%4d:  ", i );
              else
                printf( "\n " );
            }
          printf( "%2x ", *byt );
        }
    }
  else
    {
      // Display as 'word_size'-byte words (little-endian)
      uint64_t word = 0;
      bool     word_completed = true;
      unsigned int bitmask = word_size-1;
      for( ; i<size; ++i, ++byt )
        {
          if( (i & 0x1F) == 0 )
            {
              if( display_index )
                printf( "\n%4d:  ", i );
              else
                printf( "\n " );
            }

          // Compile multi-byte words, then display
          word |= ((uint64_t)(*byt)) << ((i & bitmask)*8);
          word_completed = false;
          if( (i & bitmask) == bitmask )
            {
              if( word_size == 2 )
                printf( "%04lx ", word );
              else if( word_size == 4 )
                printf( "%08lx ", word );
              else
                printf( "%016lx ", word );
              // Reset word
              word = 0;
              word_completed = true;
            }
        }
      // Any remaining undisplayed data?
      if( !word_completed )
        {
          int remaining_bytes = (size-initial_index -
                                 ((size-initial_index)/word_size)*word_size);
          if( remaining_bytes == 1 )
            printf( "%02lx ", word );
          else if( remaining_bytes == 2 )
            printf( "%04lx ", word );
          else if( remaining_bytes == 3 )
            printf( "%06lx ", word );
          else
            printf( "%lx(###%d)", word, remaining_bytes );
        }
    }
}

// ----------------------------------------------------------------------------

const char *SCA_DEV_STR[] = {
  "CONF",  "SPI",   "GPIO",
  "I2C-0", "I2C-1", "I2C-2", "I2C-3", "I2C-4", "I2C-5", "I2C-6", "I2C-7",
  "I2C-8", "I2C-9", "I2C-A", "I2C-B", "I2C-C", "I2C-D", "I2C-E", "I2C-F",
  "JTAG",  "ADC",   "DAC"
};
const char *SCA_ERR_STR[] = {
  "????", "CHAN", "CMD", "TRID", "LEN", "DISA", "BUSY", "PROG"
};

void FlxDataChecker::displayScaFrame( uint8_t *sca_frame, uint32_t nbytes,
                                      bool is_cmd, bool standalone )
{
  if( !standalone ) // Slightly modified output
    printf( "\n  =>SCA: " );

  if( nbytes < 4 )
    {
      if( standalone )
        printf( "(not a GBT-SCA message)" );
      else
        printf( "---" );
      return;
    }

  //uint32_t addr = (uint32_t) sca_frame[0]; // Address is always 0 (or 0xFF)
  uint32_t ctrl = (uint32_t) sca_frame[1];

  // Received and sent counters (modulo 8)
  printf( "CTRL=%02X(r=%d,s=%d", ctrl,
          (ctrl >> HDLC_CTRL_NRECVD_SHIFT) & HDLC_CTRL_SEQNR_MASK,
          (ctrl >> HDLC_CTRL_NSENT_SHIFT) & HDLC_CTRL_SEQNR_MASK );

  if( ctrl & HDLC_CTRL_SFORMAT )
    {
      printf( ",type=" );
      uint32_t type = ctrl & HDLC_CTRL_TYPE_MASK;
      if( type == HDLC_CTRL_RECVREADY )
        printf( "READY" );
      else if( type == HDLC_CTRL_REJECT )
        printf( "REJ" );
      else if( type == HDLC_CTRL_RECVNOTREADY )
        printf( "NOTREADY" );
      else if( type == HDLC_CTRL_SELREJECT )
        printf( "SREJ" );
      else
        printf( "---" );
    }

  // Poll/Final bit (how to handle it?)
  if( ctrl & HDLC_CTRL_POLLBIT )
    printf( ",pf=%u", ctrl & HDLC_CTRL_POLLBIT );

  if( nbytes < 8 )
    {
      // Check correctness of the CRC (###Moved to end; see further down)
      if( crc16( sca_frame, nbytes ) != (uint16_t) 0 )
        {
          printf( ") ###CRC=%04X, ", crc16( sca_frame, nbytes-2 ) );
          ++_scaCrcErrors;
        }
      else printf( ") CRC=OK " );
      return;
    }

  uint32_t trid = (uint32_t) sca_frame[2];
  uint32_t chan = (uint32_t) sca_frame[3];
  uint32_t err  = (uint32_t) sca_frame[4];
  uint32_t len  = (uint32_t) sca_frame[5];
  uint32_t cmd  = 0xFF;
  if( is_cmd )
    {
      len = (uint32_t) sca_frame[4];
      cmd = (uint32_t) sca_frame[5];
    }

  // Transaction ID (TRID) is the number to match to the original command
  printf( ") TRID=%u Chan=", trid );

  if( chan <= SCA_DEV_MAX )
    printf( "%s ", SCA_DEV_STR[chan] );
  else
    printf( "??? " );

  if( is_cmd )
    {
      printf( "CMD=%02X(", cmd );
      if( chan == SCA_DEV_CONFIG )
        {
          if( cmd == SCA_CONFIG_WR_A )
            printf( "CONFIG_WR_A" );
          else if( cmd == SCA_CONFIG_RD_A )
            printf( "CONFIG_RD_A" );
          else if( cmd == SCA_CONFIG_WR_B )
            printf( "CONFIG_WR_B" );
          else if( cmd == SCA_CONFIG_RD_B )
            printf( "CONFIG_RD_B" );
          else if( cmd == SCA_CONFIG_WR_C )
            printf( "CONFIG_WR_C" );
          else if( cmd == SCA_CONFIG_RD_C )
            printf( "CONFIG_RD_C" );
          else if( cmd == SCA_CONFIG_WR_D )
            printf( "CONFIG_WR_D" );
          else if( cmd == SCA_CONFIG_RD_D )
            printf( "CONFIG_RD_D" );
          else
            printf( "---" );
        }
      else if( chan == SCA_DEV_GPIO )
        {
          if( cmd == SCA_GPIO_WR_OUT )
            printf( "GPIO_WR_OUT" );
          else if( cmd == SCA_GPIO_RD_OUT )
            printf( "GPIO_RD_OUT" );
          else if( cmd == SCA_GPIO_RD_IN )
            printf( "GPIO_RD_IN" );
          else if( cmd == SCA_GPIO_WR_DIR )
            printf( "GPIO_WR_DIR" );
          else if( cmd == SCA_GPIO_RD_DIR )
            printf( "GPIO_RD_DIR" );
          else
            printf( "---" );
        }
      else if( chan == SCA_DEV_DAC )
        {
          if( cmd == SCA_DAC_A_WR )
            printf( "DAC_A_WR" );
          else if( cmd == SCA_DAC_B_WR )
            printf( "DAC_B_WR" );
          else if( cmd == SCA_DAC_C_WR )
            printf( "DAC_C_WR" );
          else if( cmd == SCA_DAC_D_WR )
            printf( "DAC_D_WR" );
          else if( cmd == SCA_DAC_A_RD )
            printf( "DAC_A_RD" );
          else if( cmd == SCA_DAC_B_RD )
            printf( "DAC_B_RD" );
          else if( cmd == SCA_DAC_C_RD )
            printf( "DAC_C_RD" );
          else if( cmd == SCA_DAC_D_RD )
            printf( "DAC_D_RD" );
          else
            printf( "---" );
        }
      else if( chan == SCA_DEV_ADC )
        {
          if( cmd == SCA_ADC_GO )
            printf( "ADC_GO" );
          else if( cmd == SCA_ADC_WR_MUX )
            printf( "ADC_WR_MUX" );
          else if( cmd == SCA_ADC_RD_MUX )
            printf( "ADC_RD_MUX" );
          else if( cmd == SCA_ADC_WR_CURR )
            printf( "ADC_WR_CURR" );
          else if( cmd == SCA_ADC_RD_CURR )
            printf( "ADC_RD_CURR" );
          else if( cmd == SCA_ADC_WR_GAIN )
            printf( "ADC_WR_GAIN" );
          else if( cmd == SCA_ADC_RD_GAIN )
            printf( "ADC_RD_GAIN" );
          else if( cmd == SCA_RD_CHIPID )
            printf( "ADC_RD_CHIPID" );
          else if( cmd == SCA_RD_CHIPID_V1 )
            printf( "ADC_RD_CHIPID_V1" );
          else if( cmd == SCA_ADC_GO_V1 )
            printf( "ADC_GO_V1" );
          else if( cmd == SCA_ADC_WR_MUX_V1 )
            printf( "ADC_WR_MUX_V1" );
          else if( cmd == SCA_ADC_RD_MUX_V1 )
            printf( "ADC_RD_MUX_V1" );
          else if( cmd == SCA_ADC_WR_CUREN_V1 )
            printf( "ADC_WR_CUREN_V1" );
          else if( cmd == SCA_ADC_RD_CUREN_V1 )
            printf( "ADC_RD_CUREN_V1" );
          else
            printf( "---" );
        }
      else if( chan == SCA_DEV_I2C )
        {
          if( cmd == SCA_I2C_WR_CTRL )
            printf( "I2C_WR_CTRL" );
          else if( cmd == SCA_I2C_RD_CTRL )
            printf( "I2C_RD_CTRL" );
          else if( cmd == SCA_I2C_RD_STAT )
            printf( "I2C_RD_STAT" );
          else if( cmd == SCA_I2C_SINGLE_7B_WR )
            printf( "I2C_SINGLE_7B_WR" );
          else if( cmd == SCA_I2C_SINGLE_7B_RD )
            printf( "I2C_SINGLE_7B_RD" );
          else if( cmd == SCA_I2C_MULTI_7B_WR )
            printf( "I2C_MULTI_7B_WR" );
          else if( cmd == SCA_I2C_MULTI_7B_RD )
            printf( "I2C_MULTI_7B_RD" );
          else if( cmd == SCA_I2C_SINGLE_10B_WR )
            printf( "I2C_SINGLE_10B_WR" );
          else if( cmd == SCA_I2C_SINGLE_10B_RD )
            printf( "I2C_SINGLE_10B_RD" );
          else if( cmd == SCA_I2C_MULTI_10B_WR )
            printf( "I2C_MULTI_10B_RD" );
          else if( cmd == SCA_I2C_MULTI_10B_RD )
            printf( "I2C_MULTI_10B_RD" );
          else if( cmd == SCA_I2C_WR_DATA0 )
            printf( "I2C_WR_DATA0" );
          else if( cmd == SCA_I2C_RD_DATA0 )
            printf( "I2C_RD_DATA0" );
          else if( cmd == SCA_I2C_WR_DATA1 )
            printf( "I2C_WR_DATA1" );
          else if( cmd == SCA_I2C_RD_DATA1 )
            printf( "I2C_RD_DATA1" );
          else if( cmd == SCA_I2C_WR_DATA2 )
            printf( "I2C_WR_DATA2" );
          else if( cmd == SCA_I2C_RD_DATA2 )
            printf( "I2C_RD_DATA2" );
          else if( cmd == SCA_I2C_WR_DATA3 )
            printf( "I2C_WR_DATA3" );
          else if( cmd == SCA_I2C_RD_DATA3 )
            printf( "I2C_RD_DATA3" );
          else
            printf( "---" );
        }
      else if( chan == SCA_DEV_JTAG )
        {
          if( cmd == SCA_JTAG_WR_CTRL )
            printf( "JTAG_WR_CTRL" );
          else if( cmd == SCA_JTAG_RD_CTRL )
            printf( "JTAG_RD_CTRL" );
          else if( cmd == SCA_JTAG_WR_FREQ )
            printf( "JTAG_WR_FREQ" );
          else if( cmd == SCA_JTAG_RD_FREQ )
            printf( "JTAG_RD_FREQ" );
          else if( cmd == SCA_JTAG_WR_TDO0 )
            printf( "JTAG_WR_TDO0" );
          else if( cmd == SCA_JTAG_WR_TDO1 )
            printf( "JTAG_WR_TDO1" );
          else if( cmd == SCA_JTAG_WR_TDO2 )
            printf( "JTAG_WR_TDO2" );
          else if( cmd == SCA_JTAG_WR_TDO3 )
            printf( "JTAG_WR_TDO3" );
          else if( cmd == SCA_JTAG_RD_TDI0 )
            printf( "JTAG_RD_TDI0" );
          else if( cmd == SCA_JTAG_RD_TDI1 )
            printf( "JTAG_RD_TDI1" );
          else if( cmd == SCA_JTAG_RD_TDI2 )
            printf( "JTAG_RD_TDI2" );
          else if( cmd == SCA_JTAG_RD_TDI3 )
            printf( "JTAG_RD_TDI3" );
          else if( cmd == SCA_JTAG_WR_TMS0 )
            printf( "JTAG_WR_TMS0" );
          else if( cmd == SCA_JTAG_WR_TMS1 )
            printf( "JTAG_WR_TMS1" );
          else if( cmd == SCA_JTAG_WR_TMS2 )
            printf( "JTAG_WR_TMS2" );
          else if( cmd == SCA_JTAG_WR_TMS3 )
            printf( "JTAG_WR_TMS3" );
          else if( cmd == SCA_JTAG_RD_TMS0 )
            printf( "JTAG_RD_TMS0" );
          else if( cmd == SCA_JTAG_RD_TMS1 )
            printf( "JTAG_RD_TMS1" );
          else if( cmd == SCA_JTAG_RD_TMS2 )
            printf( "JTAG_RD_TMS2" );
          else if( cmd == SCA_JTAG_RD_TMS3 )
            printf( "JTAG_RD_TMS3" );
          else if( cmd == SCA_JTAG_ARESET )
            printf( "JTAG_ARESET" );
          else if( cmd == SCA_JTAG_GO )
            printf( "JTAG_GO" );
          else if( cmd == SCA_JTAG_GO_MANUAL )
            printf( "JTAG_GO_MANUAL" );
          else
            printf( "---" );
        }
      else
        {
          printf( "---" );
        }
      printf( ") " );
    }
  else
    {
      printf( "ERR=%02X ", err );
      if( err != 0 )
        {
          // Error bits set: show which ones
          printf( "(" );
          for( int i=0; i<8; ++i )
            if( err & (1<<i) )
              printf( "%s ", SCA_ERR_STR[i] );
          printf( ") " );
          ++_scaCmdErrors;
        }
    }

  printf( "LEN=%u ", len );

  // It seems that 'len' produced by SCA has illegal values (!?)
  // so use the real length of the frame to determine the payload size
  len = nbytes - (2 + 4 + 2); // Can't rely on 'len' from SCA !!??

  // Data is big-endian (MSB first)?
  if( len == 4 )
    printf( "data=0x%02X%02X%02X%02X",
            //(uint32_t) sca_frame[9], (uint32_t) sca_frame[8],
            //(uint32_t) sca_frame[7], (uint32_t) sca_frame[6] );
            (uint32_t) sca_frame[7], (uint32_t) sca_frame[6],
            (uint32_t) sca_frame[9], (uint32_t) sca_frame[8] );
  else if( len == 3 )
    printf( "data=0x%02X%02X%02X",
            (uint32_t) sca_frame[8],
            (uint32_t) sca_frame[7], (uint32_t) sca_frame[6] );
  else if( len == 2 )
    printf( "data=0x%02X%02X",
            (uint32_t) sca_frame[7], (uint32_t) sca_frame[6] );
  else if( len == 1 )
    printf( "data=0x%02X", (uint32_t) sca_frame[6] );
  else if( len != 0 )
    printf( "###len=%u", len );

  // Check correctness of the CRC
  if( crc16( sca_frame, nbytes ) != (uint16_t) 0 )
    {
      printf( " ###CRC=%04X", crc16( sca_frame, nbytes-2 ) );
      ++_scaCrcErrors;
    }
}

// ----------------------------------------------------------------------------

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic" // Suppress anonymous struct warning

typedef struct {
  unsigned int format        : 8;
  unsigned int length        : 8;
  unsigned int bcid          : 12;
  unsigned int reserved0     : 4;
  union {
    unsigned int full_l1id   : 32;
    struct {
      unsigned int l1id      : 24;
      unsigned int xl1id     : 8;
    };
  };
  unsigned int orbit         : 32;
  unsigned int trigger_type  : 16;
  unsigned int reserved1     : 16;
  unsigned int l0id          : 32;
  unsigned long l1a_counter  : 48;
} __attribute__((packed)) TtcToHost_packet_t;

void FlxDataChecker::displayTtcFrame( uint8_t *ttc_frame )
{
  //uint32_t *frame = (uint32_t *) ttc_frame;
  TtcToHost_packet_t *frame = (TtcToHost_packet_t *) ttc_frame;
  printf( "\n  =>TTC: Fmt=%d Len=%d ", frame->format, frame->length );
  printf( "BCID=%03X L1ID=%08X ", frame->bcid, frame->full_l1id );
  printf( "Orbit=%08X TType=%04X ", frame->orbit, frame->trigger_type );
  printf( "L0ID=%08X L1Acount=%012lX", frame->l0id, (uint64_t)frame->l1a_counter );
  printf( " (%ld)", (uint64_t)frame->l1a_counter );
}

#pragma GCC diagnostic pop
// ----------------------------------------------------------------------------

int FlxDataChecker::displayDataRecvd( FlxReceiver *recvr,
                                      int          timeout_us,
                                      int          elink_filter,
                                      bool         display_sca,
                                      int          expected_trid,
                                      bool         is_sca_cmd )
{
  uint8_t *chunkdata = 0;
  uint32_t size, chunknr = 1;

  elink_filter = mapElink( elink_filter );

  this->setReceiver( recvr );
  while( this->nextChunkRecvd( &chunkdata, &size, timeout_us, elink_filter ) )
    {
      //if( size == 0 ) continue; // ###IS CONSIDERED A CHUNK (but is empty)

      // Display chunk data bytes
      printf( "Chunk %u: ", chunknr );
      uint32_t j;
      for( j=0; j<size; ++j )
        {
          printf( "%2X ", (uint32_t) chunkdata[j] );
          if( (j & 0x1F) == 0x1F ) printf( "\n" );
        }

      // Interpret as GBT-SCA reply?
      if( display_sca && size <= 4+8 &&
          (chunkdata[0] == 0x00 || chunkdata[0] == 0xFF) )
        {
          this->displayScaFrame( chunkdata, size, is_sca_cmd );

          if( size >= 8 && expected_trid > 0 )
            {
              int trid = (int) chunkdata[2];
              if( trid != expected_trid )
                {
                  printf( "### TRID %d, expected %d",
                          trid, expected_trid );
                  expected_trid = trid;
                }
              ++expected_trid;
              if( expected_trid == 255 )
                expected_trid = 1;
            }
        }
      else if( display_sca && size == 26 )
        {
          this->displayTtcFrame( chunkdata );
        }
      printf( "\n" );

      ++chunknr;
    }
  return( chunknr-1 ); // Total number of chunks handled in this call
}

// ----------------------------------------------------------------------------

bool FlxDataChecker::nextChunkRecvd( uint8_t **chunkdata,
                                     uint32_t *size,
                                     int       timeout_us,
                                     int       elink_filter,
                                     int      *elinknr,
                                     int      *errbits )
{
  if( _flxReceiver == 0 ) return false;

  elink_filter = mapElink( elink_filter );

  // Get a (data) chunk from the currently handled block,
  // if available, *including* those with size equal to zero
  // (but skipping any TIMEOUT/NULL/OOB chunks)
  uint32_t sz;
  int      errs;
  while( this->nextChunkFromBlock( chunkdata, &sz, &errs ) &&
         *chunkdata == 0 );

  while( *chunkdata == 0 )
    {
      // Done with this block?
      if( _blocksToHandle > 0 )
        {
          // Free the block space
          _flxReceiver->updateBytesConsumed( _blockSize );
          --_blocksToHandle;
          ++_blocksHandled;
        }

      // When all blocks have been handled, get next series of blocks;
      // either wait if necessary for the set time-out period
      // or quit if there is data available past the set period of time
      // (NB: if there is a lot of 'real' data to process don't use it;
      //      if there is other random data on the line it may be useful
      //      to prevent getting stuck in while(nextChunkRecvd()) forever)
      if( _blocksToHandle == 0 )
        {
          _blocksToHandle = _flxReceiver->bytesAvailable()/_blockSize;
          if( _blocksToHandle == 0 )
            {
              // Retry after time-out
              if( timeout_us > 0 ) usleep( timeout_us );
              _blocksToHandle = _flxReceiver->bytesAvailable()/_blockSize;
            }
          else if( timeout_us > 0 )
            {
              // Receiving data beyond the set time-out? Then quit...
              auto now = std::chrono::system_clock::now();
              if( (now - _startRecvTime) >=
                  std::chrono::microseconds(timeout_us) )
                {
                  _blocksToHandle = 0;
                  return false;
                }
            }
        }

      // Return false when there are no more blocks to handle
      if( _blocksToHandle == 0 )
        return false;

      // Select next block to handle
      _block = (uint8_t *) _flxReceiver->data();

      // Going to go through the chunks in this block
      this->setBlockToHandle( _block, elink_filter );

      while( this->nextChunkFromBlock( chunkdata, &sz, &errs ) &&
             *chunkdata == 0 );
    }

  //printf( "sz=%d\n", sz ); // DEBUG
  *size = sz;
  // Optional return values
  if( elinknr ) *elinknr = _blockElinknr;
  if( errbits ) *errbits = errs;

  return true;
}

// ----------------------------------------------------------------------------

void FlxDataChecker::openOutputFile( std::string filename )
{
  closeOutputFile();
  _fpOut = fopen( filename.c_str(), "w" );
  if( _fpOut == 0 )
    printf( "### Failed to open output file: %s\n", filename.c_str() );
  else
    printf( "Opened output file: %s\n", filename.c_str() );
}

// ----------------------------------------------------------------------------

void FlxDataChecker::closeOutputFile()
{
  if( _fpOut ) fclose( _fpOut );
  _fpOut = 0;
}

// ----------------------------------------------------------------------------

int FlxDataChecker::writeDataChunks( uint8_t *block,
                                     int      blocknr,
                                     int      elink_filter )
{
  elink_filter = mapElink( elink_filter );

  // Going to go through the chunks in this block
  if( this->setBlockToHandle( block, elink_filter ) )
    {
      // Apply Elink filter?
      if( elink_filter >= 0 )
        {
          int elinknr = this->blockElinkNr( block );
          if( elinknr != elink_filter )
            return 0;
        }
    }
  else
    {
      printf( "### Invalid block" );
      if( blocknr >= 0 )
        printf( ", Block %d", blocknr );
      printf( "\n" );
      return 0;
    }

  if( _fpOut == 0 ) return 0;

  uint8_t  *framedata;
  uint32_t  framesize;
  while( this->nextChunkFromBlock( &framedata, &framesize ) )
    {
      if( framesize == 0 ) continue;

      fwrite( framedata, 1, framesize, _fpOut );
    }
  return 1;
}

// ----------------------------------------------------------------------------

int FlxDataChecker::blockElinkNr( uint8_t *block )
{
  // Returns the E-link number datablock 'block' is from,
  // or -1 if the block header is not a valid one
  u_short *block_s = (u_short *) block;
  int      elinknr = -1;

  // Valid block header?
  if( block_s[1] == BLOCK_ID ||
      block_s[1] == _trailerFmt.blockId ||
      block_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode) )
    elinknr = (int) (block_s[0] & BLOCK_ELINK_MASK);

  return elinknr;
}

// ----------------------------------------------------------------------------

bool FlxDataChecker::blockIsPhase2( uint8_t *block )
{
  // Returns whether the block is considered a 'phase2' block or not
  u_short *block_s = (u_short *) block;
  return( block_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode) );
}

// ----------------------------------------------------------------------------

bool FlxDataChecker::setBlockToHandle( uint8_t *block,
                                       int      elink_filter )
{
  bool valid = true;
  u_short *block_s = (u_short *) block;
  int elinknr_seqnr = (int) block_s[0];

  _chunkInfo.clear();
  _chunkIndex   = -1;
  _block        = block;
  _blockElinknr = elinknr_seqnr & BLOCK_ELINK_MASK;

  // Apply Elink filter on request
  if( elink_filter >= 0 && _blockElinknr != elink_filter )
    return valid;

  // Go through the chunks in the block, from *end-of-block* to *begin*,
  // storing the info about each for subsequent calls to nextChunkFromBlock()
  uint32_t     trailer;
  unsigned int type;
  chunk_desc_t chnk;
  int index = _blockSize - _trailerFmt.nbytes;
  while( index > BLOCKHEADER_BYTES-1 )
    {
      trailer = 0;
      for( uint32_t i=0; i<_trailerFmt.nbytes; ++i )
        trailer |= (block[index+i] << (i*8));
      chnk.length = trailer & _trailerFmt.lengthMsk;
      type        = ((trailer & _trailerFmt.typeMsk) >> _trailerFmt.typeShift);
      chnk.type   = type;
      chnk.trunc  = ((trailer & _trailerFmt.truncMsk) != 0);
      chnk.err    = ((trailer & _trailerFmt.errorMsk) != 0);
      chnk.crcerr = ((trailer & _trailerFmt.crcerrMsk)!= 0);
      if( type == CHUNKTYPE_OUTOFBAND || type == CHUNKTYPE_NULLFILL )
        {
          chnk.length = 0; // Out-of-band chunk implies no payload data
          chnk.trunc  = false;
          chnk.err    = false;
          chnk.crcerr = false;
        }

      // The start of this chunk; account for possible padding bytes
      if( (chnk.length & (_trailerFmt.nbytes-1)) != 0 )
        index -= (chnk.length + _trailerFmt.nbytes -
                  (chnk.length & (_trailerFmt.nbytes-1)));
      else
        index -= chnk.length;
      chnk.index = index;

      // Is resulting index valid ?
      if( index > BLOCKHEADER_BYTES-1 )
        {
          chnk.invalid_sz = -1;
        }
      else
        {
          // Length can't be correct
          chnk.invalid_sz = chnk.length;
          // Adjust length...
          chnk.index = BLOCKHEADER_BYTES;
          chnk.length -= (BLOCKHEADER_BYTES -
                          (index + (chnk.length & (_trailerFmt.nbytes-1))));
          valid = false;
        }

      // Only save chunk info that will need processing
      if( type == CHUNKTYPE_BOTH || type == CHUNKTYPE_FIRST ||
          type == CHUNKTYPE_MIDDLE || type == CHUNKTYPE_LAST ||
          (type == CHUNKTYPE_TIMEOUT && chnk.trunc) )
        _chunkInfo.push_back( chnk );

      // Move to the preceeding trailer
      index -= _trailerFmt.nbytes;
    }

  // Start processing from the end of the chunk list
  // (= first chunk in this block)
  _chunkIndex = _chunkInfo.size() - 1;

  return valid;
}

// ----------------------------------------------------------------------------

bool FlxDataChecker::nextChunkFromBlock( uint8_t **chunkdata,
                                         uint32_t *size,
                                         int      *errbits,
                                         int      *elinknr )
{
  // No chunks at all, or all chunks handled ?
  if( _chunkIndex < 0 )
    {
      *size = 0;
      *chunkdata = 0;
      return false; // Block has been fully handled, or there is no block
    }

  chunk_desc_t *pdesc = &_chunkInfo[_chunkIndex];

  // Extract the chunk errors
  int errs = 0;
  if( pdesc->trunc || pdesc->err || pdesc->crcerr )
    {
      if( pdesc->trunc  ) errs |= 1;
      if( pdesc->err    ) errs |= 2;
      if( pdesc->crcerr ) errs |= 4;
    }

  if( pdesc->type == CHUNKTYPE_BOTH )
    {
      *chunkdata = &_block[pdesc->index];
      *size = pdesc->length; // Could be size 0 (but chunkdata has a value!)
      if( errbits ) *errbits = errs;
      if( elinknr ) *elinknr = _blockElinknr;
      // Reset (prevent new 'CHUNKTYPE_TIMEOUT' action below from misbehaving:
      // it could be the 'TIMEOUT' has to be added to a 'FIRST' chunk or not)
      _chunk[_blockElinknr].clear();
      _errBits[_blockElinknr] = 0;
    }
  else
    {
      if( pdesc->type == CHUNKTYPE_FIRST )
        {
          // Store this FIRST chunk part to combine with next (MIDDLEs+) LAST
          _chunk[_blockElinknr].clear();
          _errBits[_blockElinknr] = errs;
          //printf( "FIRST %d\n", pdesc->length );
        }
      else if( pdesc->type == CHUNKTYPE_MIDDLE )
        {
          _errBits[_blockElinknr] |= errs; // Combine errors
        }

      // FIRST, MIDDLE, LAST, TIMEOUT+trunc: add data to _chunk
      _chunk[_blockElinknr].insert( _chunk[_blockElinknr].end(),
                                    &_block[pdesc->index],
                                    &_block[pdesc->index + pdesc->length] );

      // Return chunk at LAST subchunk (or TIMEOUT+trunc)
      if( pdesc->type == CHUNKTYPE_LAST ||
          (pdesc->trunc && pdesc->type == CHUNKTYPE_TIMEOUT) )
        {
          //printf( "LAST %d, %d\n", pdesc->length, pdesc->trunc );
          *chunkdata = _chunk[_blockElinknr].data();
          *size = _chunk[_blockElinknr].size();
          if( errbits )
            *errbits = errs | _errBits[_blockElinknr]; // Combine error bits
          if( elinknr )
            *elinknr = _blockElinknr;
        }
      else
        {
          // Don't return a chunk, but caller must continue block handling
          // so return value is 'true'
          *chunkdata = 0; // This differentiates the returned value from
          *size = 0;      // a 'real' chunk with size=0 (see above)
        }
    }
  --_chunkIndex;
  return true;
}

// ----------------------------------------------------------------------------

int FlxDataChecker::mapElink( int elinknr )
{
#if REGMAP_VERSION >= 0x500
  // EC+IC+AUX egroup? (sticking to the RM4 numbering)
  if( ((elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT) == 7 )
    {
      if( _flxReceiver )
        {
          int epath = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
          elinknr &= ~(BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK);

          if( epath == 7 ) // EC link
            elinknr |= _flxReceiver->ecIndex();
          if( epath == 6 ) // IC link
            elinknr |= _flxReceiver->icIndex();
          //if( epath == 5 )
          // elinknr |= 30; // AUX link
        }

      /* ###OLD HARDCODED APPROACH:
      // ToHost direction: GBTX: EC=40,IC=41; lpGBT: EC=28,IC=29
      int fw = 0;
      if( _flxReceiver )
        fw = _flxReceiver->firmwareMode();
      // Is it lpGBT-type firmware?
      if( fw == FIRMW_PIXEL || fw == FIRMW_STRIP || fw == FIRMW_LPGBT )
        {
          if( epath == 7 ) elinknr |= 28; // EC link
          if( epath == 6 ) elinknr |= 29; // IC link
          //if( epath == 5 ) elinknr |= 30; // AUX link
        }
      else
        {
          if( epath == 7 ) elinknr |= 40; // EC link
          if( epath == 6 ) elinknr |= 41; // IC link
          if( epath == 5 ) elinknr |= 42; // AUX link
        }
      */
    }
#endif // REGMAP_VERSION
  return elinknr;
}

// ----------------------------------------------------------------------------
