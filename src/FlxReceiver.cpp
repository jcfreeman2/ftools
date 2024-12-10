#include <string.h> // for memset()
#include <sstream>  // for ostringstream
#include <iomanip>

#include "flxdefs.h"
#include "FlxReceiver.h"
#include "flxcard/FlxException.h"

//#define USE_LIB_CMEM

#ifndef USE_LIB_CMEM
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

// ### For debugging DMA register read issue
//#include <vector>
//static std::vector<u64> _pWrite;

// ----------------------------------------------------------------------------

FlxReceiver::FlxReceiver( int  card_nr,
                          i64  buffer_size,
                          int  dma_index,
                          bool reset )
  : _pThread( 0 ),
    _flx( 0 ),
    _firmwareMode( -1 ),
    _nChans( -1 ),
    _blockSize( -1 ),
    _running( true ),
    _cardNr( card_nr ),
    _useInterrupt( true ),
    _suspend( false ),
    _suspended( false ),
    _bytesReceived( 0 ),
    _lastBlockSize( 0 ),
    _lastBytesAvailable( 0 ),
    _bufferWraps( 0 ),
    _fullCounter( 0 ),
    _debugCounter( 0 ),
    _bufferSize( 0 ),
    _dmaWritePtr( 0 ),
    _dmaReadPtr( 0 ),
    _head( 0 ),
    _tail( 0 ),
    _tlpSizeMask( 0 ),
    _freeSpaceMinimum( 0 ),
    _full( false ),
    _fullOccurred( false ),
    _timestamp( 0 ),
    _dataBuffer( 0 ),
    _cmemHandle( -1 ),
    _cmemStartAddr( 0 ),
    _cmemEndAddr( 0 ),
    _dmaIndex( dma_index ),
    _irqIndex( FLX_INT_DATA_AVAILABLE ),
    _dmaDesc( 0 ),
    _dmaStat( 0 )
{
  // A buffer size of 0 also selects the default size
  if( buffer_size == 0 ) buffer_size = DFLT_RECV_BUF_SIZE;

  // Allocate a (continuous) buffer of the requested size
  _dataBuffer = this->cmemAllocate( buffer_size, card_nr );

  if( _dataBuffer )
    {
      // Initialize the buffer
      memset( static_cast<void *> (_dataBuffer), 0xFF, buffer_size );
      _bufferSize = buffer_size;
      // Require a minimum of free space before applying 'busy'
      // (choose 1/8th of the buffer size with a minimum of 16M)
      _freeSpaceMinimum = _bufferSize/8;
      //if( _freeSpaceMinimum < FREE_SPACE_MINIMUM )
      //  _freeSpaceMinimum = FREE_SPACE_MINIMUM;
    }
  else
    {
      if( !_errString.empty() )
        _errString += "; ";
      _errString += "Failed to allocate requested buffer size";
      _running = false;
    }

  uint32_t lockbits = LOCK_NONE;
  // Backwards compatibility
  if( dma_index == 0 )
    lockbits |= LOCK_DMA0;
  else if( dma_index == 1 )
    lockbits |= LOCK_DMA1;
  // New lockbits
  if( dma_index > -1 )
    lockbits |= ((1 << dma_index) << 16);

#if REGMAP_VERSION >= 0x500
  // Configure IRQ index/number to use
  _irqIndex += dma_index;
#endif // REGMAP_VERSION

  // Open FELIX FLX-card
  _flx = new FlxCard;
  try {
    _flx->card_open( _cardNr, lockbits );
  }
  catch( FlxException &ex ) {
    if( !_errString.empty() )
      _errString += "; ";
    _errString += "FlxCard open: ";
    _errString += ex.what();
    delete _flx;
    _flx = 0;
    return;
  }

  // Get this firmware's to-host data blocksize and trailer format
  _blockSize = _flx->cfg_get_reg( REG_BLOCKSIZE );
  if( _blockSize == 0 ) // Backwards compatibility!
    _blockSize = 1024;
  _trailerIs32Bit = (_flx->cfg_get_reg( REG_CHUNK_TRAILER_32B ) == 1);

  // Don't start the thread until a buffer has been allocated
  // (see setBufferSize)
  if( _dataBuffer && _running )
    {
      // Check for a valid DMA index
      int dma_cnt = _flx->cfg_get_option( BF_GENERIC_CONSTANTS_DESCRIPTORS );
      if( _dmaIndex < dma_cnt )
        {
          // ###Note: first 3 parameters could be removed (use member variables)
          this->dmaStart( _cmemStartAddr, _bufferSize, _dmaIndex, reset );

          // Start the thread (see run())
          _pThread = new std::thread( [this](){ this->run(); } );
        }
      else
        {
          if( !_errString.empty() )
            _errString += "; ";
          _errString += "DMA index not in firmware range";
          _running = false;
        }
    }
}

// ----------------------------------------------------------------------------

FlxReceiver::~FlxReceiver()
{
  // In case the thread is still running
  // (but rather stop it beforehand calling stop() directly..)
  this->stop();

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

void FlxReceiver::stop()
{
  this->stopDataEmulator(); // NB: also sets fan-out regs
  this->dmaStop( _dmaIndex );
  _running = false;
  if( _pThread )
    {
      if( _flx )
        {
          // Disable interrupt, then wake up thread from irq_wait() in run()
          _flx->irq_disable( _irqIndex );
          _flx->irq_cancel( _irqIndex );
        }

      // Wait until the receive thread (i.e. function run()) exits
      _pThread->join();
      _pThread = 0;
    }
}

// ----------------------------------------------------------------------------

void FlxReceiver::run()
{
  u64 pwrite, nbytes;
  i64 head, tail, freespace;

  // Now done in c'tor (see above)
  //this->dmaStart( _cmemStartAddr, _bufferSize, _dmaIndex );

  while( _running )
    {
      // Synchronize with reset and set-buffersize actions
      // (but it doesn't protect us from calls to updateBytesConsumed()
      //  which may arrive after _suspend has been reset to false)
      if( _suspend && !_suspended ) _suspended = true;
      if( _suspended )
        {
          // Pause for a little while
          std::this_thread::sleep_for( std::chrono::microseconds(1000) );
          continue;
        }

      // Determine the number of additional bytes available, if any
      _mutex.lock();
      pwrite = _dmaStat->current_address;
      while( pwrite < _cmemStartAddr || pwrite > _cmemEndAddr )
        // Can't be correct...reread... (firmware issue!?)
        pwrite = _dmaStat->current_address;
      _mutex.unlock();

      //if( _dmaWritePtr != pwrite )
      //_pWrite.push_back( pwrite );
      if( _dmaWritePtr <= pwrite )
        {
          nbytes = pwrite - _dmaWritePtr;
        }
      else
        {
          nbytes  = _cmemEndAddr - _dmaWritePtr;
          nbytes += pwrite - _cmemStartAddr;
        }

      if( nbytes == 0 || _tail != _head )
        {
          // Wait or pause for a little while because:
          // - either there is no new data, or
          // - previous data has not been consumed yet anyway so we can
          //   take a (short) break, in order not to poll too often
          if( nbytes == 0 && _useInterrupt )
            // Wait for data-available interrupt
            _flx->irq_wait( _irqIndex );
          else
            std::this_thread::sleep_for( std::chrono::microseconds(1000) );
          if( nbytes == 0 ) continue;
        }
      /*else
        {
          std::this_thread::sleep_for( std::chrono::microseconds(1000) );
        }
      */
      _dmaWritePtr = pwrite;
      head = _head + nbytes;

      // Wrap-around?
      if( head >= _bufferSize )
        {
          head %= _bufferSize;
          ++_bufferWraps;
        }
      tail = _tail; // Take snapshot
      _head = head; // Update

      // Getting full ?
      if( head > tail )
        freespace = _bufferSize - (head - tail);
      else
        freespace = tail - head;
      if( freespace < _freeSpaceMinimum )
        {
          if( !_full )
            {
              _full = true;
              ++_fullCounter;

              // Optionally do something to stem the flow of data...
              // i.e. set BUSY?
              if( _flx )
                {
                  //this->stopDataEmulator();
                  //_flx->dma_stop( _dmaIndex );
                }
              // ...
            }
          _fullOccurred = true;
        }

      // Timestamp the start of a receive operation
      // (the timestamp is reset to zero when read by
      //  a call to function timestamp())
      if( _timestamp == 0 )
        {
          struct timespec ts;
          clock_gettime( CLOCK_REALTIME, &ts );
          _timestamp = ts.tv_sec*1000000000 + ts.tv_nsec;
        }

      // Statistics
      _bytesReceived += nbytes;
      _lastBlockSize = nbytes;
    }
}

// ----------------------------------------------------------------------------

std::string FlxReceiver::infoString()
{
  std::ostringstream oss;
  oss << "firmw " << this->firmwareVersion()
      << ", trailer=";
  oss << (this->trailerIs32Bit() ? "32bit" : "16bit");
  if( _blockSize > 1024 )
    oss << " block=" << _blockSize/1024 << "K";
  oss << ", buffer=" << this->bufferSize()/(1024*1024) << "MB";
  oss << ", DMA=" << _dmaIndex;
  return oss.str();
}

// ----------------------------------------------------------------------------

std::string FlxReceiver::errorString()
{
  if( _errString.empty() )
    return std::string( "" );
  std::ostringstream oss;
  oss << "Receiver@FLX" << _cardNr << ": " << _errString;
  return oss.str();
}

// ----------------------------------------------------------------------------

bool FlxReceiver::hasError()
{
  return !_errString.empty();
}

// ----------------------------------------------------------------------------

std::string FlxReceiver::debugString()
{
  std::ostringstream oss;
  oss << "Receiver@FLX" << _cardNr << " DEBUG: "
      << "head=" << _head << ", tail=" << _tail
      << ", pwrite=" << std::hex << std::setfill('0')
      << std::setw(10) << _dmaWritePtr << std::endl;
  _mutex.lock();
  oss << "  start=" << std::setw(10) << _cmemStartAddr
      << ", end=" << std::setw(10) << _cmemEndAddr
      << ", pread=" << std::setw(10) << _dmaReadPtr
      << "(" << std::setw(10) << _dmaDesc->read_ptr
      << "), pcurr_addr=" << std::setw(10) << _dmaStat->current_address
      << ", even-PC/DMA " << std::dec << _dmaStat->even_addr_pc
      << " " << _dmaStat->even_addr_dma << std::endl;
  _mutex.unlock();
  oss << "  wraps=" << _bufferWraps
      << ", recvd=" << _bytesReceived
      << ", last-recvd=" << _lastBlockSize
      << ", last-av=" << _lastBytesAvailable
      << ", fullcnt=" << _fullCounter << std::endl;
  /*
  oss << std::hex
  for( u32 i=0; i<_pWrite.size(); ++i )
    {
      oss << std::setw(10) << _pWrite[i] << " "
      if( (i & 0x7) == 7 ) oss << std::endl;
    }
  */
  return oss.str();
}

// ----------------------------------------------------------------------------

void FlxReceiver::setUseInterrupt( bool b )
{
  if( b )
    {
      _useInterrupt = true;
      if( _flx )
        _flx->irq_enable( _irqIndex );
    }
  else
    {
      _useInterrupt = false;
      if( _flx )
        {
          // Disable interrupt, then wake up thread from irq_wait() in run()
          _flx->irq_disable( _irqIndex );
          _flx->irq_cancel( _irqIndex );
        }
    }
}

// ----------------------------------------------------------------------------

i64 FlxReceiver::bytesAvailable()
{
  // Return the number of data bytes available for processing at this time
  // without going past the end of the buffer (no wrap-around)
  i64 bytes;
  i64 t = _tail; // Take snapshot
  i64 h = _head; // Take snapshot
  if( t < h )
    {
      bytes = h - t;
    }
  else
    {
      if( h == t && !_full )
        // Buffer is empty
        bytes = 0;
      else
        // Give the number of bytes left up to the end of the buffer
        // (so that the consumer does not have to deal with
        //  buffer wrap-around situations)
        bytes = _bufferSize - t;
    }
  // Truncate to FLX-card block size (assuming size is a power of 2..)
  bytes &= ~(_blockSize-1);
  // Alternatively:
  //bytes = (bytes/_blockSize) * _blockSize;

  _lastBytesAvailable = bytes; // For debugging

  return bytes;
}

// ----------------------------------------------------------------------------

void FlxReceiver::updateBytesConsumed( i64 nbytes )
{
  if( _suspend || nbytes == 0 ) return;

  // An amount of 'nbytes' bytes have been consumed:
  // going to update the tail pointer
  i64 t = _tail + nbytes;

  // Wrap-around the end of the buffer when appropriate (NB: a number
  // of bytes has been provided that wraps around 'exactly'; see above)
  if( t == _bufferSize ) t = 0;

  // Still in 'full' state ?
  if( _full )
    {
      i64 freespace, h = _head;
      if( h > t )
        freespace = _bufferSize - (h - t);
      else
        freespace = t - h;
      if( freespace >= _freeSpaceMinimum )
        {
          _full = false;

          // Optionally do something to restart the flow of data...
          // ...
        }
    }
  _tail = t;

  // Update the DMA read pointer
  _dmaReadPtr += nbytes;
  if( _dmaReadPtr >= _cmemEndAddr )
    _dmaReadPtr -= _bufferSize;
  // Remain (just) behind the write pointer?
  //if( _dmaReadPtr == _dmaWritePtr )
  //  _dmaReadPtr -= 1;
  _mutex.lock();
  _dmaDesc->read_ptr = _dmaReadPtr & _tlpSizeMask; // Update
  _mutex.unlock();
}

// ----------------------------------------------------------------------------

bool FlxReceiver::setBufferSize( i64 size )
{
  // Demand a certain minimum size... e.g. twice the minimum space
  if( size < 2*FREE_SPACE_MINIMUM ) return false;

  // Already allocated ?
  if( size == _bufferSize ) return true;

  this->suspend();

  // Reset receive buffer administration
  _bytesReceived   = 0;
  _head            = 0;
  _tail            = 0;
  _full            = false;
  _fullOccurred    = false;
  _fullCounter     = 0;

  // Free existing buffer if any
  //if( _dataBuffer ) delete [] _dataBuffer;
  _dataBuffer = 0;
  _bufferSize = 0;

  // Allocate a new buffer of the requested size
  bool result = true;
  _dataBuffer = this->cmemAllocate( size, _cardNr );

  if( _dataBuffer )
    {
      // Initialize the buffer
      memset( static_cast<void *> (_dataBuffer), 0x00, size );
      _bufferSize = size;
      // Require a minimum of free space before applying 'busy'
      // (choose 1/8th of the buffer size with a minimum of FREE_SPACE_MINIMUM)
      _freeSpaceMinimum = size/8;
      if( _freeSpaceMinimum < FREE_SPACE_MINIMUM )
        _freeSpaceMinimum = FREE_SPACE_MINIMUM;

      this->dmaStart( _cmemStartAddr, _bufferSize, _dmaIndex );
      if( _pThread == 0 )
        {
          // Start the thread now that a buffer has been allocated
          _running = true;
          _pThread = new std::thread( [this](){ this->run(); } );
        }
    }
  else
    {
      _errString += "; Failed to allocate requested buffer size";
      _running = false;
      result = false;
    }

  this->resume();
  return result;
}

// ----------------------------------------------------------------------------

u64 FlxReceiver::bufferFullPercentage()
{
  if( _dmaDesc == 0 || _dmaStat == 0 ) return 0L;

  // CurrBuffer full/unread percentage:
  // how close is the write pointer 'behind' the read pointer
  // (or how far is the read pointer lagging behind the write pointer)
  // as a percentage of the buffer size
  _mutex.lock();
  u64 pwrite  = _dmaStat->current_address;
  u64 pread   = _dmaDesc->read_ptr;
  u64 pstart  = _cmemStartAddr; //_dmaDesc->start_address;
  u64 pend    = _cmemEndAddr;   //_dmaDesc->end_address;
  _mutex.unlock();
  u64 buffer_size = pend - pstart;
  u64 percent = 0L;

  // Only if the write and read pointers make sense
  if( buffer_size > 0L &&
      pwrite >= pstart && pwrite <= pend &&
      pread >= pstart && pread <= pend )
    {
      if( pread == pwrite )
        {
          if( _full ) percent = 100;
        }
      else if( pread < pwrite )
        {
          percent = (100L*(pwrite - pread) + buffer_size/2L)/buffer_size;
        }
      else
        {
          // So: pread > pwrite
          percent = (100L*((pend - pread) + (pwrite - pstart)) +
                     buffer_size/2L)/buffer_size;
        }
    }
  return percent;
}

// ----------------------------------------------------------------------------

void FlxReceiver::startDataEmulator( bool external )
{
  // Note (11 Oct 2016): possibly serve a full-mode emulator;
  // from an email from Julia:
  /*
    - enable FULL mode data Emulator
    - set the fan-out select to the emulator  
    - enable the FULL mode to-Host channel
    - latch the 'enable of the FULL mode to-Host channel' 

    ./flx-config set CR_FH_GBT23_EGROUP4_CTRL=0x40
    ./flx-config set GBT_DNLNK_FO_SEL=0x01
    ./flx-config set CR_TH_GBT00_EGROUP0_CTRL=0x01
    ./flx-config set CR_TH_UPDATE_CTRL=0x1
  */

  // Start the data emulator on the FLX-card
  if( _flx == 0 ) return;

  _mutex.lock();
  bool locked = this->fanOutLocked();
  if( external )
    {
      if( !locked )
        {
          // Select real GBT link for all CentralRouter channels
          _flx->cfg_set_option( BF_GBT_TOHOST_FANOUT_SEL, 0 );

          // Enable GBT_EMU for all GBTs ('to frontend')
          _flx->cfg_set_option( BF_GBT_TOFRONTEND_FANOUT_SEL, 0xFFFFFF );
        }
      // 'TOFRONTEND' emulator enable
#if REGMAP_VERSION < 0x500
      _flx->cfg_set_option( BF_GBT_EMU_ENA_TOHOST, 0 );
      _flx->cfg_set_option( BF_GBT_EMU_ENA_TOFRONTEND, 1 );
      _flx->cfg_set_option( BF_GBT_FM_EMU_ENA_TOHOST, 0 ); // FULL-mode
#else
      _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOHOST, 0 );
      _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOFRONTEND, 1 );
#endif
    }
  else
    {
      if( !locked )
        {
          // Enable GBT_EMU for all GBTs ('to host')
          _flx->cfg_set_option( BF_GBT_TOHOST_FANOUT_SEL, 0xFFFFFF );

          // Select CentralRouter data for all GBT links
          _flx->cfg_set_option( BF_GBT_TOFRONTEND_FANOUT_SEL, 0 );
        }
      if( _flx->cfg_get_reg( REG_FIRMWARE_MODE ) == 1 )
        {
          // 'TOHOST' FULL-mode emulator enable
#if REGMAP_VERSION < 0x500
          _flx->cfg_set_option( BF_GBT_FM_EMU_ENA_TOHOST, 1 );
#else
          _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOHOST, 1 );
#endif
        }
      else
        {
          // 'TOHOST' emulator enable
#if REGMAP_VERSION < 0x500
          _flx->cfg_set_option( BF_GBT_EMU_ENA_TOFRONTEND, 0 );
          _flx->cfg_set_option( BF_GBT_EMU_ENA_TOHOST, 1 );
#else
          _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOFRONTEND, 0 );
          _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOHOST, 1 );
#endif
        }
    }

  _mutex.unlock();
}

// ----------------------------------------------------------------------------

void FlxReceiver::stopDataEmulator()
{
  // Stop the data emulator on the FLX-card
  if( _flx == 0 ) return;

  _mutex.lock();

  // Disable emulators
#if REGMAP_VERSION < 0x500
  _flx->cfg_set_option( BF_GBT_EMU_ENA_TOHOST, 0 );
  _flx->cfg_set_option( BF_GBT_EMU_ENA_TOFRONTEND, 0 );
  _flx->cfg_set_option( BF_GBT_FM_EMU_ENA_TOHOST, 0 ); // FULL-mode
#else
  _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOHOST, 0 );
  _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOFRONTEND, 0 );
#endif

  bool locked = this->fanOutLocked();
  if( !locked )
    {
      // Select CentralRouter data for all GBT links
      // ("up/downlink fanout select")
      _flx->cfg_set_option( BF_GBT_TOFRONTEND_FANOUT_SEL, 0 );
      // Slowly crap data enters the CR! (when GTH-links not connected?),
      //_flx->cfg_set_option( BF_GBT_TOHOST_FANOUT_SEL, 0xFFFFFF );
      // 19 Mar 2020: leave ToHost fan-out untouched..
    }

  _mutex.unlock();
}

// ----------------------------------------------------------------------------

void FlxReceiver::setFanOutForDaq()
{
  this->stopDataEmulator();

  if( _flx == 0 ) return;

  // (Set "downlink fanout select")
  _mutex.lock();
  bool locked = this->fanOutLocked();
  if( !locked )
    {
      _flx->cfg_set_option( BF_GBT_TOFRONTEND_FANOUT_SEL, 0 );
      _flx->cfg_set_option( BF_GBT_TOHOST_FANOUT_SEL, 0 );
    }
  _mutex.unlock();
}

// ----------------------------------------------------------------------------

bool FlxReceiver::fanOutLocked()
{
  if( _flx == 0 ) return false;

  // Check for the FO_SEL lock bit(s) (added 11 July 2016)
  bool locked = false;
  if( _flx->cfg_get_option( BF_GBT_TOHOST_FANOUT_LOCK ) == 1 )
    locked = true;
  if( _flx->cfg_get_option( BF_GBT_TOFRONTEND_FANOUT_LOCK ) == 1 )
    locked = true;
  return locked;
}

// ----------------------------------------------------------------------------

std::string FlxReceiver::firmwareVersion()
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

int FlxReceiver::firmwareMode()
{
  if( _flx == 0 ) return 0xFF;
  if( _firmwareMode == -1 )
    _firmwareMode = _flx->cfg_get_reg( REG_FIRMWARE_MODE );
  return _firmwareMode;
}

// ----------------------------------------------------------------------------

int FlxReceiver::numberOfChans()
{
  if( _flx == 0 ) return 0;
  if( _nChans == -1 )
    _nChans = _flx->cfg_get_reg( REG_NUM_OF_CHANNELS );
  return _nChans;
}

// ----------------------------------------------------------------------------

int FlxReceiver::ecIndex()
{
  if( _flx == 0 ) return 0;
#if REGMAP_VERSION < 0x500
  return 0x3F;
#else
  return (int) _flx->cfg_get_option( BF_AXI_STREAMS_TOHOST_EC_INDEX );
#endif // REGMAP_VERSION
}

// ----------------------------------------------------------------------------

int FlxReceiver::icIndex()
{
  if( _flx == 0 ) return 0;
#if REGMAP_VERSION < 0x500
  return 0x3E;
#else
  return (int) _flx->cfg_get_option( BF_AXI_STREAMS_TOHOST_IC_INDEX );
#endif // REGMAP_VERSION
}

// ----------------------------------------------------------------------------
// Private functions
// ----------------------------------------------------------------------------

void FlxReceiver::suspend()
{
  if( _pThread )
    {
      // Prevent thread from writing anything more into the buffer
      _suspend = true;
      // Wake up thread from irq_wait() in run()
      if( _flx )
        _flx->irq_cancel( _irqIndex );
      // Now wait to make sure this thread takes '_suspend' into account...
      volatile bool b = _suspended;
      while( !b ) b = this->suspended(); // Function to prevent optimization
    }
}

// ----------------------------------------------------------------------------

bool FlxReceiver::suspended()
{
  // This function was only added to prevent the compiler from optimizing
  // a while-loop (see suspend()) on the _suspended variable!
  return _suspended;
}

// ----------------------------------------------------------------------------

void FlxReceiver::resume()
{
  // Resume normal receiving operation
  _suspend   = false;
  _suspended = false;
}

// ----------------------------------------------------------------------------

char *FlxReceiver::cmemAllocate( u64 buffer_size, int id )
{
  u64   cmemPhysAddr = 0;
  char *buffer = 0;

  // Open CMEM(_RCC) and allocate the requested buffer size
#ifdef USE_LIB_CMEM
  u64 cmemVirtAddr;
  int ret = CMEM_Open();
  ostringstream oss;
  oss << "FlxReceiver";
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
      if( !_errString.empty() )
        _errString += "; ";
      std::ostringstream oss;
      oss << "CMEM_RCC error: 0x" << std::hex << ret;
      _errString += oss.str();
    }

#else
  if( (_cmemHandle = open("/dev/cmem_rcc", O_RDWR)) < 0 )
    {
      if( !_errString.empty() )
        _errString += "; ";
      _errString += "Failed to open /dev/cmem_rcc";
      _running = false;
      return 0;
    }

  if( id >= 0 )
    sprintf( _cmemDescriptor.name, "FlxReceiver%d", id );
  else
    sprintf( _cmemDescriptor.name, "FlxReceiver" );
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
          if( !_errString.empty() )
            _errString += "; ";
          _errString += "mmap/ioctl(SETUADDR) failed";
        }
    }
  else
    {
      if( !_errString.empty() )
        _errString += "; ";
      _errString += "ioctl(CMEM_RCC_GET) failed";
    }
#endif // USE_LIB_CMEM
  if( buffer == 0 )
    {
      _running = false;
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

void FlxReceiver::dmaStart( u64  dst_phys_addr,
                            u64  size,
                            int  dma_index,
                            bool reset )
{
  if( !(_flx != 0 && dst_phys_addr != 0 && size > 0)  )
    return;

  // Just in case...
  //this->stopDataEmulator(); //###Also sets fan-out regs: better not
  this->dmaStop( dma_index, reset );

  volatile flxcard_bar0_regs_t *bar0 =
    (volatile flxcard_bar0_regs_t *) _flx->bar0Address();
  _dmaDesc = &bar0->DMA_DESC[dma_index];
  _dmaStat = &bar0->DMA_DESC_STATUS[dma_index];

  _mutex.lock();

  // Enable data-available interrupt
  _flx->irq_enable( _irqIndex );

  int tlp = _flx->dma_max_tlp_bytes();
  _tlpSizeMask = ~((u64)tlp - 1);

  _flx->dma_to_host( dma_index, dst_phys_addr, size,
                     /*tlp,*/ FLX_DMA_WRAPAROUND );

  // If we don't sleep, _dmaReadPtr is 0 after the next statement!?
  std::this_thread::sleep_for( std::chrono::microseconds(1000) );
  _dmaReadPtr  = _dmaDesc->read_ptr; // Where we are, processing the data
  _dmaWritePtr = dst_phys_addr;      // Where we are, counting the received data

  _mutex.unlock();
}

// ----------------------------------------------------------------------------

void FlxReceiver::dmaStop( int dma_index, bool reset )
{
  if( _flx == 0 ) return;

  _mutex.lock();

  _flx->dma_stop( dma_index );

  // Reset?
  if( reset )
    _flx->dma_reset();

  // Flush
  _flx->soft_reset();

  _mutex.unlock();
}

// ----------------------------------------------------------------------------
