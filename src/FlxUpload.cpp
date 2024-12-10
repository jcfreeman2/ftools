#include <string.h> // for memset()
#include <unistd.h> // for usleep()
#include <time.h>   // for nanosleep()

#include "FlxUpload.h"
#include "flxcard/FlxException.h"
#include "EmuDataGenerator.h"
#include "crc.h"

//#define USE_LIB_CMEM

#ifndef USE_LIB_CMEM
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#endif

#include <iostream>
#include <iomanip>
#include <sstream> // for ostringstream
#include <fstream> // for ifstream
using namespace std;

#include "nlohmann/json.hpp"
using namespace nlohmann;
#include "yaml-cpp/yaml.h"

// ----------------------------------------------------------------------------

FlxUpload::FlxUpload( int card_nr,
                      i64 buffer_size,
                      int dma_index,
                      int hdlc_seq_nr )
  : _flx( 0 ),
    _cardNr( card_nr ),
    _blockSize( -1 ),
    _bar2( 0 ),
    _has5BitFromHostLength( false ),
    _bufferSize( 0 ),
    _dataBuffer( 0 ),
    _cmemHandle( -1 ),
    _cmemStartAddr( 0 ),
    _cmemEndAddr( 0 ),
    _dmaIndex( dma_index ),
    _dmaCircular( false ),
    _dmaTlp( 0 ),
    _dmaSoftwPtr( 0 ),
    _bar0( 0 ),
    _dmaDesc( 0 ),
    _dmaStat( 0 ),
    _bytesToUpload( 0 ),
    //_uploadDmaSize( 8*1024 ), // Do not exceed 16KB -> data corruption
    // Corruption at > 1024 (10 Oct 2017)
    _uploadDmaSize( 1024 ),
    _uploadIndex( 0 ),
    _writeIndex( 0 ),
    _timestamp( 0 ),
    _fillerByte( 0 ),
    _hdlcSeqNr( hdlc_seq_nr ),
    _pbChunkCntr( 0 ),
    _pbBlockCntr( 0 ),
    _pbRandomData( false )
{
  // A buffer size of 0 also selects the default size
  if( buffer_size == 0 ) buffer_size = DFLT_SEND_BUF_SIZE;

  // Allocate a (continuous) buffer of the requested size
  _dataBuffer = this->cmemAllocate( buffer_size, card_nr );

  if( _dataBuffer )
    {
      // Initialize the buffer
      memset( static_cast<void *> (_dataBuffer), 0xFF, buffer_size );
      _bufferSize = buffer_size;
    }
  else
    {
      if( !_errString.empty() ) _errString += "; ";
      _errString += "Failed to allocate requested buffer size";
    }

  // Open FELIX FLX-card
  _flx = new FlxCard;
  try {
    uint32_t lockbits = LOCK_NONE;
    // Backwards compatibility
    if( dma_index == 0 )
      lockbits |= LOCK_DMA0;
    else if( dma_index == 1 )
      lockbits |= LOCK_DMA1;
    // New lockbits
    if( dma_index > -1 )
      lockbits |= ((1 << dma_index) << 16);

    _flx->card_open( _cardNr, lockbits );

    // Find DMA index to use?
    if( dma_index == -1 )
      {
        // DMA controller index for FromHost data
        dma_index = _flx->cfg_get_option(BF_GENERIC_CONSTANTS_DESCRIPTORS)-1;
        lockbits |= ((1 << dma_index) << 16); // New lockbit
        // Backwards compatibility
        if( dma_index == 0 )
          lockbits |= LOCK_DMA0;
        else if( dma_index == 1 )
          lockbits |= LOCK_DMA1;

        // Reopen device with correct lockbits
        _flx->card_close();
        _flx->card_open( _cardNr, lockbits );
      }
    _dmaIndex = dma_index;
  }
  catch( FlxException &ex ) {
    if( !_errString.empty() ) _errString += "; ";
    _errString += "FlxCard open: ";
    _errString += ex.what();
    delete _flx;
    _flx = 0;
    return;
  }

  // Get a pointer to the BAR2 registers
  u64 bar2_addr = _flx->bar2Address();
  _bar2 = (flxcard_bar2_regs_t *) bar2_addr;

  // Get a pointer to the BAR0 registers
  u64 bar0_addr = _flx->bar0Address();
  _bar0 = (volatile flxcard_bar0_regs_t *) bar0_addr;
  _dmaDesc = &_bar0->DMA_DESC[dma_index];
  _dmaStat = &_bar0->DMA_DESC_STATUS[dma_index];

  this->dmaStop();
  // Force circular DMA inactive state initially
  // to prevent hanging in upload(): dmaActive()
  _dmaSoftwPtr = _dmaStat->current_address;

  // Get this firmware's to-host data blocksize and trailer format
  _blockSize = _bar2->BLOCKSIZE;
  if( _blockSize == 0 ) // Backwards compatibility!
    _blockSize = 1024;
  _blockSizeCode  = (_blockSize/1024-1) << 8;
  _trailerFmt.setTrailerIs32Bit( (_bar2->CHUNK_TRAILER_32B == 1) );

#if REGMAP_VERSION >= 0x500
  _has5BitFromHostLength = (_bar2->FROMHOST_LENGTH_IS_5BIT == 1);
#endif // REGMAP_VERSION

  _minimumUsleep = this->minimumUsleep();
}

// ----------------------------------------------------------------------------

FlxUpload::~FlxUpload()
{
  if( _pbFile.is_open() ) _pbFile.close();

  // Close the FLX-card
  try {
    if( _flx )
      _flx->card_close();
  }
  catch( FlxException &ex ) {
    // Do something..?
  }
  delete _flx;

  // Free the receive buffer and close CMEM(_RCC)
#ifdef USE_LIB_CMEM
  int ret = CMEM_SegmentFree( _cmemHandle );
  if( ret == CMEM_RCC_SUCCESS )
    ret = CMEM_Close();
  if( ret != CMEM_RCC_SUCCESS )
    rcc_error_print( stdout, ret );

#else
  if( _dataBuffer )
    {
      munmap( (void *) _cmemDescriptor.uaddr, _cmemDescriptor.size );
      ioctl( _cmemHandle, CMEM_RCC_FREE, &_cmemDescriptor.handle );
      close( _cmemHandle );
    }
#endif // USE_LIB_CMEM
}

// ----------------------------------------------------------------------------

void FlxUpload::stop()
{
  this->dmaStop();
}

// ----------------------------------------------------------------------------

std::string FlxUpload::errorString()
{
  if( _errString.empty() )
    return std::string( "" );

  std::ostringstream oss;
  oss << "FlxUpload#" << _cardNr << ": " << _errString;

  // Clear the error string
  _errString.clear();

  return oss.str();
}

// ----------------------------------------------------------------------------

bool FlxUpload::hasError()
{
  return !_errString.empty();
}

// ----------------------------------------------------------------------------

std::string FlxUpload::debugString()
{
  std::ostringstream oss;
  oss << "FlxUpload" << _cardNr << " DEBUG: "
      << std::hex << setfill('0')
      << "start=" << std::setw(10) << _cmemStartAddr
      << ", end=" << std::setw(10) << _cmemEndAddr
      << ", pread=" << std::setw(10) << _dmaDesc->read_ptr
      << ", pwrite=" << std::setw(10) << _dmaStat->current_address
      << ", even-PC/DMA " << _dmaStat->even_addr_pc
      << std::dec << " " << _dmaStat->even_addr_dma
      << std::endl;
    return oss.str();
}

// ----------------------------------------------------------------------------

void FlxUpload::setFanOutForDaq()
{
  if( _flx == 0 ) return;

  // (Set "downlink fanout select")
  bool locked = this->fanOutIsLocked();
  if( !locked )
    {
      _flx->cfg_set_option( BF_GBT_TOFRONTEND_FANOUT_SEL, 0 );
      _flx->cfg_set_option( BF_GBT_TOHOST_FANOUT_SEL, 0 );
    }
}

// ----------------------------------------------------------------------------

bool FlxUpload::fanOutIsLocked()
{
  if( _flx == 0 ) return false;

  // Check for the FANOUT_LOCK bit(s)
  bool locked = false;
  if( _flx->cfg_get_option( BF_GBT_TOHOST_FANOUT_LOCK ) == 1 )
    locked = true;
  if( _flx->cfg_get_option( BF_GBT_TOFRONTEND_FANOUT_LOCK ) == 1 )
    locked = true;
  return locked;
}

// ----------------------------------------------------------------------------

int FlxUpload::prepareData( int link_count, int *elink,
                            int size, int pattern_id, int repeat )
{
  for( int i=0; i<link_count; ++i )
    elink[i] = mapElink( elink[i] );
  // Create the test data bytes according to a pattern identifier
  // to be uploaded to 'link_count' E-links defined in array 'elink'
  char data[256];
  int  i, j;
  if( pattern_id == 1 )
    {
      // 0x55-0xAA pattern
      for( i=0; i<256; ++i )
        if( i & 1 )
          data[i] = (char) 0xAA;
        else
          data[i] = (char) 0x55;
    }
  else if( pattern_id == 2 )
    {
      // 0xFF only
      for( i=0; i<256; ++i )
        data[i] = (char) 0xFF;
    }
  else if( pattern_id == 3 )
    {
      // See below..
    }
  else
    {
      // Incrementing pattern
      for( i=0; i<256; ++i )
        data[i] = (char) (i & 0xFF);
    }

  // Format and write 'size' bytes of test data to the DMA buffer,
  // for each E-link in turn, and this 'repeat' times
  int cnt, lnk, blocks = 0, data_i = 0;
  i64 buf_i = _writeIndex;
  for( cnt=0; cnt<repeat; ++cnt )
    {
      for( lnk=0; lnk<link_count; ++lnk )
        {
          int hdr, final_bytes, final_size, final_hdr;
          blocks = (size + TOGBT_PAYLOAD_BYTES-1) / TOGBT_PAYLOAD_BYTES;
          final_bytes = size - (blocks-1) * TOGBT_PAYLOAD_BYTES;
          if( _has5BitFromHostLength )
            {
              hdr = ((elink[lnk] << TOGBT_ELINK_SHIFT) | 0x1F);
              final_hdr  = hdr & ~TOGBT_LENGTH_MASK_5BIT;
              final_hdr |= final_bytes; // Length in bytes
            }
          else
            {
              hdr = ((elink[lnk] << TOGBT_ELINK_SHIFT) |
                     ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT));
              final_size = (final_bytes + 1) / 2; // Length in 2-byte units
              final_hdr  = hdr & ~TOGBT_LENGTH_MASK;
              final_hdr |= ((final_size << TOGBT_LENGTH_SHIFT)|TOGBT_EOM_MASK);
            }

          data_i = 0;

          // Fully-filled To-GBT byte blocks
          for( i=0; i<blocks-1; ++i )
            {
              if( pattern_id == 3 )
                {
                  // Fill the block with a constant,
                  // incrementing the constant for each subsequent chunk
                  for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                    _dataBuffer[buf_i] = (char) (cnt & 0xFF);
                }
              else
                {
                  for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                    _dataBuffer[buf_i] = data[(data_i + j) & 0xFF];
                }
              data_i += TOGBT_PAYLOAD_BYTES;
              _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
              _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
              if( buf_i >= _bufferSize ) buf_i = 0;
            }

          // Final (possibly not fully filled) To-GBT byte block
          for( j=TOGBT_PAYLOAD_BYTES-1; j>=final_bytes; --j, ++buf_i )
            _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
          if( pattern_id == 3 )
            {
              // Final data bytes
              for( j=final_bytes-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = (char) (cnt & 0xFF);
            }
          else
            {
              // Final data bytes
              for( j=final_bytes-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = data[(data_i + j) & 0xFF];
            }
          _dataBuffer[buf_i++] = (char) (final_hdr & 0xFF);
          _dataBuffer[buf_i++] = (char) ((final_hdr & 0xFF00) >> 8);
          if( buf_i >= _bufferSize ) buf_i = 0;
        }
    }

  _bytesToUpload = repeat * link_count * blocks * TOGBT_BLOCK_BYTES;
  _writeIndex = buf_i;
  //_uploadIndex = 0;

  return repeat * link_count * blocks * TOGBT_BLOCK_BYTES;
}

// ----------------------------------------------------------------------------

int FlxUpload::prepareData( int elink, int size, int pattern_id, int repeat )
{
  elink = mapElink( elink );
  // Create the test data bytes according to a pattern identifier
  // to be uploaded to the E-link defined by the given parameters
  char data[256];
  int  i, j;
  if( pattern_id == 1 )
    {
      // 0x55-0xAA pattern
      for( i=0; i<256; ++i )
        if( i & 1 )
          data[i] = (char) 0xAA;
        else
          data[i] = (char) 0x55;
    }
  else if( pattern_id == 2 )
    {
      // 0xFF only
      for( i=0; i<256; ++i )
        data[i] = (char) 0xFF;
    }
  else if( pattern_id == 3 )
    {
      // See below..
    }
  else if( (pattern_id & 0xFF) == 4 )
    {
      // Given byte value (2nd byte of 'pattern_id') only
      char byt = (char) ((pattern_id >> 8) & 0xFF);
      for( i=0; i<256; ++i )
        data[i] = byt;
    }
  else
    {
      // Incrementing pattern
      for( i=0; i<256; ++i )
        data[i] = (char) (i & 0xFF);
    }

  // Format and write 'size' bytes of test data to the DMA buffer,
  // 'repeat' times
  int hdr, blocks, final_bytes, final_size, final_hdr;
  blocks = (size + TOGBT_PAYLOAD_BYTES-1) / TOGBT_PAYLOAD_BYTES;
  final_bytes = size - (blocks-1) * TOGBT_PAYLOAD_BYTES;
  if( _has5BitFromHostLength )
    {
      hdr = ((elink << TOGBT_ELINK_SHIFT) | 0x1F);
      final_hdr  = hdr & ~TOGBT_LENGTH_MASK_5BIT;
      final_hdr |= final_bytes; // Length in bytes
    }
  else
    {
      hdr = ((elink << TOGBT_ELINK_SHIFT) |
             ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT));
      final_size = (final_bytes + 1) / 2; // Length in 2-byte units
      final_hdr  = hdr & ~TOGBT_LENGTH_MASK;
      final_hdr |= ((final_size << TOGBT_LENGTH_SHIFT) | TOGBT_EOM_MASK);
    }

  int cnt, data_i;
  i64 buf_i = _writeIndex;
  //std::cout << hex << "writeIndex=" << _writeIndex << dec << endl;
  for( cnt=0; cnt<repeat; ++cnt )
    {
      data_i = 0;

      // Fully-filled To-GBT byte blocks
      for( i=0; i<blocks-1; ++i )
        {
          if( pattern_id == 3 )
            {
              // Fill the block with a constant,
              // incrementing the constant for each subsequent chunk
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = (char) (cnt & 0xFF);
            }
          else
            {
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = data[(data_i + j) & 0xFF];
            }
          data_i += TOGBT_PAYLOAD_BYTES;
          _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
          _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);

          /* Inverted byte order:
          _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
          _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
          for( j=0; j<TOGBT_PAYLOAD_BYTES; ++j, ++buf_i, ++data_i )
            _dataBuffer[buf_i] = data[data_i & 0xFF];
          */

          if( buf_i >= _bufferSize ) buf_i = 0;
        }

      // Final (possibly not fully filled) To-GBT byte block
      //for( j=TOGBT_PAYLOAD_BYTES-1; j>=final_size*2; --j, ++buf_i )
      for( j=TOGBT_PAYLOAD_BYTES-1; j>=final_bytes; --j, ++buf_i )
        _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
      if( pattern_id == 3 )
        {
          for( j=final_bytes-1; j>=0; --j, ++buf_i )
            _dataBuffer[buf_i] = (char) (cnt & 0xFF); // Final data bytes
        }
      else
        {
          for( j=final_bytes-1; j>=0; --j, ++buf_i )
            _dataBuffer[buf_i] = data[(data_i + j) & 0xFF]; // Final data bytes
        }
      _dataBuffer[buf_i++] = (char) (final_hdr & 0xFF);
      _dataBuffer[buf_i++] = (char) ((final_hdr & 0xFF00) >> 8);

      /* Inverted byte order:
      _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
      _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
      for( j=0; j<size*2; ++j, ++buf_i, ++data_i )
        _dataBuffer[buf_i] = data[data_i & 0xFF]; // Final data bytes
      for( ; j<TOGBT_PAYLOAD_BYTES; ++j, ++buf_i )
        _dataBuffer[buf_i] = (char) 0xFF; // Remaining block bytes: 0xFF
      */

      if( buf_i >= _bufferSize ) buf_i = 0;
    }

  // Start DMA (now done in upload())
  //this->dmaStart( 0, repeat*blocks*TOGBT_BLOCK_BYTES );

  _bytesToUpload = repeat * blocks * TOGBT_BLOCK_BYTES;
  _writeIndex = buf_i;
  //_writeIndex = 0;
  //_uploadIndex = 0;

  return repeat * blocks * TOGBT_BLOCK_BYTES;
}

// ----------------------------------------------------------------------------

int FlxUpload::prepareData( int elink,
                            const std::string &filename,
                            int repeat, bool binary, bool display )
{
  elink = mapElink( elink );
  std::ifstream file( filename );
  if( !file.is_open() )
    {
      _errString = "Failed to open file: ";
      _errString += filename;
      cout << "###" << this->errorString() << endl;
      return -1; // Something went wrong
    }
  cout << "Opened file " << filename << endl;
  _delays.clear();

  // Storage for the number of bytes to read from the file at a time
  char data[1024*TOGBT_PAYLOAD_BYTES];
  i64 shorts, bytes_read = 0, total_bytes_read = 0, blocks_to_upload = 0;
  int i, j, blocks, hdr;
  int data_i;
  int eom_i = -1;
  int line_nr = 0;
  i64 buf_i = _writeIndex;
  if( _has5BitFromHostLength )
    hdr = ((elink << TOGBT_ELINK_SHIFT) | 0x1F);
  else
    hdr = ((elink << TOGBT_ELINK_SHIFT) |
           ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT));
  if( binary )
    {
      file.read( data, sizeof(data) );
      bytes_read = file.gcount();
      while( bytes_read > 0 )
        {
          total_bytes_read += bytes_read;

          // Format and write data bytes to the DMA buffer
          if( _has5BitFromHostLength )
            {
              hdr &= ~TOGBT_LENGTH_MASK_5BIT;
              hdr |= 0x1F;
            }
          else
            {
              hdr &= ~TOGBT_LENGTH_MASK;
              hdr |= ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT);
            }
          blocks = (bytes_read + TOGBT_PAYLOAD_BYTES-1) / TOGBT_PAYLOAD_BYTES;
          blocks_to_upload += blocks;
          data_i = 0;

          // Is it going to fit?
          if( blocks_to_upload*TOGBT_PAYLOAD_BYTES > (_bufferSize-_writeIndex) )
            {
              std::ostringstream oss;
              oss << "Data (" << blocks_to_upload
                  << "*" << TOGBT_PAYLOAD_BYTES
                  << " bytes) does not fit buffer (size "
                  << _bufferSize << ")";
              _errString = oss.str();
              cout << "###" << this->errorString() << endl;
              return -1;
            }

          // Fully filled To-GBT byte blocks
          for( i=0; i<blocks-1; ++i )
            {
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = data[data_i + j];
              data_i += TOGBT_PAYLOAD_BYTES;
              _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
              _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
            }

          // Final (possibly not fully filled) To-GBT byte block
          eom_i = buf_i; // Remember this index,
                         // as it may become the final header
                         // NB: ###does not work for 5-bit length:
                         //     <30bytes is automatically considered to be EoM
          bytes_read -= (blocks-1) * TOGBT_PAYLOAD_BYTES;
          shorts      = (bytes_read + 1) / 2; // Length in 2-byte units
          if( _has5BitFromHostLength )
            {
              hdr &= ~TOGBT_LENGTH_MASK_5BIT;
              hdr |= bytes_read;
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=bytes_read; --j, ++buf_i )
                _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
              for( j=bytes_read-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = data[data_i + j]; // Final data bytes
            }
          else
            {
              hdr &= ~TOGBT_LENGTH_MASK;
              hdr |= (shorts << TOGBT_LENGTH_SHIFT);
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=shorts*2; --j, ++buf_i )
                _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
              for( j=shorts*2-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = data[data_i + j]; // Final data bytes
            }
          _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
          _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);

          file.read( data, sizeof(data) );
          bytes_read = file.gcount();
        }
      // Set End-Of-Message bit in the final header
      if( eom_i >= 0 && !_has5BitFromHostLength)
        _dataBuffer[eom_i] |= TOGBT_EOM_MASK;
    }
  else
    {
      // Text file: one line per data chunk
      std::string line;
      if( display )
        cout << "Line: Data" << endl
             << "----------" << endl;
      while( !file.eof() )
        {
          std::getline( file, line );
          if( line.empty() )
            {
              if( !file.eof() )
                ++line_nr;
              // Next line
              continue;
            }
          ++line_nr;

          // Remove leading white-space
          size_t startpos = line.find_first_not_of( " \t" );
          if( startpos != string::npos && startpos != 0 )
            line = line.substr( startpos );

          std::istringstream iss;
          int val, count = 0;
          char byteval = 0;
          if( line[0] == '#' )
            {
              // Comment line: skip it, next line
              continue;
            }
          else if( line[0] == '+' )
            {
              // Multi-byte marker:
              // two numbers in such a line mean: bytevalue + bytecount
              // (Add a chunk of size 'bytecount' with each byte='byteval')
              line = line.substr( 1 );
              iss.str( line );
              iss >> std::hex >> val; // Byte value: hexadecimal
              if( iss.fail() )
                {
                  cout << "### Line " << line_nr
                       << ", '+' syntax" << endl;
                }
              else if( val < 0 || val > 255 )
                {
                  cout << "### Line " << line_nr
                       << ", '+' value range err" << endl;
                }
              else
                {
                  iss >> std::dec >> count; // Byte count: decimal
                  if( iss.fail() )
                    cout << "### Line " << line_nr
                         << ", '+' syntax" << endl;
                  else if( count < 0 )
                    cout << "### Line " << line_nr
                         << ", '+' count range err" << endl;
                }
              byteval = (char) (val & 0xFF);
              bytes_read = count;

              if( display )
                cout << setw(3) << line_nr << ": '+' = 0x" << hex
                     << (unsigned int) byteval << ", "
                     << dec << count << " bytes" << endl;
            }
          else if( line[0] == '&' )
            {
              // Delay marker: followed by the number of microseconds to delay
              line = line.substr( 1 );
              iss.str( line );
              iss >> val; // Delay value: decimal
              if( iss.fail() )
                {
                  cout << "### Line " << line_nr
                       << ", '&' syntax" << endl;
                }
              else if( val <= 0 )
                {
                  cout << "### Line " << line_nr
                       << ", '&' value range err" << endl;
                }
              else
                {
                  delay_t d;
                  // This is the data block index at which to insert the delay
                  d.index = _writeIndex + blocks_to_upload * TOGBT_BLOCK_BYTES;
                  // Length of the delay in microseconds
                  d.delay_us = val;
                  _delays.push_back( d );

                  if( display )
                    cout << setw(3) << line_nr << ": "
                         << "delay " << d.delay_us
                         << " @ index " << d.index
                         << hex << " (" << d.index << ")" << dec << endl;
                }
              // Skip the rest of this line
              continue;
            }
          else if( line[0] == '>' )
            {
              // Change the E-link number to upload to!
              line = line.substr( 1 );
              iss.str( line );
              iss >> std::hex >> val; // E-link number: hexadecimal
              if( iss.fail() )
                {
                  cout << "### Line " << line_nr
                       << ", '>' syntax" << endl;
                }
              else if( val < 0 || val > FLX_MAX_ELINK_NR )
                {
                  cout << "### Line " << line_nr
                       << ", '>' value range err" << endl;
                }
              else
                {
                  elink = (val & BLOCK_ELINK_MASK) >> BLOCK_ELINK_SHIFT;
                  if( _has5BitFromHostLength )
                    hdr = ((elink << TOGBT_ELINK_SHIFT) | 0x1F);
                  else
                    hdr = ((elink << TOGBT_ELINK_SHIFT) |
                           ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT));

                  if( display )
                    cout << setw(3) << line_nr << ": "
                         << "E-link = " << hex << val << dec << endl;
                }
              // Skip the rest of this line
              continue;
            }
          else
            {
              // Line with data chunk byte values
              int cnt = 1; // Entry counter
              unsigned int val;
              iss.str( line );
              iss >> std::hex; // Byte values: hexadecimal
              while( !iss.eof() )
                {
                  // Next value (or end-of-file/line)
                  iss >> val;
                  if( !iss.fail() )
                    {
                      if( val >= 0 && val <= 255 )
                        {
                          data[bytes_read] = (char) (val & 0xFF);
                          ++bytes_read;
                        }
                      else
                        {
                          cout << "###Line " << line_nr
                               << ", value out-of-range [0..0xFF]: 0x"
                               << hex << val << dec << endl;
                        }
                    }
                  else if( !iss.eof() )
                    {
                      std::string s;
                      iss.clear();
                      iss >> s; // Skip past the illegal entry
                      cout << "###Line " << line_nr
                           << ", illegal entry #" << cnt << endl;
                    }
                  ++cnt;
                }

              if( display )
                cout << setw(3) << line_nr << ": "
                     << bytes_read << " bytes" << endl;
            }

          if( bytes_read <= 0 )
            {
              // Nothing to upload, go to next line
              bytes_read = 0;
              continue;
            }
          total_bytes_read += bytes_read;

          // Format and write data bytes to the DMA buffer
          if( _has5BitFromHostLength )
            {
              hdr &= ~TOGBT_LENGTH_MASK_5BIT;
              hdr |= 0x1F;
            }
          else
            {
              hdr &= ~(TOGBT_LENGTH_MASK | TOGBT_EOM_MASK);
              hdr |= ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT);
            }
          blocks = (bytes_read + TOGBT_PAYLOAD_BYTES-1) / TOGBT_PAYLOAD_BYTES;
          blocks_to_upload += blocks;
          data_i = 0;

          // Is it going to fit?
          if( blocks_to_upload*TOGBT_PAYLOAD_BYTES > (_bufferSize-_writeIndex) )
            {
              std::ostringstream oss;
              oss << "Data (" << blocks_to_upload
                  << "*" << TOGBT_PAYLOAD_BYTES
                  << " bytes) does not fit buffer (size "
                  << _bufferSize << ")";
              _errString = oss.str();
              cout << "###" << this->errorString() << endl;
              return -1;
            }

          // Fully filled To-GBT byte blocks
          for( i=0; i<blocks-1; ++i )
            {
              if( count > 0 )
                // Fill with a constante byte value
                for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                  _dataBuffer[buf_i] = byteval;
              else
                for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                  _dataBuffer[buf_i] = data[data_i + j];
              data_i += TOGBT_PAYLOAD_BYTES;
              _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
              _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
            }

          // Final (possibly not fully filled) To-GBT byte block
          bytes_read -= (blocks-1) * TOGBT_PAYLOAD_BYTES;
          shorts      = (bytes_read + 1) / 2; // Length in 2-byte units
          if( _has5BitFromHostLength )
            {
              hdr &= ~TOGBT_LENGTH_MASK_5BIT;
              hdr |= bytes_read;
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=bytes_read; --j, ++buf_i )
                _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
              if( count > 0 )
                // Fill with a constante byte value
                for( j=bytes_read-1; j>=0; --j, ++buf_i )
                  _dataBuffer[buf_i] = byteval; // Final data bytes
              else
                for( j=bytes_read-1; j>=0; --j, ++buf_i )
                  _dataBuffer[buf_i] = data[data_i + j]; // Final data bytes
            }
          else
            {
              hdr &= ~TOGBT_LENGTH_MASK;
              hdr |= (shorts << TOGBT_LENGTH_SHIFT) | TOGBT_EOM_MASK;
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=shorts*2; --j, ++buf_i )
                _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
              if( count > 0 )
                // Fill with a constante byte value
                for( j=shorts*2-1; j>=0; --j, ++buf_i )
                  _dataBuffer[buf_i] = byteval; // Final data bytes
              else
                for( j=shorts*2-1; j>=0; --j, ++buf_i )
                  _dataBuffer[buf_i] = data[data_i + j]; // Final data bytes
            }
          _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
          _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);

          // Next line
          bytes_read = 0;
        }
    }

  file.close();
  cout << "Bytes read: " << total_bytes_read;
  if( !binary )
    cout << ", lines: " << line_nr << ", delays: " << _delays.size();
  cout << endl;

  if( bytes_read == 0 )
    {
      // Will it fit ?
      _bytesToUpload = repeat * blocks_to_upload * TOGBT_BLOCK_BYTES;
      if( _bytesToUpload > (_bufferSize - _writeIndex) )
        {
          std::ostringstream oss;
          oss << "Data (" << repeat
              << "*" << blocks_to_upload
              << "*" << TOGBT_PAYLOAD_BYTES
              << " bytes) does not fit buffer (size "
              << _bufferSize << ")";
          _errString = oss.str();
          cout << "###" << this->errorString() << endl;
          return -1;
        }

      // Repeat data 'repeat' times
      --repeat;
      for( int cnt=0; cnt<repeat; ++cnt )
        {
          memcpy( (void *) &_dataBuffer[buf_i], (void *) _dataBuffer,
                  blocks_to_upload * TOGBT_BLOCK_BYTES );
          buf_i += blocks_to_upload * TOGBT_BLOCK_BYTES;
        }
      if( !_delays.empty() && repeat > 0 )
        {
          // Also repeat any defined delays
          int sz = _delays.size();
          delay_t d;
          std::list<delay_t>::iterator it;
          for( int cnt=0; cnt<repeat; ++cnt )
            {
              it = _delays.begin();
              for( i=0; i<sz; ++i,++it )
                {
                  d = *it;
                  // Adjust the index to point into the repeated data block
                  d.index += (cnt+1) * blocks_to_upload * TOGBT_BLOCK_BYTES;
                  // Append the delay info to the list
                  _delays.push_back( d );
                }
            }
        }

      // DMA is started in upload()
      _writeIndex = buf_i;
      //_uploadIndex  = 0;

      return _bytesToUpload;
    }
  return -1; // Something went wrong
}

// ----------------------------------------------------------------------------

int FlxUpload::prepareData( int elink, int size, u8 *data )
{
  elink = mapElink( elink );
  // Format and write 'size' bytes from 'data' to the DMA buffer
  int hdr, blocks, final_bytes, final_size, final_hdr;
  blocks = (size + TOGBT_PAYLOAD_BYTES-1) / TOGBT_PAYLOAD_BYTES;
  final_bytes = size - (blocks-1) * TOGBT_PAYLOAD_BYTES;
  if( _has5BitFromHostLength )
    {
      hdr = ((elink << TOGBT_ELINK_SHIFT) | 0x1F);
      final_hdr  = hdr & ~TOGBT_LENGTH_MASK_5BIT;
      final_hdr |= final_bytes; // Length in bytes
    }
  else
    {
      hdr = ((elink << TOGBT_ELINK_SHIFT) |
             ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT));
      final_size = (final_bytes + 1) / 2; // Length in 2-byte units
      final_hdr  = hdr & ~TOGBT_LENGTH_MASK;
      final_hdr |= ((final_size << TOGBT_LENGTH_SHIFT) | TOGBT_EOM_MASK);
    }

  int i, j, data_i = 0;
  i64 buf_i = _writeIndex;

  // Fully-filled To-GBT byte blocks
  for( i=0; i<blocks-1; ++i )
    {
      for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
        _dataBuffer[buf_i] = data[data_i + j];
      data_i += TOGBT_PAYLOAD_BYTES;
      _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
      _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
      if( buf_i >= _bufferSize ) buf_i = 0;
    }

  // Final (possibly not fully filled) To-GBT byte block
  for( j=TOGBT_PAYLOAD_BYTES-1; j>=final_bytes; --j, ++buf_i )
    _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
  for( j=final_bytes-1; j>=0; --j, ++buf_i )
    _dataBuffer[buf_i] = data[data_i + j]; // Final data bytes
  _dataBuffer[buf_i++] = (char) (final_hdr & 0xFF);
  _dataBuffer[buf_i++] = (char) ((final_hdr & 0xFF00) >> 8);
  if( buf_i >= _bufferSize ) buf_i = 0;

  //std::cout << std::hex;
  //for( int i=0; i<blocks*TOGBT_BLOCK_BYTES; ++i )
  //  std::cout << ((int) _dataBuffer[_writeIndex+i] & 0xFF) << " ";
  //std::cout << std::endl;

  // DMA is started in upload()
  _bytesToUpload = blocks * TOGBT_BLOCK_BYTES;
  _writeIndex = buf_i;
  //_uploadIndex = 0;

  return _bytesToUpload;
}

// ----------------------------------------------------------------------------

int FlxUpload::prepareData( int elink, std::vector<sca_msg_t> &msgs )
{
  elink = mapElink( elink );
  // Format and write bytes from GBT-SCA message list 'msgs' to the DMA buffer
  // NB: negative sizes are turned into 'dummy chunks' (with zero-bytes as data)
  int hdr;
  if( _has5BitFromHostLength )
    hdr = ((elink << TOGBT_ELINK_SHIFT) | 0x1F);
  else
    hdr = ((elink << TOGBT_ELINK_SHIFT) |
           ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT));
  /*int dummy_hdr = ((0    << TOGBT_GBT_SHIFT) |
                   (egroup << TOGBT_EGROUP_SHIFT) |
                   (epath  << TOGBT_EPATH_SHIFT) |
                   ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT) |
                   TOGBT_EOM_MASK);
  */
  int total_blocks = 0;
  i64 buf_i = _writeIndex;
  bool dummy_chunk;
  for( unsigned int msg=0; msg<msgs.size(); ++msg )
    {
      int sz = msgs[msg].len;
      dummy_chunk = false;
      if( sz < 0 )
        {
          // Negative size: insert a 'dummy chunk' of that size
          sz = (-sz);
          dummy_chunk = true;
        }
      int blocks = (sz + TOGBT_PAYLOAD_BYTES-1) / TOGBT_PAYLOAD_BYTES;
      int final_bytes, final_size, final_hdr;
      final_bytes = sz - (blocks-1) * TOGBT_PAYLOAD_BYTES;
      if( _has5BitFromHostLength )
        {
          final_hdr  = hdr & ~TOGBT_LENGTH_MASK_5BIT;
          final_hdr |= final_bytes; // Length in bytes
        }
      else
        {
          final_size = (final_bytes + 1) / 2; // Length in 2-byte units
          final_hdr  = hdr & ~TOGBT_LENGTH_MASK;
          final_hdr |= ((final_size << TOGBT_LENGTH_SHIFT) | TOGBT_EOM_MASK);
        }

      int i, j;
      // Fully-filled To-GBT byte blocks
      for( i=0; i<blocks-1; ++i )
        {
          if( dummy_chunk )
            {
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = (u8) 0x00;
            }
          else
            {
              for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
                _dataBuffer[buf_i] = msgs[msg].data[j];
            }
          _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
          _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
          if( buf_i >= _bufferSize ) buf_i = 0;
        }

      // Final (possibly not fully filled) To-GBT byte block
      for( j=TOGBT_PAYLOAD_BYTES-1; j>=final_bytes; --j, ++buf_i )
        _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
      if( dummy_chunk )
        {
          for( j=final_bytes-1; j>=0; --j, ++buf_i )
            _dataBuffer[buf_i] = (u8) 0x00; // Final data bytes
        }
      else
        {
          for( j=final_bytes-1; j>=0; --j, ++buf_i )
            _dataBuffer[buf_i] = msgs[msg].data[j]; // Final data bytes
        }
      _dataBuffer[buf_i++] = (char) (final_hdr & 0xFF);
      _dataBuffer[buf_i++] = (char) ((final_hdr & 0xFF00) >> 8);
      if( buf_i >= _bufferSize ) buf_i = 0;

      total_blocks += blocks;

      // Add (a) dummy block(s) in between
      /*for( i=0; i<0; ++i )
        {
          for( j=0; j<TOGBT_PAYLOAD_BYTES; ++j )
            _dataBuffer[buf_i++] = (char) 0;
          _dataBuffer[buf_i++] = (char) (dummy_hdr & 0xFF);
          _dataBuffer[buf_i++] = (char) ((dummy_hdr & 0xFF00) >> 8);
        }
        total_blocks +=0;*/
    }

  // DMA is started in upload()
  _bytesToUpload = total_blocks * TOGBT_BLOCK_BYTES;
  _writeIndex = buf_i;
  //_uploadIndex = 0;

  return _bytesToUpload;
}

// ----------------------------------------------------------------------------

int FlxUpload::prepareDataRaw( int size, int pattern_id )
{
  // Create the test data bytes according to a pattern identifier
  char data[256];
  int  i;
  if( pattern_id == 1 )
    {
      // 0x55-0xAA pattern
      for( i=0; i<256; ++i )
        if( i & 1 )
          data[i] = (char) 0xAA;
        else
          data[i] = (char) 0x55;
    }
  else if( pattern_id == 2 )
    {
      // 0xFF only
      for( i=0; i<256; ++i )
        data[i] = (char) 0xFF;
    }
  else
    {
      // Incrementing pattern
      for( i=0; i<256; ++i )
        data[i] = (char) (i & 0xFF);
    }

  // Write 'size' bytes of test data to the DMA buffer
  i64 buf_i  = _writeIndex;
  int data_i = 0;
  for( i=0; i<size; ++i, ++buf_i, ++data_i )
    _dataBuffer[buf_i] = data[data_i & 0xFF];

  // DMA is started in upload()
  _bytesToUpload = size;
  _writeIndex = buf_i;
  //_uploadIndex = 0;

  return size;
}

// ----------------------------------------------------------------------------

int FlxUpload::prepareDataRaw( const std::string &filename, bool binary )
{
  std::ifstream file( filename );
  if( !file.is_open() )
    {
      _errString = "Failed to open file: ";
      _errString += filename;
      cout << "###" << this->errorString() << endl;
      return -1; // Something went wrong
    }
  cout << "Opened file " << filename << endl;

  i64 bytes_read = 0, bytes_to_upload = 0;
  i64 buf_i = _writeIndex;
  int line_nr = 0;
  if( binary )
    {
      // Storage for the number of bytes to read from the file at a time
      char data[1024*TOGBT_PAYLOAD_BYTES];
      int i, data_i;

      file.read( data, sizeof(data) );
      bytes_read = file.gcount();
      while( bytes_read > 0 )
        {
          bytes_to_upload += bytes_read;
          data_i = 0;

          for( i=0; i<bytes_read; ++i, ++buf_i, ++data_i )
            _dataBuffer[buf_i] = data[data_i];

          file.read( data, sizeof(data) );
          bytes_read = file.gcount();
        }
    }
  else
    {
      // Text file: lines with data(word) values
      std::string line;
      std::getline( file, line );
      while( !file.eof() )
        {
          ++line_nr;

          if( line.empty() )
            {
              // Next line
              std::getline( file, line );
              continue;
            }

          // Remove leading white-space
          size_t startpos = line.find_first_not_of( " \t" );
          if( startpos != string::npos && startpos != 0 )
            line = line.substr( startpos );

          std::istringstream iss;
          unsigned int val;
          if( line[0] == '#' )
            {
              // Comment line: skip it
              std::getline( file, line );
              continue;
            }
          else
            {
              // Line with (MROD) data
              int cnt = 1; // Entry counter
              iss.str( line );
              //cout << "Line " << line_nr << ": " << line << endl;
              iss >> std::hex; // Word values: hexadecimal
              while( !iss.eof() )
                {
                  // Next value (or end-of-file/line)
                  iss >> val;
                  if( !iss.fail() )
                    {
                      unsigned char *buf =
                        (unsigned char *) &_dataBuffer[buf_i];
                      *buf = (unsigned char) ((val>> 0) & 0xFF); ++buf;
                      *buf = (unsigned char) ((val>> 8) & 0xFF); ++buf;
                      *buf = (unsigned char) ((val>>16) & 0xFF); ++buf;
                      *buf = (unsigned char) ((val>>24) & 0xFF); ++buf;
                      buf_i += 4;
                      bytes_read += 4;
                    }
                  else if( !iss.eof() )
                    {
                      std::string s;
                      iss.clear();
                      iss >> s; // Skip past the illegal entry
                      cout << "###Line " << line_nr
                           << ", illegal entry #" << cnt << endl;
                    }
                  ++cnt;
                }
            }

          if( bytes_read <= 0 )
            {
              // Nothing to upload, go to next line
              std::getline( file, line );
              bytes_read = 0;
              continue;
            }
          bytes_to_upload += bytes_read;

          // Next line
          std::getline( file, line );
          bytes_read = 0;
        }
    }

  file.close();
  cout << "Bytes read: " << bytes_to_upload
       << " (words: " << bytes_to_upload/4 << ")";
  if( !binary )
    cout << ", lines: " << line_nr;
  cout << endl;

  if( bytes_read == 0 )
    {
      // DMA is started in upload()
      _bytesToUpload = ((bytes_to_upload + 31)/32) * 32;
      _writeIndex = buf_i;

      cout << "Bytes in upload: " << _bytesToUpload << endl;

      return _bytesToUpload;
    }
  return -1; // Something went wrong
}

// ----------------------------------------------------------------------------

bool FlxUpload::upload( int speed_factor )
{
  if( _bytesToUpload == 0 )
    return false;

  if( this->dmaActive() )
    {
      //struct timespec ts;
      //ts.tv_sec  = 0;
      //ts.tv_nsec = (TOGBT_BLOCK_BYTES/8) * 1000; // in nanoseconds
      //nanosleep( &ts, 0 );
      this->pause( TOGBT_BLOCK_BYTES/8 ); // in microseconds
      return false;
    }

  // Timestamp the start of an upload operation (the timestamp is reset
  // to zero when read by a call to function timestamp())
  if( _timestamp == 0 )
    {
      struct timespec ts;
      clock_gettime( CLOCK_REALTIME, &ts );
      _timestamp = ts.tv_sec*1000000000 + ts.tv_nsec;
    }

  u64 dma_size;
  if( _bytesToUpload > _uploadDmaSize )
    dma_size = _uploadDmaSize;
  else
    dma_size = _bytesToUpload;
  if( _uploadIndex + dma_size > (u64) _bufferSize )
    // Limit DMA to up to end of buffer (in case of single-shot DMA)
    dma_size = (u64) _bufferSize - _uploadIndex;

  // Delay handling required?
  if( !_delays.empty() )
    {
      delay_t &d = _delays.front();
      if( _uploadIndex == d.index )
        {
          // Arrived at a delay index: insert the requested delay
          this->pause( d.delay_us );
          // This delay has been handled, get rid of it
          _delays.pop_front();
        }
      if( !_delays.empty() )
        {
          delay_t &d = _delays.front();
          if( _uploadIndex + dma_size > d.index )
            {
              // Upcoming delay: adjust the DMA size to arrive at the index
              // where the requested delay is to be inserted
              dma_size = d.index - _uploadIndex;
            }
        }
    }

  this->dmaStart( _uploadIndex, dma_size );

  // Update upload administration
  _bytesToUpload -= dma_size;
  _uploadIndex   += dma_size;
  if( _uploadIndex >= (u64) _bufferSize ) {
    //cout << "WRAP @ " << _uploadIndex << endl;
    _uploadIndex = 0;
  }

  // Limit overall upload rate to a maximum of 8 MB/s
  // (for a 2-bit E-link, could be twice that for a 4-bit E-link,
  //  and 4x that for an 8-bit E-link)
  if( dma_size > 127 )
    //this->pause( (dma_size/(8*speed_factor))+1 );
    this->pause( dma_size/(8*speed_factor) );

  return true;
}

// ----------------------------------------------------------------------------

bool FlxUpload::uploadFinished()
{
  return( _bytesToUpload == 0 && !this->dmaActive() );
}

// ----------------------------------------------------------------------------

bool FlxUpload::startPlayback( const std::string &filename, int blocksize )
{
  if( _pbFile.is_open() ) _pbFile.close();
  
  _pbFile.open( filename.c_str(), std::ifstream::binary );
  if( !_pbFile.is_open() )
    {
      _errString = "Failed to open playback file: ";
      _errString += filename;
      return false;
    }

  // Initialize playback counters
  _pbBlockCntr = 0;
  _pbChunkCntr = 0;

  // Initialize E-link intermediate buffers
  for( unsigned int i=0; i<FLX_MAX_ELINK_NR+1; ++i )
    _eBuf[i].clear();

  if( blocksize > 16384 )
    {
      _errString = "Playback of blocks > 16K currently not supported";
      return false;
    }
  else if( blocksize > 0 )
    {
      // Force a different size than read from the FLX-device itself
      _blockSize = blocksize;
      _blockSizeCode  = (_blockSize/1024-1) << 8;
    }

  // In case of random data upload
  unsigned int seed = 1;
  srandom( seed );

  return true;
}

// ----------------------------------------------------------------------------

bool FlxUpload::playback( int elink_filter, int speed_factor )
{
  elink_filter = mapElink( elink_filter );
  // Allow up to 16K blocks (see startPlayback())
  char block[16384];//[BLOCK_BYTES];
  _pbFile.read( block, _blockSize );
  i64 bytes_read = _pbFile.gcount();
  if( bytes_read != _blockSize )
    return false;

  //_uploadIndex   = 0; // OLD method (one-shot DMAs from address 0)
  //_writeIndex    = 0; // OLD method (one-shot DMAs from address 0)
  _bytesToUpload = formatBlockForUpload( block, elink_filter );
  if( _bytesToUpload > 0 )
    {
      ++_pbBlockCntr;

      while( !this->uploadFinished() )
        {
          //cout << "playback upload:" << _bytesToUpload << endl;
          this->upload( speed_factor );
        }
    }

  return true;
}

// ----------------------------------------------------------------------------

bool FlxUpload::writeIcTxtFile( int gbt, int i2c_addr,
                                const std::string &filename,
                                int *nbytes_file,
                                bool ec, bool lpgbt_v1 )
{
  std::ifstream file( filename );
  if( !file.is_open() )
    {
      _errString = "Failed to open IC file: ";
      _errString += filename;
      return false;
    }
  // Read file contents (ASCII)
  u32 byteval;
  int index = 0;
  u8  databytes[1024];
  file >> std::hex; // Values are hexadecimal
  while( !file.eof() )
    {
      // Next value (or end-of-file)
      file >> byteval;
      if( file.eof() )
        break;

      if( file.fail() )
        {
          std::ostringstream oss;
          oss << "Illegal value at index=" << index;
          _errString = oss.str();
          return false;
        }
      else if( byteval < 0 || byteval > 0xFF )
        {
          std::ostringstream oss;
          oss << "Invalid byte value: 0x" << std::hex << std::setfill('0')
              << std::setw(2) << byteval << " at index " << std::dec << index;
          _errString = oss.str();
          return false;
        }
      databytes[index] = (u8) (byteval & 0xFF);
      ++index;
      if( index >= (int) sizeof(databytes) )
        {
          _errString = "File too large (>1024 values)";
          break;
        }
    }
  file.close();
  *nbytes_file = index; // Number of byte values in file

  bool result;
  result = this->writeIcChannel( gbt, i2c_addr, 0, index, databytes,
                                 ec, lpgbt_v1 );
  return result;
}

// ----------------------------------------------------------------------------

bool FlxUpload::writeIcChannel( int gbt, int i2c_addr, int reg_addr,
                                int nbytes, u8 *data, bool ec, bool lpgbt_v1 )
{
  bool read = false;
  return this->uploadIcFrame( read, gbt, i2c_addr, reg_addr, nbytes, data,
                              ec, lpgbt_v1 );
}

// ----------------------------------------------------------------------------

bool FlxUpload::readIcChannel( int gbt, int i2c_addr, int reg_addr,
                               int nbytes, bool ec, bool lpgbt_v1 )
{
  bool read = true;
  return this->uploadIcFrame( read, gbt, i2c_addr, reg_addr, nbytes, 0,
                              ec, lpgbt_v1 );
}

// ----------------------------------------------------------------------------

bool FlxUpload::uploadIcFrame( bool read,
                               int  gbt, int i2c_addr, int reg_addr,
                               int  nbytes, u8 *data,
                               bool ec, bool lpgbt_v1 )
{
  int len = nbytes;
  if( len > 1024 )
    {
      _errString = "Write/Read IC payload too large (>1024) for this app";
      return false;
    }

  // Compose IC frame for a socalled write-read sequence
  u8 frame[7 + 1024 + 1]; // Space for frame with up to 1024 bytes payload
  int index = 0;
  if( !lpgbt_v1 )
    frame[index++] = 0;                    // Additional byte!?...
  frame[index]   = (u8) ((i2c_addr & 0x7F) << 1);// GBTX I2C address
  if( read )
    frame[index] |= (u8) 1;                // Read/write bit
  ++index;
  frame[index++] = 1;                      // Command (not used in GBTX v1 + 2)
  frame[index++] = (u8) (len & 0xFF);      // Number of data bytes
  frame[index++] = (u8) ((len >> 8) & 0xFF);
  frame[index++] = (u8) (reg_addr & 0xFF); // Register (start) address
  frame[index++] = (u8) ((reg_addr >> 8) & 0xFF);
  if( !read )
    {
      // Bytes to write
      memcpy( &frame[index], data, len );  // Copy data bytes into frame
      index += len;
    }

  // Calculate parity
  // (do not include zero-byte and I2C-address, in case of GBT or lpGBTv0)
  int parity_start_index = (lpgbt_v1 ? 0 : 2);
  u8 parity = 0;
  for( int i=parity_start_index; i<index; ++i )
    parity ^= frame[i];
  frame[index++] = parity;                 // Parity byte

  // Upload IC frame, to IC- or EC-channel
  int epath;
  if( ec )
    epath = 7;
  else
    epath = 6;
  int frame_len = index;
  int egroup    = 7;
  int elink     = ((gbt    << BLOCK_GBT_SHIFT) |
                   (egroup << BLOCK_EGROUP_SHIFT) |
                   (epath  << BLOCK_EPATH_SHIFT));
  this->prepareData( elink, frame_len, frame );
  while( !this->uploadFinished() )
    {
      //cout << "uploadIcFrame upload:" << _bytesToUpload << endl;
      this->upload();
    }

  return true;
}

// ----------------------------------------------------------------------------

bool FlxUpload::addIcFrame( bool read, int i2c_addr, int reg_addr,
                            int  nbytes, u8 *data,
                            bool lpgbt_v1 )
{
  int len = nbytes;
  if( len > 4 )
    {
      _errString = "Write/Read IC payload too large (>4) for this app";
      return false;
    }

  sca_msg_t msg;

  // Compose IC frame for a socalled write-read sequence
  u8 *frame = msg.data;
  int index = 0;
  if( !lpgbt_v1 )
    frame[index++] = 0;                    // Additional byte!?...
  frame[index]   = (u8) ((i2c_addr & 0x7F) << 1);// GBTX I2C address
  if( read )
    frame[index] |= (u8) 1;                // Read/write bit
  ++index;
  frame[index++] = 1;                      // Command (not used in GBTX v1 + 2)
  frame[index++] = (u8) (len & 0xFF);      // Number of data bytes
  frame[index++] = (u8) ((len >> 8) & 0xFF);
  frame[index++] = (u8) (reg_addr & 0xFF); // Register (start) address
  frame[index++] = (u8) ((reg_addr >> 8) & 0xFF);
  if( !read )
    {
      // Bytes to write
      memcpy( &frame[index], data, len );  // Copy data bytes into frame
      index += len;
    }

  // Calculate parity
  // (do not include zero-byte and I2C-address, in case of GBT or lpGBTv0)
  int parity_start_index = (lpgbt_v1 ? 0 : 2);
  u8 parity = 0;
  for( int i=parity_start_index; i<index; ++i )
    parity ^= frame[i];
  frame[index++] = parity;                 // Parity byte

  msg.len = index;
  // Add to list of frames (reusing the GBT-SCA frames list)
  _scaMsgList.push_back( msg );

  return true;
}

// ----------------------------------------------------------------------------
// Examples uploadScaFrame():
// Transaction id freely chosen from range 1..0xFE
//
// Enable GPIO, via GBT EC-bits, i.e. egroup = 5, epath = 7:
// uploadScaFrame(elinknr, 0xAA, SCA_DEV_CONFIG, 2,
//                SCA_CONFIG_WR_B, [0x00,(1<<SCA_DEV_GPIO)])
//
// Set GPIO direction (cmd=0x20) to all output:
// uploadScaFrame(elinknr, 0xAB, SCA_DEV_GPIO, 4,
//                SCA_GPIO_WR_DIR, [0xFF, 0xFF, 0xFF, 0xFF])
//
// Set GPIO (cmd=0x10) bit 0+1 to 0:
// uploadScaFrame(elinknr, 0xAC, SCA_DEV_GPIO, 4,
//                SCA_GPIO_WR_OUT, [0xFC, 0xFF, 0xFF, 0xFF])

int FlxUpload::uploadScaFrame( int elink,
                               int *trid, int chan, int len, int cmd,
                               u8  *data )
{
  int sca_addr = 0; // Always 0?
  // Compose EC frame: (SOF+)ADDR+CTRL, payload: 4 + 0/2/4 bytes, FCS(, EOF)
  // (SOF/EOF are added by the firmware)
  u8  frame[2+(4+4)+2];
  frame[0] = (u8) (sca_addr & 0xFF);
  u64 seqnr = _hdlcSeqNr & HDLC_CTRL_SEQNR_MASK;
  // Control byte: acknowledge up to previous message RECeiVeD (by default?)
  // i.e. set to "first frame not received; it acknowledges that all frames
  // with seqnr up to (seqnr-1)%8 have been received"
  frame[1] = (u8) ((seqnr << HDLC_CTRL_NRECVD_SHIFT)|
                   (seqnr << HDLC_CTRL_NSENT_SHIFT));
  ++_hdlcSeqNr;
  // Make sure the transaction ID is valid
  if( *trid == 0 || *trid == 255 )
    *trid = 1;
  frame[2] = (u8) ((*trid) & 0xFF);
  // Update the transaction ID
  ++(*trid);
  if( *trid > 252 ) // Reserve 253 and 254 for our own custom purposes
    *trid = 1;
  frame[3] = (u8) (chan & 0xFF);
  frame[4] = (u8) (len  & 0xFF);
  //if( chan == SCA_DEV_CONFIG ) --frame[4]; // ###HACK (required or not?? TBD)
  frame[5] = (u8) (cmd  & 0xFF);
  // Add GBT-SCA frame data bytes
  for( int i=0; i<len; ++i )
    frame[6+i] = data[i];
  // Add GBT-SCA frame CRC
  u16 crc = crc16( frame, 2+4+len );
  frame[6+len]   = (u8) ((crc >> 8) & 0xFF);
  frame[6+len+1] = (u8) (crc & 0xFF);

  int upload_sz = this->prepareData( elink, 6+len+2, frame );
  //int sz = 6+len+2;
  //upload_sz = this->prepareData( elink, 1, &sz, frame );
  while( !this->uploadFinished() )
    {
      //cout << "uploadScaFrame upload:" << _bytesToUpload << endl;
      this->upload();
    }
  return upload_sz;
}

// ----------------------------------------------------------------------------

void FlxUpload::resetScaFrames()
{
  _scaMsgList.clear();
}

// ----------------------------------------------------------------------------

void FlxUpload::dumpScaFrames()
{
  sca_msg_t *pmsg;
  for( unsigned int i=0; i<_scaMsgList.size(); ++i )
    {
      pmsg = &_scaMsgList[i];
      cout << dec << setfill(' ') << setw(3) << i << ": ";
      if( pmsg->len < 0 )
        {
          cout << "delay=" << (-(pmsg->len));
        }
      else
        {
          cout << hex << setfill('0');
          for( int j=0; j<pmsg->len; ++j )
            cout << ' ' << setw(2) << ((int) pmsg->data[j] & 0xFF);
        }
      cout << endl;
    }
}

// ----------------------------------------------------------------------------

bool FlxUpload::addScaFrame( int *trid, int chan, int len, int cmd, u8 *data )
{
  sca_msg_t msg;
  int sca_addr = 0; // Always 0?
  // Compose EC frame: (SOF+)ADDR+CTRL, payload: 4 + 0/2/4 bytes, FCS(, EOF)
  // (SOF/EOF are added by the firmware)
  // and append it to the current list of SCA frames
  u8 *frame = msg.data;
  frame[0] = (u8) (sca_addr & 0xFF);
  u64 seqnr = _hdlcSeqNr & HDLC_CTRL_SEQNR_MASK;
  //if( (_hdlcSeqNr & 7) == 6 ){//DEBUG!!
  //  ++seqnr;
  //}
  // Control byte: acknowledge up to previous message RECeiVeD (by default?)
  // i.e. set to "first frame not received; it acknowledges that all frames
  // with seqnr up to (seqnr-1)%8 have been received"
  frame[1] = (u8) ((seqnr << HDLC_CTRL_NRECVD_SHIFT)|
                   (seqnr << HDLC_CTRL_NSENT_SHIFT));
  ++_hdlcSeqNr;
  // Make sure the transaction ID is valid, i.e. not 0 or 255 (reserved)
  if( *trid == 0 || *trid == 255 )
    *trid = 1;
  frame[2] = (u8) ((*trid) & 0xFF);
  // Update the transaction ID
  ++(*trid);
  if( *trid > 252 ) // Reserve 253 and 254 for our own custom purposes
    *trid = 1;
  frame[3] = (u8) (chan & 0xFF);
  frame[4] = (u8) (len  & 0xFF);
  //if( chan == SCA_DEV_CONFIG ) --frame[4]; // ###HACK (required or not?? TBD)
  frame[5] = (u8) (cmd  & 0xFF);
  // Add GBT-SCA frame data bytes
  for( int i=0; i<len; ++i )
    frame[6+i] = data[i];
  // Add GBT-SCA frame CRC
  u16 crc = crc16( frame, 2+4+len );
  frame[6+len]   = (u8) ((crc >> 8) & 0xFF);
  frame[6+len+1] = (u8) (crc & 0xFF);

  msg.len = 2 + 4 + len + 2;
  // Add to list of GBT-SCA frames
  _scaMsgList.push_back( msg );
  return true;
}

// ----------------------------------------------------------------------------

bool FlxUpload::addScaDelay( int len )
{
  // Negative length value is turned into a 'dummy chunk' of that length
  // by function prepareData()
  sca_msg_t msg;
  msg.len = -len;
  // Add to list of GBT-SCA frames
  _scaMsgList.push_back( msg );
  return true;
}

// ----------------------------------------------------------------------------

bool FlxUpload::addScaDelayUs( int microseconds )
{
  return this->addScaDelay( microseconds*10 );
}

// ----------------------------------------------------------------------------

int FlxUpload::uploadScaFrames( int elink )
{
  int upload_sz = this->prepareData( elink, _scaMsgList );
  //cout << "uploadScaFrames upload:" << _bytesToUpload << endl;
  while( !this->uploadFinished() )
    this->upload();
  return upload_sz;
}

// ----------------------------------------------------------------------------

int FlxUpload::scaConnect( int elink )
{
  return this->scaControl( elink,
                           //HDLC_CTRL_CONNECT | HDLC_CTRL_POLLBIT );
                           HDLC_CTRL_CONNECT );
}

// ----------------------------------------------------------------------------

int FlxUpload::scaReset( int elink )
{
  return this->scaControl( elink,
                           //HDLC_CTRL_RESET | HDLC_CTRL_POLLBIT );
                           HDLC_CTRL_RESET );
}

// ----------------------------------------------------------------------------

int FlxUpload::scaTest( int elink )
{
  return this->scaControl( elink,
                           //HDLC_CTRL_TEST | HDLC_CTRL_POLLBIT );
                           HDLC_CTRL_TEST );
}

// ----------------------------------------------------------------------------

void FlxUpload::dumpData( int size, int offset )
{
  char *data = &_dataBuffer[offset];
  cout << hex << setfill('0');
  for( int i=0; i<size; ++i )
    {
      if( (i & 31) == 0 ) cout << endl << setw(6) << i+offset;
      cout << ' ' << setw(2) << ((int) data[i] & 0xFF);
    }
  cout << dec << endl;
}

// ----------------------------------------------------------------------------

std::string FlxUpload::firmwareVersion()
{
  if( _flx == 0 ) return std::string();

  std::ostringstream oss;
  u64 ts  = _flx->cfg_get_reg( REG_BOARD_ID_TIMESTAMP );
  u64 fw  = _flx->cfg_get_reg( REG_FIRMWARE_MODE );
  u64 chn = _flx->cfg_get_reg( REG_NUM_OF_CHANNELS );
  u64 typ = _flx->cfg_get_reg( REG_CARD_TYPE );

  oss << "FLX" << typ << "-";
  if( fw == FIRMW_GBT )
    oss << "GBT-";
  else if( fw == FIRMW_FULL )
    oss << "FULL-";
  else if( fw == FIRMW_LTDB )
    oss << "LTDB-";
  else if( fw == FIRMW_FEI4 )
    oss << "FEI4-";
  else if( fw == FIRMW_PIXEL )
    oss << "PIXEL-";
  else if( fw == FIRMW_STRIP )
    oss << "STRIP-";
  else if( fw == FIRMW_FELIG )
    oss << "FELIG-";
  else if( fw == FIRMW_FMEMU )
    oss << "FMEMU-";
  else if( fw == FIRMW_MROD )
    oss << "MROD-";
  else if( fw == FIRMW_LPGBT )
    oss << "LPGBT-";
  else
    oss << "?" << "?" << "?-"; // Avoid 'trigraph'
  oss << chn << "chan-" << std::hex << ts;

  u64 git = _flx->cfg_get_reg( REG_GIT_TAG );
  oss << "-GIT:";
  char ch;
  for( int i=0; i<8; ++i, git>>=8 )
    {
      ch = (char) (git & 0xFF);
      if( ch == 0 ) break;
      oss << ch;
    }
  git = _flx->cfg_get_reg( REG_GIT_COMMIT_NUMBER );
  oss << "/" << std::dec << git;
  return oss.str();
}

// ---------------------------------------------------------------------------- 

int FlxUpload::firmwareMode()
{
  if( _flx == 0 ) return 0xFF;
  return _flx->cfg_get_reg( REG_FIRMWARE_MODE );
}

// ----------------------------------------------------------------------------

int FlxUpload::numberOfChans()
{
  if( _flx == 0 ) return 0;
  return _flx->cfg_get_reg( REG_NUM_OF_CHANNELS );
}

// ----------------------------------------------------------------------------
// Private functions
// ----------------------------------------------------------------------------

char *FlxUpload::cmemAllocate( u64 buffer_size, int id )
{
  u64   cmemPhysAddr = 0;
  char *buffer = 0;

  // Open CMEM(_RCC) and allocate the requested buffer size
#ifdef USE_LIB_CMEM
  u64 cmemVirtAddr;
  int ret = CMEM_Open();
  ostringstream oss;
  oss << "FlxUpload";
  if( id >= 0 ) oss << id;
  char name[] = oss.str().c_str();
  if( ret == CMEM_RCC_SUCCESS )
    ret = CMEM_GFPBPASegmentAllocate( buffer_size, name, &_cmemHandle );
  if( ret == CMEM_RCC_SUCCESS )
    ret = CMEM_SegmentPhysicalAddress( _cmemHandle, &cmemPhysAddr );
  if( ret == CMEM_RCC_SUCCESS )
    ret = CMEM_SegmentVirtualAddress( _cmemHandle, &cmemVirtAddr );
  if( ret == CMEM_RCC_SUCCESS )
    {
      buffer = (char *) cmemVirtAddr;
    }
  else
    {
      rcc_error_print( stdout, ret );
      if( !_errString.empty() ) _errString += "; ";
      std::ostringstream oss;
      oss << "CMEM_RCC error: 0x" << std::hex << ret;
      _errString += oss.str();
    }

#else
  if( (_cmemHandle = open("/dev/cmem_rcc", O_RDWR)) < 0 )
    {
      if( !_errString.empty() ) _errString += "; ";
      _errString += "Failed to open /dev/cmem_rcc";
      return 0;
    }

  if( id >= 0 )
    sprintf( _cmemDescriptor.name, "FlxUpload%d", id );
  else
    sprintf( _cmemDescriptor.name, "FlxUpload" );
  _cmemDescriptor.size    = buffer_size;
  _cmemDescriptor.order   = 0;
  _cmemDescriptor.type    = TYPE_GFPBPA;
  _cmemDescriptor.numa_id = 0;
  if( CMEM_RCC_SUCCESS == ioctl(_cmemHandle, CMEM_RCC_GET, &_cmemDescriptor) )
    {
      i64 result = (i64) mmap( 0, buffer_size, PROT_READ|PROT_WRITE,
                               MAP_SHARED, _cmemHandle,
                               (i64) _cmemDescriptor.paddr );
      if( result != -1 )
        {
          _cmemDescriptor.uaddr = result;
          if( CMEM_RCC_SUCCESS == ioctl(_cmemHandle, CMEM_RCC_SETUADDR,
                                        &_cmemDescriptor) )
            buffer = (char *) _cmemDescriptor.uaddr;

          cmemPhysAddr = _cmemDescriptor.paddr;
          //cmemVirtAddr = _cmemDescriptor.uaddr;
        }
      if( buffer == 0 )
        {
          if( !_errString.empty() ) _errString += "; ";
          _errString += "mmap/ioctl(SETUADDR) failed";
        }
    }
  else
    {
      if( !_errString.empty() ) _errString += "; ";
      _errString += "ioctl(GET) failed";
    }
#endif // USE_LIB_CMEM
  if( buffer == 0 )
    {
      _cmemStartAddr = 0;
      _cmemEndAddr   = 0;
    }
  else
    {
      _cmemStartAddr = cmemPhysAddr;
      _cmemEndAddr   = _cmemStartAddr + buffer_size;
    }

  return buffer;
}

// ----------------------------------------------------------------------------

void FlxUpload::dmaStart( u64 upload_index,
                          u64 size )
{
  if( !(_flx && _cmemStartAddr != 0 && size >= 0)  )
    return;

  if( _dmaCircular )
    {
      // Circular DMA still to be set up?
      if( (_bar0->DMA_DESC_ENABLE & (1 << _dmaIndex)) == 0 )
        {
          _flx->dma_from_host( _dmaIndex, _cmemStartAddr, _bufferSize,
                               FLX_DMA_WRAPAROUND | (_dmaTlp << 16) );

          _dmaSoftwPtr = _dmaDesc->read_ptr;

          // Adjust TLP: set to its minimum of 32 bytes
          // (to guarantee even a single GBT-SCA reply gets uploaded)
          // ###SHOULDN'T DO IT AFTER DMA ENABLE: dma_from_host() should do it
          //_dmaDesc->tlp = 32 / 4;
        }

      // Update the read pointer for circular DMA
      _dmaSoftwPtr += size;
      if( _dmaSoftwPtr >= _cmemEndAddr )
        _dmaSoftwPtr -= _bufferSize;
      _dmaDesc->read_ptr = _dmaSoftwPtr;
    }
  else
    {
      // Just in case...
      this->dmaStop();

      // Configure and start a single-shot DMA
      _flx->dma_from_host( _dmaIndex, _cmemStartAddr + upload_index, size,
                           0 | (_dmaTlp << 16) );
    }
}

// ----------------------------------------------------------------------------

void FlxUpload::dmaStop()
{
  if( _flx == 0 ) return;

  _flx->dma_stop( _dmaIndex );

  // Reset etc.
  //_flx->soft_reset();
  //usleep( 10000 );
  //_flx->dma_reset();
  //usleep( 10000 );
  //_flx->dma_fifo_flush(); OBSOLETE
  //usleep( 10000 );
}

// ----------------------------------------------------------------------------

bool FlxUpload::dmaActive()
{
  if( _flx == 0 ) return false;

  if( _dmaCircular )
    {
      //return( _dmaStat->current_address != _dmaSoftwPtr );

      // ### Temporary?: 'current_address' remains 1 TLP-size behind...
      u64 curr_addr = _dmaStat->current_address;
      if( _dmaSoftwPtr > curr_addr )
        // If consumer (curr_addr) is 32 bytes or less behind the producer:
        // DMA not active
        return( (_dmaSoftwPtr - curr_addr > 32) );
      else if( _dmaSoftwPtr < curr_addr )
        // If consumer (curr_addr) is somewhere at top of buffer,
        // and producer at start of buffer, DMA is still active
        return true;
      else
        return false;
    }
  else
    {
      return( (_bar0->DMA_DESC_ENABLE & (1 << _dmaIndex)) != 0 );
    }
}

// ----------------------------------------------------------------------------

int FlxUpload::formatBlockForUpload( char *block, int elink_filter )
{
  uint16_t *block_u16 = (uint16_t *) block;

  // Check for a valid block header
  if( !(block_u16[1] == BLOCK_ID ||
	block_u16[1] == _trailerFmt.blockId ||
	block_u16[1] == (BLOCK_ID_PHASE2 | _blockSizeCode)) )
    return 0;

  //int elinkseqnr = (int) (((uint32_t) block[0]) | ((uint32_t) block[1] << 8));
  int elinkseqnr = (int) block_u16[0];
  int elinknr    = (elinkseqnr & BLOCK_ELINK_MASK) >> BLOCK_ELINK_SHIFT;

  // Apply Elink filter
  if( elink_filter >= 0 && elinknr != elink_filter )
    return 0;

  // Go through the chunks in the block,
  // from *end-of-block* to *start-of-block*, storing info about each
  uint32_t i, trailer;
  chunk_desc_t chnk;
  std::vector<chunk_desc_t> chunks;
  int index = _blockSize - _trailerFmt.nbytes; // Last trailer in block
  uint8_t *block_u8 = (uint8_t *) block;
  chnk.trunc  = false;
  chnk.err    = false;
  chnk.crcerr = false;
  chnk.invalid_sz = -1;
  while( index > BLOCKHEADER_BYTES-1 )
    {
      trailer = 0;
      for( i=0; i<_trailerFmt.nbytes; ++i )
        trailer |= (block_u8[index+i] << (i*8));
      chnk.length = trailer & _trailerFmt.lengthMsk;
      chnk.type   = ((trailer & _trailerFmt.typeMsk) >> _trailerFmt.typeShift);

      // The start of this chunk; account for possible padding bytes
      if( (chnk.length & (_trailerFmt.nbytes-1)) != 0 )
        index -= (chnk.length + _trailerFmt.nbytes -
                  (chnk.length & (_trailerFmt.nbytes-1)));
      else
        index -= chnk.length;
      chnk.index = index;

      // Move to the preceeding trailer
      index -= _trailerFmt.nbytes;

      if( (chnk.type == CHUNKTYPE_BOTH ||
           chnk.type == CHUNKTYPE_MIDDLE ||
           chnk.type == CHUNKTYPE_FIRST ||
           chnk.type == CHUNKTYPE_LAST) && index > 0 )
        // Chunk is to be processed and uploaded
        chunks.push_back( chnk );
    }

  // Format chunk-by-chunk for upload
  // from *start-of-block* to *end-of-block*,
  // so reverse order in which chunks were found above,
  // taking into account FIRST, MIDDLE and LAST chunk types
  int hdr;
  if( _has5BitFromHostLength )
    hdr = ((elinknr << TOGBT_ELINK_SHIFT) | 0x1F);
  else
    hdr = ((elinknr << TOGBT_ELINK_SHIFT) |
           ((TOGBT_PAYLOAD_BYTES/2) << TOGBT_LENGTH_SHIFT));
  char *chunkdata;
  int nbytes = 0;
  int c;
  for( c=chunks.size()-1; c>=0; --c )
    {
      chunkdata = &block[chunks[c].index];

      if( chunks[c].type == CHUNKTYPE_BOTH )
        {
          // Format directly from block into DMA buffer
          nbytes += formatChunkForUpload( hdr, nbytes,
                                          chunkdata, chunks[c].length );
          ++_pbChunkCntr;
        }
      else if( chunks[c].type == CHUNKTYPE_LAST )
        {
          // Append to saved FIRST(+MIDDLE) part(s) in intermediate buffer
          vector<char> *ebuf = &_eBuf[elinknr];
          ebuf->insert( ebuf->end(), chunkdata, chunkdata + chunks[c].length );

          // Format as FLX upload blocks in DMA buffer
          nbytes += formatChunkForUpload( hdr, nbytes,
                                          ebuf->data(), ebuf->size() );
          ++_pbChunkCntr;

          // Clear intermediate buffer
          ebuf->clear();
        }
      else if( chunks[c].type == CHUNKTYPE_FIRST ||
               chunks[c].type == CHUNKTYPE_MIDDLE )
        {
          // Copy to intermediate buffer
          vector<char> *ebuf = &_eBuf[elinknr];
          ebuf->insert( ebuf->end(), chunkdata, chunkdata + chunks[c].length );
        }
    }

  return nbytes;
}

// ----------------------------------------------------------------------------

int FlxUpload::formatChunkForUpload( int hdr, int dest_index,
                                     char *chunkdata, int chunksize )
{
  // Format and write the chunk data to the DMA buffer for upload
  int nblocks = (chunksize + TOGBT_PAYLOAD_BYTES-1) / TOGBT_PAYLOAD_BYTES;
  int final_bytes, final_size, final_hdr;
  final_bytes = chunksize - (nblocks-1) * TOGBT_PAYLOAD_BYTES;
  if( _has5BitFromHostLength )
    {
      final_hdr  = hdr & ~TOGBT_LENGTH_MASK_5BIT;
      final_hdr |= final_bytes; // Length in bytes
    }
  else
    {
      final_size = (final_bytes + 1) / 2; // Length in 2-byte units
      final_hdr  = hdr & ~TOGBT_LENGTH_MASK;
      final_hdr |= ((final_size << TOGBT_LENGTH_SHIFT) | TOGBT_EOM_MASK);
    }

  //char *buf = &_dataBuffer[dest_index];
  //i64 buf_i = dest_index; // OLD method (one-shot DMAs from address 0)
  i64 buf_i = _writeIndex;
  static long cntr = 0;
  //unsigned char *p = (unsigned char *) chunkdata;

  // Fully-filled To-GBT byte blocks
  int i, j;
  for( i=0; i<nblocks-1; ++i )
    {
      // SPECIAL: fill chunk with random data
      // (catch errors provoked by a certain byte order?)
      if( _pbRandomData )
        {
          for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j )
            if( (i*TOGBT_PAYLOAD_BYTES+j) >= CHUNKHDR_SIZE )
              //chunkdata[j] = (char) 0x11;
              //chunkdata[j] = (char) (hdr >> TOGBT_EPATH_SHIFT);//Data=elinknr
              chunkdata[j] = (char) (random() & 0xFF);
        }

      for( j=TOGBT_PAYLOAD_BYTES-1; j>=0; --j, ++buf_i )
        _dataBuffer[buf_i] = chunkdata[j];
      chunkdata += TOGBT_PAYLOAD_BYTES;
      _dataBuffer[buf_i++] = (char) (hdr & 0xFF);
      _dataBuffer[buf_i++] = (char) ((hdr & 0xFF00) >> 8);
      if( buf_i >= _bufferSize ) buf_i = 0;
    }

  // SPECIAL: fill chunk with random data
  // (catch errors provoked by a certain byte sequences?)
  if( _pbRandomData )
    {
      for( j=final_bytes-1; j>=0; --j )
        if( (i*TOGBT_PAYLOAD_BYTES+j) >= CHUNKHDR_SIZE )
          //chunkdata[j] = (char) 0x11; 
          //chunkdata[j] = (char) (hdr >> TOGBT_EPATH_SHIFT);//Data=elinknr
          chunkdata[j] = (char) (random() & 0xFF);
      //unsigned char *p = (unsigned char *) chunkdata;
      // A counter in order to identify the chunk that generates an error
      //p[final_bytes-1] = ((cntr>>24) & 0xFF);
      //p[final_bytes-2] = ((cntr>>16) & 0xFF);
      //p[final_bytes-3] = ((cntr>> 8) & 0xFF);
      //p[final_bytes-4] = ((cntr>> 0) & 0xFF);
      // Following bytes to start at a 4-byte aligned position
      // ### 8-bit E-link: high likelyhood of receiving only byt 8d,
      // loosing 91, 6e, e7 and the next byte:
      //p[final_bytes-9+2]  = 0xe7;
      //p[final_bytes-10+2] = 0x6e;
      //p[final_bytes-11+2] = 0x91;
      //p[final_bytes-12+2] = 0x8d;
      // ### And this sequence has the same effect:
      //p[final_bytes-13+2] = 0xb3;
      //p[final_bytes-14+2] = 0x71;
      //p[final_bytes-15+2] = 0x8e;
      //p[final_bytes-16+2] = 0x92;
    }

  // After determining which random block (check the counter!)
  // triggers a problem use the following code to dump that block's content
  // to compare to the received data chunk to find out which byte sequence
  // in particular triggers the problem
  //if( random_data && cntr == 0x1d2de3 )
  /*if( random_data && cntr == 0x29fd4a )
    {
      cout << hex << endl;
      for( int k=0; k<200; ++k )
        {
          cout << setw(2) << ((u32) p[k] & 0xFF) << " ";
          if( (k & 0x1F) == 0x1F ) cout << endl;
        }
      cout << endl;
      }*/
  ++cntr;

  // Final (likely not fully filled) To-GBT byte block
  for( j=TOGBT_PAYLOAD_BYTES-1; j>=final_bytes; --j, ++buf_i )
    _dataBuffer[buf_i] = _fillerByte; // 'Empty' block bytes
  for( j=final_bytes-1; j>=0; --j, ++buf_i )
    _dataBuffer[buf_i] = chunkdata[j]; // Final data bytes
  _dataBuffer[buf_i++] = (char) (final_hdr & 0xFF);
  _dataBuffer[buf_i++] = (char) ((final_hdr & 0xFF00) >> 8);
  if( buf_i >= _bufferSize ) buf_i = 0;

  _writeIndex = buf_i;

  return( nblocks * TOGBT_BLOCK_BYTES );
}

// ----------------------------------------------------------------------------

int FlxUpload::scaControl( int elink, int control )
{
  int sca_addr = 0; // Always 0?
  // Compose control HDLC frame for GBT-SCA: SOF+ADDR+CTRL, FCS, EOF
  u8 frame[2+2];
  frame[0] = (u8) (sca_addr & 0xFF);
  frame[1] = (u8) (control & 0xFF);
  u16 crc = crc16( frame, 2 );
  frame[2] = (u8) ((crc >> 8) & 0xFF);
  frame[3] = (u8) (crc & 0xFF);
  int upload_sz = this->prepareData( elink, sizeof(frame), frame );
  while( !this->uploadFinished() )
    {
      //cout << "scaControl upload:" << _bytesToUpload << endl;
      this->upload();
    }
  return upload_sz;
}

// ----------------------------------------------------------------------------

void FlxUpload::pause( int delay_us )
{
  // Use usleep() when appropriate, but run a busy-wait loop for short delays
  // (determined by usleep() overhead) to achieve better timing precision
  if( delay_us > _minimumUsleep )
    {
      usleep( delay_us - _minimumUsleep );
    }
  else if( delay_us > 0 )
    {
      struct timespec ts1, ts2;
      volatile long s, t;
      clock_gettime( CLOCK_REALTIME, &ts1 );
      s = ts1.tv_sec*1000000000 + ts1.tv_nsec;
      clock_gettime( CLOCK_REALTIME, &ts2 );
      t = ((ts2.tv_sec*1000000000 + ts2.tv_nsec) - s)/1000;
      while( t < delay_us )
        {
          clock_gettime( CLOCK_REALTIME, &ts2 );
          t = ((ts2.tv_sec*1000000000 + ts2.tv_nsec) - s)/1000;
        }
    }
}

// ----------------------------------------------------------------------------

int FlxUpload::minimumUsleep()
{
  // Determine the time overhead it takes to execute usleep(1), in microseconds
  struct timespec ts1, ts2;
  clock_gettime( CLOCK_REALTIME, &ts1 );
  // Average over 4 calls..
  usleep( 1 );
  usleep( 1 );
  usleep( 1 );
  usleep( 1 );
  clock_gettime( CLOCK_REALTIME, &ts2 );
  long t = ((ts2.tv_sec*1000000000 + ts2.tv_nsec) -
            (ts1.tv_sec*1000000000 + ts1.tv_nsec))/4;
  if( t < 1000 || t > 1000000 )
    t = 50000;
  return (t-1000+500)/1000; // In microseconds
}

// ----------------------------------------------------------------------------

int FlxUpload::mapElink( int elinknr )
{
#if REGMAP_VERSION >= 0x500
  // EC+IC+AUX egroup? (sticking to the RM4 numbering)
  if( ((elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT) == 7 )
    {
      int epath = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
      elinknr &= ~(BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK);

      // Read EC and IC number/index from a register
      if( epath == 7 ) // EC link
        elinknr |= _bar2->AXI_STREAMS_FROMHOST.EC_INDEX;
      if( epath == 6 ) // IC link
        elinknr |= _bar2->AXI_STREAMS_FROMHOST.IC_INDEX;
      //if( epath == 5 ) // AUX link
      //  elinknr |= 18;

      /* ###OLD HARDCODED APPROACH:
      // FromHost direction: GBTX: EC=40,IC=41; lpGBT: EC=16,IC=17
      // Is it lpGBT-type firmware?
      //if( _gbtConfig[0].linkMode() == LINKMODE_LPGBT )
      uint64_t fw = _firmwareMode;
      if( fw == FIRMW_PIXEL || fw == FIRMW_STRIP || fw == FIRMW_LPGBT )
        {
          if( epath == 7 ) elinknr |= 16; // EC link
          if( epath == 6 ) elinknr |= 17; // IC link
          //if( epath == 5 ) elinknr |= 18; // AUX link
        }
      else
        {
          if( epath == 7 ) elinknr |= 40; // EC link
          if( epath == 6 ) elinknr |= 41; // IC link
          //if( epath == 5 ) elinknr |= 42; // AUX link
        }
      */
    }
#endif // REGMAP_VERSION
  return elinknr;
}

// ----------------------------------------------------------------------------
