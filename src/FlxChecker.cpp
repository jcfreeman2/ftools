#include <cstdio>
#include <cstdlib>
#include <unistd.h> // For usleep()
#include <sstream> // For ostringstream
#include <iomanip>

#include "FlxChecker.h"
#include "FlxReceiver.h"
#include "crc.h"

// ----------------------------------------------------------------------------

FlxChecker::FlxChecker()
{
  this->reset();
}

// ----------------------------------------------------------------------------

void FlxChecker::reset()
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
    }
  _blocksChecked  = 0;
  errorIndex      = -1;
  _undefBlocks    = 0;

  _truncCount     = 0;
  _errCount       = 0;
  _crcerrCount    = 0;
}

// ----------------------------------------------------------------------------

void FlxChecker::checkBlocks( uint64_t startAddress,
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
              ++_blocksChecked;
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
              ++_blocksChecked;
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
              ++_blocksChecked;
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
              ++_blocksChecked;
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
          ++_blocksChecked;
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
      ++_blocksChecked;
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

bool FlxChecker::checkChunks( uint8_t *block, int blocknr, int elinknr,
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
              if( (grp == 7 && epath == 3) ||             // RM4
                  (gbt == 24 && grp == 0 && epath == 0) ) // RM5
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

bool FlxChecker::checkChunkData( uint8_t *chunk, uint32_t length,
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

bool FlxChecker::checkTtcChunkData( uint8_t *chunk, uint32_t length,
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

void FlxChecker::displaySummary()
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
