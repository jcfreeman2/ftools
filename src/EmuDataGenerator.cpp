#include <iostream>
#include <iomanip>
using namespace std;
#include <stdlib.h>

#include "flxdefs.h"
#include "EmuDataGenerator.h"
//#include "protodune.h"
#define PROTODUNE_FRAMESIZE (116*4)
uint64_t PROTODUNE_DATA[PROTODUNE_FRAMESIZE*4];

// To generate some debug output in generate():
//#define DEBUG_ME

// ----------------------------------------------------------------------------

bool EmuDataGenerator::generate( uint32_t *emudata,
                                 uint32_t  emusize,
                                 uint64_t  nbits_word,
                                 uint64_t  nbits_index_word,
                                 uint32_t  modes_word,
                                 uint32_t  maxchunksize_word,
                                 uint32_t  req_chunksize,
                                 uint32_t  egroup,
                                 uint32_t  pattern_id,
                                 uint32_t  idle_chars,
                                 bool      lsb_first,
                                 bool      emuram_msb_first,
                                 bool      width_proportional_sz,
                                 bool      random_sz,
                                 bool      use_streamid,
                                 bool      soc_is_k287,
                                 bool      add_busy,
                                 bool      omit_one_soc,
                                 bool      omit_one_eoc,
                                 bool      fei4b_kchars )
{
  // Initialize emudata to all zeroes
  uint32_t i;
  for( i=0; i<emusize; ++i ) emudata[i] = 0;

  // Adjust emusize here such that it fits a whole number of 8b10b-coded words
  // in 2-bit width, meaning it also fits 4-, 8- or 16-bit elink widths
  emusize = (((emusize*2)/10)*10)/2;
#ifdef DEBUG_ME
  cout << "emusize = " << emusize << endl;
#endif

#if REGMAP_VERSION < 0x500
  // Resize _chunkData to fit the number of bytes (unencoded)
  // to fill (emusize*16) bits, encoded: (emusize*16)/10 bytes
  // (in case the emudata array would be filled by a single 16-bit elink)
  //_chunkData.resize((emusize * 16) / 10);
  // Unencoded should also fit, so:
  _chunkData.resize((emusize * 16) / 8);
#else
  // 32-bits per emulator word should fit, stored byte-for-byte
  _chunkData.resize((emusize * 32) / 8);
#endif // REGMAP_VERSION

#if REGMAP_VERSION < 0x500
  // Set the *maximum* chunksize to the lowest configured maximum chunksize
  uint32_t max_chunksz = 0x7FFFFFFF;
  for( i=0; i<4; ++i )
    {
      uint32_t max = (maxchunksize_word>>(i*3) & 0x7) * FLX_MAXCHUNK_UNIT;
      if( max == 0 ) max = 0x7FFFFFFF;
      if( max < max_chunksz )
        max_chunksz = max;
    }
#ifdef DEBUG_ME
  cout << "maxchunksz = " << max_chunksz << endl;
#endif
#endif // REGMAP_VERSION

  // Determine the number of chunks per emulator data RAM block:
  // for each E-link width generate the same number of chunks per cycle
  // through the emulator data array (to keep the number of chunks
  // per E-link and their counters synchronized),
  // so use a 2-bit wide E-link as a reference;
  // also subtract the initial 8 comma symbols added
  uint32_t nbits, max_i, max_chunkcnt;
  nbits = 2;
  max_i = ((emusize * nbits)/10) - 8;
  // Chunk size includes (8) info bytes:
  max_chunkcnt = max_i/(1 + req_chunksize + 1 + idle_chars);
  _chunkCount = max_chunkcnt;
#ifdef DEBUG_ME
  cout << "max_i(2-bit)= " << max_i << endl;
  cout << "maxchunkcnt = " << max_chunkcnt << endl;
#endif

  bool success = true;//, emuram_msb_first = false;
  uint32_t mode, chunksz, index;
  uint32_t e_id; // For use in emulator data
  for( uint32_t epath=0; epath<FLX_ELINK_PATHS; ++epath )
    {
#if REGMAP_VERSION < 0x500
      // Number of bits of a path is in the corresponding nibble of nbits_word
      nbits = ((nbits_word >> (epath*4)) & 0xF) + 1;
#else
      // Number of bits of a path is in the corresponding byte of nbits_word
      nbits = ((nbits_word >> (epath*8)) & 0xFF);
#endif // REGMAP_VERSION
      if( nbits <= 1 ) continue; // This is a disabled E-path

      if( nbits_index_word == 0x0000000018100800 )
        // Indicates lpGBT..
        e_id = ((epath  << BLOCK_EPATH_SHIFT) |
                (egroup << (BLOCK_EGROUP_SHIFT-1)));
      else
        e_id = ((epath  << BLOCK_EPATH_SHIFT) |
                (egroup << BLOCK_EGROUP_SHIFT));

      // What is the mode to use for this E-link (path)?
      mode = ((modes_word >> (epath*4)) & 0xF);

      if( mode == 1 ) { // ***** 8b10b mode *****

        /* ## Replaced by a global maximum (see further up):
        // Determine the maximum chunksize allowed for this E-link width
        if( nbits == 2 )
          max_chunksz = (maxchunksize_word>>(0*3) & 0x7) * MAXCHUNK_UNIT;
        else if( nbits == 4 )
          max_chunksz = (maxchunksize_word>>(1*3) & 0x7) * MAXCHUNK_UNIT;
        else if( nbits == 8 )
          max_chunksz = (maxchunksize_word>>(2*3) & 0x7) * MAXCHUNK_UNIT;
        else if( nbits == 16 )
          max_chunksz = (maxchunksize_word>>(3*3) & 0x7) * MAXCHUNK_UNIT;
        if( max_chunksz == 0 )
          max_chunksz = 0x7FFFFFFF; // Basically unlimited..
        */

        // Maximum index to fill in _chunkData for this number-of-bits width
        max_i = (emusize * nbits)/10;

        // Adjust requested chunksize to 'width_proportional_sz' if necessary,
        // i.e. multiply the given chunksize by the width's multiple of 2-bits,
        // in order to make use of as much as possible of the given amount
        // of emulator data space; note that this may easily cause exceeding
        // the maximum chunksize for widths > 2
        if( width_proportional_sz )
          chunksz = req_chunksize * (nbits/2);
        else
          chunksz = req_chunksize;

        // Start with a certain number of comma symbols...
        index = 0;
        for( i=0; i<8; ++i, ++index )
          _chunkData[index] = COMMA_CODE;

        // ..followed by (multiple) chunks
        uint32_t next_index, chunkcntr = 0;
        while( index <= max_i && chunkcntr < max_chunkcnt )
          {
            if( random_sz )
              {
                // Determine a random (data) size to use for the next chunk
                // (here: size between req_chunksize/2 and req_chunksize,
                // adjusted to width_proportional_sz if required)
                uint32_t sz = (req_chunksize + 1)/2;
                double   r  = ((double) rand())/RAND_MAX;
                double   d  = 0.5 * (double)(1 - (req_chunksize & 1));
                if( width_proportional_sz )
                  chunksz = (sz*(nbits/2) +
                             (uint32_t) (((double)(sz*(nbits/2))) * r + d));
                else
                  chunksz = sz + (uint32_t) ((double)sz * r + d);
              }

            // Make sure the allowed maximum chunksize is not exceeded
            // ###OUTCOMMENTED FOR TEST PURPOSES
            //if( chunksz > max_chunksz ) chunksz = max_chunksz;

            // Check if the next chunk will fit
            //next_index = index + (1 + CHUNKHDR_SIZE + chunksz + 1 + 8);
            // chunksz includes header:
            next_index = index + (1 + chunksz + 1 + idle_chars);
            if( next_index > max_i )
              {
                // It won't fit, forget it: from here onwards fill with commas
                for( ; index<max_i; ++index )
                  _chunkData[index] = COMMA_CODE;
                // Should exit the while-loop on the basis of the chunk counter
                // not because of lack of space, so we consider this an error..
                success = false; _errorId = 1;
                continue; // Jump to start of while-loop
              }

            if( chunkcntr == 2 && (omit_one_soc || omit_one_eoc) )// For testing
              index += this->createChunk( &_chunkData[index],
                                          chunksz, chunkcntr,
                                          nbits, pattern_id,
                                          e_id, use_streamid,
                                          omit_one_soc, omit_one_eoc );
            else
              index += this->createChunk( &_chunkData[index],
                                          chunksz, chunkcntr,
                                          nbits, pattern_id,
                                          e_id, use_streamid );

            // Add SOB/EOB for testing?
            if( add_busy && chunkcntr == 1+epath+egroup*8 && idle_chars >= 2)
              {
                // Start-of-Busy
                _chunkData[index++] = SOB_CODE;

                // A configurable number of comma symbols between chunks
                for( i=0; i<idle_chars-2; ++i, ++index )
                  _chunkData[index] = COMMA_CODE;

                // End-of-Busy
                _chunkData[index++] = EOB_CODE;
              }
            else
              {
                // A configurable number of comma symbols between chunks
                for( i=0; i<idle_chars; ++i, ++index )
                  _chunkData[index] = COMMA_CODE;
              }

            ++chunkcntr;

            /* OLD SOB/EOB test
            if( add_busy )
              {
                if( chunkcntr == 1 )
                  // Start-of-Busy
                  _chunkData[index++] = SOB_CODE;
                else if( chunkcntr == 2 )
                  // End-of-Busy
                  _chunkData[index++] = EOB_CODE;
              }
            */
          }

        // Fill any remaining uninitialised array locations with comma symbols
        for( ; index<max_i; ++index )
          _chunkData[index] = COMMA_CODE;

        // We expect to have generated max_chunkcnt chunks!
        if( chunkcntr < max_chunkcnt )
          {
            success = false;
            _errorId = 2;
          }

        // Encode the data (to 8b10b)
        if( !_encoder.encode( _chunkData.data(), max_i,
                              soc_is_k287, fei4b_kchars ) )
          {
            success = false; // Something was not okay...
            _errorId = 3;
          }

#ifdef DEBUG_ME
        // Decode back to original
        uint32_t *pdata = _chunkData.data();
        if( !_encoder.decode( pdata, max_i ) )
          cout << "###ERR decode @byte " << _encoder.errIndex() << endl;
        // Display the decoded data
        cout << "EPATH " << epath << ":";
        cout << hex;
        for( i=0; i<max_i; ++i )
          {
            if( (i & 0x7) == 0 ) cout << endl << setw(3) << i << ": ";
            cout << setw(3) << pdata[i] << " ";
          }
        cout << endl;
        // Re-encode..
        _encoder.encode( pdata, max_i );
#endif // DEBUG_ME

        // Now copy bit-by-bit the (8b10b-)encoded data into the appropriate
        // bits -matching the elink's width- in the emulator data array
#if REGMAP_VERSION < 0x500
        uint32_t nbits_index = ((nbits_index_word >> (epath*4)) & 0xF);
#else
        uint32_t nbits_index = ((nbits_index_word >> (epath*8)) & 0xFF);
#endif // REGMAP_VERSION
        uint32_t bitmask_emu_msb = 1 << (nbits_index + nbits - 1);
        uint32_t bitmask_emu_lsb = 1 << nbits_index;

        uint32_t bitmask_emu;
        if( emuram_msb_first )
          // Fill emulator data left-to-right: msb-to-lsb
          bitmask_emu = bitmask_emu_msb;
        else
          // Fill emulator data right-to-left: lsb-to-msb
          bitmask_emu = bitmask_emu_lsb;

#if REGMAP_VERSION < 0x500
        // NSW 640Mb/s E-links are actually two 320Mb/s E-links
        // with data bits alternating between the two,
        // starting with the lower of the two E-links;
        // NB: these values are for emuram_msb_first:
        uint32_t elink16_bitmask_emu[] = { 0x0080, 0x8000, 0x0040, 0x4000,
                                           0x0020, 0x2000, 0x0010, 0x1000,
                                           0x0008, 0x0800, 0x0004, 0x0400,
                                           0x0002, 0x0200, 0x0001, 0x0100 };
        uint32_t elink16_index = 0;
        if( nbits == 16 )
          bitmask_emu = elink16_bitmask_emu[0];
#endif // REGMAP_VERSION

        uint32_t i_emu = 0;
        for( index=0; index<max_i; ++index )
          {
            uint32_t bitmask_chunk;
            uint32_t word8b10b = _chunkData[index];

            // Start chunk data from MSB (10th bit),
            // where the 8b10b 'a' bit is located,
            // or from LSB ('LSB-first')
            if( lsb_first )
              bitmask_chunk = 1;
            else
              bitmask_chunk = 1 << (10-1);

            while( bitmask_chunk > 0 && bitmask_chunk < 1<<10) // Bit-by-bit
              {
                if( word8b10b & bitmask_chunk )
                  emudata[i_emu] |= bitmask_emu;

                if( emuram_msb_first )
                  {
#if REGMAP_VERSION < 0x500
                    // Special handling of NSW 640Mb/s 16-bit E-links
                    if( nbits == 16 )
                      {
                        elink16_index = (elink16_index + 1) & 0xF;
                        bitmask_emu = elink16_bitmask_emu[elink16_index];
                        if( elink16_index == 0 )
                          ++i_emu; // Go to next emulator RAM location
                      }
                    else
#endif // REGMAP_VERSION
                      {
                        bitmask_emu >>= 1;
                        if( bitmask_emu < bitmask_emu_lsb )
                          {
                            ++i_emu; // Go to next emulator RAM location
                            bitmask_emu = bitmask_emu_msb; // Restart from MSB
                          }
                      }
                  }
                else
                  {
                    bitmask_emu <<= 1;
                    if( bitmask_emu > bitmask_emu_msb )
                      {
                        ++i_emu; // Go to next emulator RAM location
                        bitmask_emu = bitmask_emu_lsb; // Restart from LSB
                      }
                  }
                if( lsb_first )
                  bitmask_chunk <<= 1;
                else
                  bitmask_chunk >>= 1;
              }
          }

      } else if( mode == 0 ) { // ***** 'direct' mode *****

        // Maximum index to fill in _chunkData for this number-of-bits width
        max_i = (emusize * nbits)/8;

        // Start with a certain number of zeros...
        index = 0;
        for( i=0; i<5; ++i, ++index )
          _chunkData[index] = 0;

        // Fill array locations with the requested pattern
        if( pattern_id == 1 )
          {
            // Alternating 0x55/0xAA pattern
            for( ; index<(max_i/256)*256; ++index )
              if( index & 1 )
                _chunkData[index] = 0xAA;
              else
                _chunkData[index] = 0x55;
          }
        else if( pattern_id == 2 )
          {
            // Only 0xFF
            for( ; index<(max_i/256)*256; ++index )
              _chunkData[index] = 0xFF;
          }
        else if( pattern_id == 3 )
          {
            // Only 0x00
            for( ; index<(max_i/256)*256; ++index )
              _chunkData[index] = 0x00;
          }
        else
          {
            // Incrementing byte value
            i = 1;
            for( ; index<(max_i/256)*256; ++index, ++i )
              _chunkData[index] = i & 0xFF; //0xFF; //(1 << (i&7));
          }

        // Fill any remaining uninitialised array locations with zeros
        for( ; index<max_i; ++index )
          _chunkData[index] = 0xFF;

        // Now copy bit-by-bit the link data into the appropriate emulator bits
        // -matching the elink's width- in the emulator data array
#if REGMAP_VERSION < 0x500
        uint32_t nbits_index = ((nbits_index_word >> (epath*4)) & 0xF);
#else
        uint32_t nbits_index = ((nbits_index_word >> (epath*8)) & 0xFF);
#endif // REGMAP_VERSION
        uint32_t bitmask_emu_msb = 1 << (nbits_index + nbits - 1);
        uint32_t bitmask_emu_lsb = 1 << nbits_index;

        uint32_t bitmask_emu;
        if( lsb_first )
          // Fill emulator data right-to-left: lsb-to-msb
          bitmask_emu = bitmask_emu_lsb;
        else
          // Fill emulator data left-to-right: msb-to-lsb
          bitmask_emu = bitmask_emu_msb;

        uint32_t i_emu = 0;
        for( index=0; index<max_i; ++index )
          {
            // Start data from LSB (bit 0)
            uint32_t bitmask_chunk = 1 << 0;
            uint32_t byte = _chunkData[index];

            while( bitmask_chunk < 0x100 ) // 8 bits, bit-by-bit
              {
                if( byte & bitmask_chunk )
                  emudata[i_emu] |= bitmask_emu;

                if( lsb_first )
                  {
                    bitmask_emu <<= 1;
                    if( bitmask_emu > bitmask_emu_msb )
                      {
                        ++i_emu; // Go to next emulator RAM location
                        bitmask_emu = bitmask_emu_lsb; // Start from LSB
                      }
                  }
                else
                  {
                    bitmask_emu >>= 1;
                    if( bitmask_emu < bitmask_emu_lsb )
                      {
                        ++i_emu; // Go to next emulator RAM location
                        bitmask_emu = bitmask_emu_msb; // Start from MSB
                      }
                  }

                bitmask_chunk <<= 1;
              }
          }
      }

    }

  return success;
}

// ----------------------------------------------------------------------------

uint32_t EmuDataGenerator::createChunk( uint32_t *chunk_data,
                                        uint32_t  chunk_size,
                                        uint32_t  chunk_counter,
                                        uint32_t  nbits,
                                        uint32_t  pattern_id,
                                        uint32_t  e_id,
                                        bool      use_streamid,
                                        bool      omit_soc,
                                        bool      omit_eoc )
{
  uint32_t chunk_data_size, i, index = 0;

  // Start-of-chunk
  if( !omit_soc )
    chunk_data[index++] = SOC_CODE;

  // The chunk header
  // Currently:
  // Header format: [0xAA0, 12 bit] [packet length in bytes, 12 bit]
  //                [L1ID, 16bit] [0xBBAA, 16 bit]
  // (extra byte added to make it a 16-bit multiple).
  if( chunk_size < (uint32_t) CHUNKHDR_SIZE )
    chunk_size = (uint32_t) CHUNKHDR_SIZE; // Need at least space for header
  chunk_data_size = chunk_size - CHUNKHDR_SIZE;
  if( use_streamid )
    chunk_data[index] = (chunk_counter & 0xFF); // Chunk counter = StreamID
  else
    chunk_data[index] = 0xAA;
  //chunk_data[index + 1] = (chunk_data_size & 0xF00) >> 8;
  chunk_data[index + 1] = (chunk_data_size & 0xFF00) >> 8; // Use all 16 bits
  chunk_data[index + 2] = (chunk_data_size & 0xFF);
  //chunk_data[index+3] = (chunk_counter & 0xFF00) >> 8;
  //chunk_data[index+4] = (chunk_counter & 0xFF);
  chunk_data[index + 3] = (chunk_counter & 0xFF);
  chunk_data[index + 4] = e_id;
  chunk_data[index + 5] = 0xBB;
  chunk_data[index + 6] = 0xAA;
  chunk_data[index + 7] = nbits;
  index += CHUNKHDR_SIZE;

  // The chunk data
  if( pattern_id == 1 )
    {
      // Alternating 0x55/0xAA pattern
      for( i=0; i<chunk_data_size; ++i )
        if( i & 1 )
          chunk_data[index+i] = 0xAA;
        else
          chunk_data[index+i] = 0x55;
    }
  else if( pattern_id == 2 )
    {
      // Only 0xFF
      for( i=0; i<chunk_data_size; ++i )
        chunk_data[index+i] = 0xFF;
    }
  else if( pattern_id == 3 )
    {
      // Only 0x00
      for( i=0; i<chunk_data_size; ++i )
        chunk_data[index+i] = 0x00;
    }
  else
    {
      // Incrementing byte value
      for( i=0; i<chunk_data_size; ++i )
        chunk_data[index+i] = (i & 0xFF);
    }
  // For debugging: add current 'epath' to data
  //if( chunk_data_size > 0 ) chunk_data[index] = epath;

  index += chunk_data_size;

  // End-of-chunk
  if( !omit_eoc )
    chunk_data[index++] = EOC_CODE;

  return index;
}

// ----------------------------------------------------------------------------

// IDLE=K28.5, SOP=K28.1, EOP=K28.6, SOB=K28.2, EOB=K28.3
const uint64_t FM_KCHAR_IDLE =  (((uint64_t) 1<<32) | 0xBC);
const uint64_t FM_KCHAR_SOP  =  (((uint64_t) 1<<32) | 0x3C);
const uint64_t FM_KCHAR_EOP  =  (((uint64_t) 1<<32) | 0xDC);
const uint64_t FM_KCHAR_SOB  =  (((uint64_t) 1<<32) | 0x5C);
const uint64_t FM_KCHAR_EOB  =  (((uint64_t) 1<<32) | 0x7C);

bool EmuDataGenerator::generateFm( uint64_t *emudata,
                                   uint64_t  emusize,
                                   uint32_t  req_chunksize,
                                   uint32_t  pattern_id,
                                   uint32_t  idle_chars,
                                   bool      random_sz,
                                   bool      crc_new,
                                   bool      use_streamid,
                                   bool      add_busy,
                                   bool      omit_one_soc,
                                   bool      omit_one_eoc,
                                   bool      add_crc_err )
{
  // Initialize emudata to all zeroes
  uint32_t i;
  for( i=0; i<emusize; ++i ) emudata[i] = 0;

  // Determine the number of chunks that will fit
  // (chunk size includes 8-byte header): 2 IDLEs, SOP, chunk, EOP
  uint32_t max_chunkcnt = (emusize-2)/(1 + req_chunksize/4 + 1 + idle_chars);
  uint32_t index = 0;
  bool     success = true;
  _chunkCount = max_chunkcnt;
 
  // Start with some IDLE symbols
  emudata[index++] = FM_KCHAR_IDLE;
  emudata[index++] = FM_KCHAR_IDLE;

  // Multiple chunks
  uint32_t next_index, chunkcntr = 0, chunksz, chunk_datasz;
  while( index < emusize && chunkcntr < max_chunkcnt )
    {
      if( random_sz && req_chunksize > 8 ) // Size not less than 8
        {
          // Determine a random (data) size to use for the next chunk
          // (here: size between req_chunksize/2 and req_chunksize,
          //  but rounded up to a multiple of 4 bytes)
          uint32_t sz = (req_chunksize + 1)/2;
          double   r  = ((double) rand())/RAND_MAX;
          double   d  = 0.5 * (double)(1 - (req_chunksize & 1));
          chunksz = ((sz + (uint32_t) ((double)sz * r + d) + 3)/4)*4;
        }
      else
        {
          chunksz = req_chunksize;
        }

      // Check if the next chunk will fit
      // (chunksz includes header)
      next_index = index + (1 + chunksz/4 + 1);
      if( next_index >= emusize )
        {
          // It won't fit, so forget it: from here onwards fill with IDLEs
          for( ; index<emusize; ++index )
            emudata[index] = FM_KCHAR_IDLE;
          // Should exit the while-loop on the basis of the chunk counter
          // so we consider this an error...
          success = false; _errorId = 1;
          continue; // Jump to start of while-loop
        }

      // SOP
      emudata[index++] = FM_KCHAR_SOP;
      if( omit_one_soc && chunkcntr == 2 ) --index; // For testing

      // Add chunk header
      chunk_datasz = chunksz - CHUNKHDR_SIZE;
      if( use_streamid )
        emudata[index++] = ((chunkcntr & 0xFF) |  // Chunk counter = StreamID
                            (chunk_datasz & 0xF00) |
                            ((chunk_datasz & 0x0FF) << 16) |
                            ((chunkcntr & 0xFF) << 24));
      else
        emudata[index++] = (0xAA |
                            (chunk_datasz & 0xF00) |
                            ((chunk_datasz & 0x0FF) << 16) |
                            ((chunkcntr & 0xFF) << 24));
      emudata[index++] = 0x10AABB00; // ewidth=0x10=16 bits

      // Add chunk data according to 'pattern_id'
      if( pattern_id == 1 )
        for( i=0; i<chunk_datasz/4; ++i )
          emudata[index++] = 0xAA55AA55;
      else if( pattern_id == 2 )
        for( i=0; i<chunk_datasz/4; ++i )
          emudata[index++] = 0xFFFFFFFF;
      else if( pattern_id == 3 )
        for( i=0; i<chunk_datasz/4; ++i )
          emudata[index++] = 0x00000000;
      else
        {
          unsigned int cntr = 0;
          for( i=0; i<chunk_datasz/4; ++i, cntr+=4 )
            emudata[index++] = ((((cntr + 3) & 0xFF) << 24) |
                                (((cntr + 2) & 0xFF) << 16) |
                                (((cntr + 1) & 0xFF) <<  8) |
                                (((cntr + 0) & 0xFF) <<  0));
        }

      // EOP (+ 20-bits CRC)
      uint64_t crc = crc20( &emudata[index-chunksz/4], chunksz/4, crc_new );
      if( add_crc_err && chunkcntr == 3 ) ++crc; // For testing
      emudata[index++] = FM_KCHAR_EOP | (crc << 8);
      if( omit_one_eoc && chunkcntr == 2 ) --index; // For testing

      if( add_busy && chunkcntr == 0 )
        emudata[index++] = FM_KCHAR_SOB;

      // A configurable number of comma symbols in between chunks
      for( i=0; i<idle_chars; ++i )
        emudata[index++] = FM_KCHAR_IDLE;

      if( add_busy && chunkcntr == 0 )
        emudata[index++] = FM_KCHAR_EOB;

      ++chunkcntr;
    }

  // Fill any remaining uninitialised array locations with IDLE symbols
  for( ; index<emusize; ++index )
    emudata[index] = FM_KCHAR_IDLE;

  // We expect to have generated max_chunkcnt chunks!
  if( chunkcntr < max_chunkcnt )
    {
      success = false;
      _errorId = 2;
    }

  return success;
}

// ----------------------------------------------------------------------------

bool EmuDataGenerator::generateFmDune( uint64_t *emudata,
                                       uint64_t  emusize,
                                       uint32_t *req_chunksize,
                                       uint32_t  idle_chars )
{
  // Initialize emudata to all zeroes
  uint32_t i;
  for( i=0; i<emusize; ++i ) emudata[i] = 0;

  uint32_t chunksz = PROTODUNE_FRAMESIZE;
  *req_chunksize = chunksz;

  // Determine the number of chunks that will fit
  // (chunk size includes 8-byte header): 2 IDLEs, SOP, chunk, EOP
  uint32_t max_chunkcnt = (emusize-2)/(1 + chunksz/4 + 1 + idle_chars);
  uint32_t index = 0;
  bool     success = true;
  _chunkCount = max_chunkcnt;
  
  // Adjust to the number of ProtoDUNE data chunks provided
  if( max_chunkcnt > sizeof(PROTODUNE_DATA)/(sizeof(uint64_t)*chunksz/4) )
    {
      max_chunkcnt = sizeof(PROTODUNE_DATA)/(sizeof(uint64_t)*chunksz/4);
      _chunkCount = max_chunkcnt;
    }
 
  // Start with some IDLE symbols
  emudata[index++] = FM_KCHAR_IDLE;
  emudata[index++] = FM_KCHAR_IDLE;

  // Multiple chunks (frames)
  uint32_t next_index, data_index = 0, chunkcntr = 0;
  while( index < emusize && chunkcntr < max_chunkcnt )
    {
      // Check if the next frame will fit
      next_index = index + (1 + chunksz/4 + 1);
      if( next_index >= emusize )
        {
          // It won't fit, so forget it: from here onwards fill with IDLEs
          for( ; index<emusize; ++index )
            emudata[index] = FM_KCHAR_IDLE;
          // Should exit the while-loop on the basis of the chunk counter
          // so we consider this an error...
          success = false; _errorId = 1;
          continue; // Jump to start of while-loop
        }

      // SOP
      emudata[index++] = FM_KCHAR_SOP;

      // Add frame data
      for( unsigned int i=0; i<chunksz/4; ++i )
        emudata[index++] = PROTODUNE_DATA[data_index++];

      // EOP
      emudata[index++] = FM_KCHAR_EOP;

      // A configurable number of comma symbols in between frames
      for( i=0; i<idle_chars; ++i )
        emudata[index++] = FM_KCHAR_IDLE;

      ++chunkcntr;
    }

  // Fill any remaining uninitialised array locations with IDLE symbols
  for( ; index<emusize; ++index )
    emudata[index] = FM_KCHAR_IDLE;

  // We expect to have generated max_chunkcnt chunks!
  if( chunkcntr < max_chunkcnt )
    {
      success = false;
      _errorId = 2;
    }

  return success;
}

// ----------------------------------------------------------------------------

#define CRC_WIDTH      20
#define CRC_MASK       ((1 << CRC_WIDTH) - 1)
#define CRC_POLYNOM_1  0xC1ACF
#define CRC_POLYNOM_2  0x8359F
#define CRC_INITVAL    0xFFFFF

uint64_t EmuDataGenerator::crc20( uint64_t *data,
                                  uint64_t  length,
                                  bool      crc_new )
{
  // Initialize
  uint64_t crc = CRC_INITVAL;
  uint64_t polynomial;
  if( crc_new )
    polynomial = CRC_POLYNOM_2;
  else
    polynomial = CRC_POLYNOM_1;

  unsigned int i, k;
  for( k=0; k<CRC_WIDTH; ++k )
    {
      if( (crc & 1) )
        crc = (crc >> 1) ^ ((1 << (CRC_WIDTH-1)) | (polynomial >> 1));
      else
        crc = (crc >> 1);
   }

  // Calculate CRC
  for( i=0; i<length; ++i )
    {
      for( k=1; k<=32; ++k )
        {
          if( crc & (1 << (CRC_WIDTH-1)) )
            crc = ((crc << 1) | ((data[i] >> (32-k)) & 1)) ^ polynomial;
          else
            crc = ((crc << 1) | ((data[i] >> (32-k)) & 1));
        }
      crc &= CRC_MASK;
    }

  // One more loop
  for( k=0; k<CRC_WIDTH; ++k )
    {
      if( crc & (1 << (CRC_WIDTH-1)) )
        crc = (crc << 1) ^ polynomial;
      else
        crc = (crc << 1);
    }
  crc &= CRC_MASK;

  return crc;
}

// ----------------------------------------------------------------------------
