#ifndef FLXRECEIVER_H
#define FLXRECEIVER_H

#include <thread>
#include <mutex>

typedef int64_t  i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

#include "cmem_rcc/cmem_rcc.h"
#include "flxcard/FlxCard.h"

// Receiver buffer size
//#define DFLT_RECV_BUF_SIZE  0x001000000 //   16 MByte
//#define DFLT_RECV_BUF_SIZE  0x010000000 //  256 MByte
//#define DFLT_RECV_BUF_SIZE  0x020000000 //  512 MByte
#define DFLT_RECV_BUF_SIZE    0x040000000 // 1024 MByte
#define FREE_SPACE_MINIMUM   (16*1024*1024)

class FlxReceiver
{
 public:
  FlxReceiver( int      card_nr,
               i64      buffer_size = DFLT_RECV_BUF_SIZE,
               int      dma_index = 0,
               bool     reset = false );
  ~FlxReceiver( );

  // General
  void        stop            ( );
  void        run             ( );
  std::string infoString      ( );
  std::string errorString     ( );
  void        clearErrorString( ) { _errString.clear(); }
  bool        hasError        ( );
  std::string debugString     ( );

  // Data reception
  void  setUseInterrupt       ( bool b );
  bool  hasData               ( ) { return( _head != _tail || _full ); }
  char *data                  ( ) { return &_dataBuffer[_tail]; }
  i64   bytesAvailable        ( );
  void  updateBytesConsumed   ( i64 bytes );
  bool  empty                 ( ) { return ( _head == _tail && !_full ); }
  bool  full                  ( ) { return _full; }
  bool  fullOccurred          ( ) { return _fullOccurred; }
  void  resetFullOccurred     ( ) { _fullOccurred = false; }
  char *buffer                ( ) { return _dataBuffer; }
  bool  setBufferSize         ( i64 size );
  i64   bufferSize            ( ) { return _bufferSize; }
  u64   bufferFullPercentage  ( );

  i64   bytesReceived         ( ) { return _bytesReceived; }
  int   bufferWraps           ( ) { return _bufferWraps; }
  int   debugCounter          ( ) { return _debugCounter; }
  u64   timestamp             ( ) { u64 t = _timestamp; _timestamp = 0;
                                    return t; }
  int   blockSize( )              { return _blockSize; }
  void  setBlockSize( int size )  { _blockSize = size; }
  bool  trailerIs32Bit        ( ) { return _trailerIs32Bit; }

  // Emulator
  void  startDataEmulator     ( bool external = false );
  void  stopDataEmulator      ( );
  void  setFanOutForDaq       ( );
  bool  fanOutLocked          ( );

  // Info
  std::string firmwareVersion ( );
  int   firmwareMode          ( );
  int   numberOfChans         ( );
  int   ecIndex               ( );
  int   icIndex               ( );

 private:
  void  suspend     ( );
  bool  suspended   ( );
  void  resume      ( );

  char *cmemAllocate( u64 buffer_size, int id = -1 );
  void  dmaStart    ( u64 dst_phys_addr, u64 size, int dma_index = 0,
                      bool reset = true );
  void  dmaStop     ( int dma_index = 0, bool reset = false );

 private:
  std::thread *_pThread;
  std::mutex   _mutex; // To guard multithreaded access to FlxCard registers
  FlxCard     *_flx;
  int          _firmwareMode, _nChans;
  int          _blockSize;
  bool         _trailerIs32Bit;
  bool         _running;
  int          _cardNr;
  bool         _useInterrupt;
  bool         _suspend, _suspended;

  // Statistics
  i64      _bytesReceived;
  int      _lastBlockSize, _lastBytesAvailable;
  int      _bufferWraps, _fullCounter;
  int      _debugCounter;

  // String containing a description of the last error that occurred
  std::string _errString;

  // Buffer administration
  i64      _bufferSize;
  u64      _dmaWritePtr, _dmaReadPtr;
  i64      _head, _tail; // Buffer indices reflecting _writePtr and _readPtr
  u64      _tlpSizeMask;
  i64      _freeSpaceMinimum;
  bool     _full, _fullOccurred;

  // Receive timestamp
  u64      _timestamp;

  // Pointer to allocated data receive buffer
  char    *_dataBuffer;

  // CMEM stuff
  int        _cmemHandle;
  cmem_rcc_t _cmemDescriptor;
  u64        _cmemStartAddr, _cmemEndAddr;

  // DMA
  int      _dmaIndex;
  int      _irqIndex;
  volatile dma_descriptor_t *_dmaDesc;
  volatile dma_status_t     *_dmaStat;
};

#endif // FLXRECEIVER_H
