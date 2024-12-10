#include <cstdio>
#include <cstdlib>
#include <unistd.h> // For usleep()
#include <sstream> // For ostringstream
#include <iomanip>

#include "FlxParser.h"
#include "FlxReceiver.h"
#include "crc.h"

// ----------------------------------------------------------------------------

FlxParser::FlxParser()
{
  _blockSize      = 1024;
  _blockSizeCode  = (_blockSize/1024-1) << 8;
  _block          = 0;
  _blockElinknr   = -1;
  _chunkIndex     = -1;
  _flxReceiver    = 0;
  _blocksToHandle = 0;
  _blocksHandled  = 0;
  _fpOut          = 0;
  _scaCrcErrors   = 0;
  _scaCmdErrors   = 0;
  for( int i=0; i<=FLX_MAX_ELINK_NR; ++i )
    _errBits[i] = 0;
}

// ----------------------------------------------------------------------------

void FlxParser::setReceiver( FlxReceiver *r )
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

int FlxParser::displayBlock( uint8_t *block, int blocknr,
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

int FlxParser::displayChunks( uint8_t *block, int blocknr,
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
          if( display_sca && type == CHUNKTYPE_BOTH )
            {
              if( //(chunks[c].length <= (4+8) && chunks[c].length >= (4+4)) )
                 chunks[c].length <= (4+8) )
                {
                  uint8_t *sca_frame = &block[chunks[c].index];
                  uint32_t sca_bytes = chunks[c].length;
                  if( sca_frame[0] == 0x00 || sca_frame[0] == 0xFF )
                    this->displayScaFrame( sca_frame, sca_bytes, is_sca_cmd );
                }
              else if( chunks[c].length == 26 &&
                       ((grp == 7 && epath == 3) || gbt == 24) ) // RM4 or RM5
                {
                  this->displayTtcFrame( &block[chunks[c].index] );
                }
              //else if( blablabla ) // TO BE DEFINED
              //  {
              //    this->displayIcFrame( &block[chunks[c].index] );
              //  }
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

int FlxParser::displayDataChunks( uint8_t *block,
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
      else if( display_sca && framesize == 26 &&
               ((grp == 7 && epath == 3) || gbt == 24) )
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

void FlxParser::displayDataWords( uint8_t *bytes,
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

const char *PARSER_SCA_DEV_STR[] = {
  "CONF",  "SPI",   "GPIO",
  "I2C-0", "I2C-1", "I2C-2", "I2C-3", "I2C-4", "I2C-5", "I2C-6", "I2C-7",
  "I2C-8", "I2C-9", "I2C-A", "I2C-B", "I2C-C", "I2C-D", "I2C-E", "I2C-F",
  "JTAG",  "ADC",   "DAC"
};
const char *PARSER_SCA_ERR_STR[] = {
  "????", "CHAN", "CMD", "TRID", "LEN", "DISA", "BUSY", "PROG"
};

void FlxParser::displayScaFrame( uint8_t *sca_frame, uint32_t nbytes,
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
    printf( "%s ", PARSER_SCA_DEV_STR[chan] );
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
              printf( "%s ", PARSER_SCA_ERR_STR[i] );
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

void FlxParser::displayTtcFrame( uint8_t *ttc_frame )
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

void FlxParser::displayIcFrame( uint8_t *ic_frame )
{
  // TO BE DONE
}

// ----------------------------------------------------------------------------

int FlxParser::displayDataRecvd( FlxReceiver *recvr,
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

bool FlxParser::nextChunkRecvd( uint8_t **chunkdata,
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

void FlxParser::openOutputFile( std::string filename )
{
  closeOutputFile();
  _fpOut = fopen( filename.c_str(), "w" );
  if( _fpOut == 0 )
    printf( "### Failed to open output file: %s\n", filename.c_str() );
  else
    printf( "Opened output file: %s\n", filename.c_str() );
}

// ----------------------------------------------------------------------------

void FlxParser::closeOutputFile()
{
  if( _fpOut ) fclose( _fpOut );
  _fpOut = 0;
}

// ----------------------------------------------------------------------------

int FlxParser::writeDataChunks( uint8_t *block,
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

int FlxParser::blockElinkNr( uint8_t *block )
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

bool FlxParser::blockIsPhase2( uint8_t *block )
{
  // Returns whether the block is considered a 'phase2' block or not
  u_short *block_s = (u_short *) block;
  return( block_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode) );
}

// ----------------------------------------------------------------------------

bool FlxParser::setBlockToHandle( uint8_t *block,
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

bool FlxParser::nextChunkFromBlock( uint8_t **chunkdata,
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

int FlxParser::mapElink( int elinknr )
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
