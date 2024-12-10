#include "FlxDaq.h"
#include "FlxReceiver.h"
#include "FileWriter.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x18013100;
//const int VERSION_ID = 0x16032200;
//const int VERSION_ID = 0xXXXXXXXX;

// ----------------------------------------------------------------------------
// Constructor / destructor / info
// ----------------------------------------------------------------------------

FlxDaq::FlxDaq( int card_nr, int64_t buffer_size, int dma_index, bool reset )
  : _dataReceiver( 0 ),
    _fileWriter( 0 )
{
  // Start data-acquisition from the FLX-card with the given number

  // Allocate default buffer size
  _dataReceiver = new FlxReceiver( card_nr, buffer_size, dma_index, reset );

  // Create the file-writer thread, providing it with a link to
  // the reader thread
  _fileWriter = new FileWriter( _dataReceiver );
}

// ----------------------------------------------------------------------------

FlxDaq::~FlxDaq()
{
  this->stop();
}

// ----------------------------------------------------------------------------

void FlxDaq::stop()
{
  if( _fileWriter )
    {
      _fileWriter->stop();
      delete _fileWriter;
      _fileWriter = 0;
    }
  if( _dataReceiver )
    {
      _dataReceiver->stop();
      delete _dataReceiver;
      _dataReceiver = 0;
    }
}

// ----------------------------------------------------------------------------

void FlxDaq::setUseInterrupt( bool b )
{
  if( _dataReceiver )
    _dataReceiver->setUseInterrupt( b );
}

// ----------------------------------------------------------------------------
// General
// ----------------------------------------------------------------------------

int FlxDaq::classVersion()
{
  return VERSION_ID;
}

// ----------------------------------------------------------------------------

std::string FlxDaq::infoString()
{
  return _dataReceiver->infoString();
}

// ----------------------------------------------------------------------------

std::string FlxDaq::errorString()
{
  std::string str;
  str += _dataReceiver->errorString();
  if( !str.empty() && !_fileWriter->errorString().empty() )
    str += std::string( ", " );
  str += _fileWriter->errorString();

  // Clear the error strings...
  _dataReceiver->clearErrorString();
  _fileWriter->clearErrorString();

  return str;
}

// ----------------------------------------------------------------------------

bool FlxDaq::hasError()
{
  if( !_dataReceiver->errorString().empty() ||
      !_fileWriter->errorString().empty() )
    return true;
  return false;
}

// ----------------------------------------------------------------------------

std::string FlxDaq::debugString()
{
  std::string str;
  str += _dataReceiver->debugString();
  if( !str.empty() )
    str += std::string( "\n" );
  str += _fileWriter->debugString();
  return str;
}

// ----------------------------------------------------------------------------
// Acquisition
// ----------------------------------------------------------------------------

bool FlxDaq::startRecording( std::string filename, int runnr, bool per_elink )
{
  return _fileWriter->startRecording( filename, runnr, per_elink );
}

// ----------------------------------------------------------------------------

bool FlxDaq::stopRecording()
{
  return _fileWriter->stopRecording();
}

// ----------------------------------------------------------------------------

void FlxDaq::startEmulator( bool external )
{
  _dataReceiver->startDataEmulator( external );
}

// ----------------------------------------------------------------------------

void FlxDaq::stopEmulator()
{
  _dataReceiver->stopDataEmulator();
}

// ----------------------------------------------------------------------------

void FlxDaq::setFanOutForDaq()
{
  _dataReceiver->setFanOutForDaq();
}

// ----------------------------------------------------------------------------

bool FlxDaq::fanOutLocked()
{
  return _dataReceiver->fanOutLocked();
}

// ----------------------------------------------------------------------------

std::string FlxDaq::fileName()
{
  return _fileWriter->fileName();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::fileMaxSize()
{
  return _fileWriter->fileMaxSize();
}

// ----------------------------------------------------------------------------

void FlxDaq::setFileMaxSize( int64_t size )
{
  _fileWriter->setFileMaxSize( size );
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::fileChunkSize()
{
  return _fileWriter->fileChunkSize();
}

// ----------------------------------------------------------------------------

void FlxDaq::setFileChunkSize( int64_t size )
{
  _fileWriter->setFileChunkSize( size );
}

// ----------------------------------------------------------------------------

void FlxDaq::setFileNameDateTime( bool enable )
{
  _fileWriter->setAddDateTime( enable );
}

// ----------------------------------------------------------------------------

void FlxDaq::setFlush( bool enable )
{
  _fileWriter->setFlush( enable );
}

// ----------------------------------------------------------------------------

void FlxDaq::enableBackpressure( bool enable )
{
  _fileWriter->enableBackpressure( enable );
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::bufferSize()
{
  return _dataReceiver->bufferSize();
}

// ----------------------------------------------------------------------------

bool FlxDaq::setBufferSize( int64_t size )
{
  return _dataReceiver->setBufferSize( size );
}

// ----------------------------------------------------------------------------

bool FlxDaq::bufferEmpty()
{
  return _dataReceiver->empty();
}

// ----------------------------------------------------------------------------

bool FlxDaq::bufferFull()
{
  return _dataReceiver->full();
}

// ----------------------------------------------------------------------------

bool FlxDaq::bufferFullOccurred()
{
  return _dataReceiver->fullOccurred();
}

// ----------------------------------------------------------------------------

void FlxDaq::resetBufferFullOccurred()
{
  _dataReceiver->resetFullOccurred();
}

// ----------------------------------------------------------------------------

char *FlxDaq::dataBuffer()
{
  // To allow direct access to the 'raw' data buffer
  return _dataReceiver->data();
}

// ----------------------------------------------------------------------------

std::string FlxDaq::firmwareVersionString()
{
  return _dataReceiver->firmwareVersion();
}

// ----------------------------------------------------------------------------

int FlxDaq::firmwareMode()
{
  int mode = _dataReceiver->firmwareMode();
  // 0: GBT mode
  // 1: FULL mode
  // 2: LTDB mode (GBT mode with only IC and TTC links)
  // 3: FEI4 mode
  // 4: ITK-PIXEL mode
  // 5: ITK-STRIP mode
  // 6: FELIG mode
  // 7: FULL-EMU mode
  // 8: MROD mode
  // 9: lpGBT mode
  if( mode <= 9 && mode >= 0 )
    return mode;
  else
    return -1;
}

// ----------------------------------------------------------------------------

int FlxDaq::numberOfChans()
{
  return _dataReceiver->numberOfChans();
}

// ----------------------------------------------------------------------------

int FlxDaq::blockSize()
{
  return _dataReceiver->blockSize();
}

// ----------------------------------------------------------------------------

void FlxDaq::setBlockSize( int size )
{
  _dataReceiver->setBlockSize( size );
  _fileWriter->setBlockSize( size );
}

// ----------------------------------------------------------------------------

bool FlxDaq::trailerIs32Bit()
{
  return _dataReceiver->trailerIs32Bit();
}

// ----------------------------------------------------------------------------
// Statistics
// ----------------------------------------------------------------------------

int64_t FlxDaq::bytesAvailable()
{
  return _dataReceiver->bytesAvailable();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::bytesReceivedCount()
{
  return _dataReceiver->bytesReceived();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::bytesWrittenCount()
{
  return _fileWriter->bytesWritten();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::bytesRecordedCount()
{
  return _fileWriter->bytesRecorded();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::bytesRecordedInRunCount()
{
  return _fileWriter->bytesRecordedInRun();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::bytesFlushedCount()
{
  return _fileWriter->bytesFlushed();
}

// ----------------------------------------------------------------------------

int FlxDaq::bufferWrapCount()
{
  return _dataReceiver->bufferWraps();
}

// ----------------------------------------------------------------------------

int FlxDaq::bufferFullPercentage()
{
  return (int) _dataReceiver->bufferFullPercentage();
}

// ----------------------------------------------------------------------------

int FlxDaq::fileCount()
{
  return _fileWriter->fileCount();
}

// ----------------------------------------------------------------------------
// Data checks
// ----------------------------------------------------------------------------

int64_t FlxDaq::blocksChecked()
{
  return _fileWriter->blocksChecked();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::blockErrors()
{
  return _fileWriter->blockErrors();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::trailerErrors()
{
  return _fileWriter->trailerErrors();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::truncErrors()
{
  return _fileWriter->truncErrors();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::errErrors()
{
  return _fileWriter->errErrors();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::crcErrors()
{
  return _fileWriter->crcErrors();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::lengthErrors()
{
  return _fileWriter->lengthErrors();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::typeErrors()
{
  return _fileWriter->typeErrors();
}

// ----------------------------------------------------------------------------

int64_t FlxDaq::blockErrorIndex()
{
  return _fileWriter->blockErrorIndex();
}

// ----------------------------------------------------------------------------

uint8_t *FlxDaq::blockErrorData()
{
  return _fileWriter->blockErrorData();
}

// ----------------------------------------------------------------------------

int FlxDaq::blockErrorDataSize()
{
  return _fileWriter->blockErrorDataSize();
}

// ----------------------------------------------------------------------------

uint64_t FlxDaq::eLinkChunkCount( int elinknr )
{
  return _fileWriter->chunkCount( elinknr );
}

// ----------------------------------------------------------------------------

void FlxDaq::displayChunkCounts()
{
  _fileWriter->displayChunkCounts();
}

// ----------------------------------------------------------------------------
