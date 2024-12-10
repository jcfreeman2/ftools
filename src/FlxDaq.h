#ifndef FLXDAQ_H
#define FLXDAQ_H

#include <string>

class FlxReceiver;
class FileWriter;

class FlxDaq
{
 public:
  // C'tor, d'tor
  FlxDaq( int card_nr, int64_t buffer_size, int dma_index = 0,
          bool reset = true );
  ~FlxDaq();

  void stop                       ( );
  void setUseInterrupt            ( bool b );

  // General
  int         classVersion        ( ); // Version of this class
  std::string infoString          ( );
  std::string errorString         ( );
  bool        hasError            ( );
  std::string debugString         ( );

  // Data acquisition
  bool        startRecording      ( std::string filename,
				    int         runnr = 0,
                                    bool        per_elink = false );
  bool        stopRecording       ( );
  void        startEmulator       ( bool external );
  void        stopEmulator        ( );
  void        setFanOutForDaq     ( );
  bool        fanOutLocked        ( );
  std::string fileName            ( );
  int64_t     fileMaxSize         ( );
  void        setFileMaxSize      ( int64_t size );
  int64_t     fileChunkSize       ( );
  void        setFileChunkSize    ( int64_t size );
  void        setFileNameDateTime ( bool enable );
  void        setFlush            ( bool enable );
  void        enableBackpressure  ( bool enable );
  int64_t     bufferSize          ( );
  bool        setBufferSize       ( int64_t size );
  bool        bufferEmpty         ( );
  bool        bufferFull          ( );
  bool        bufferFullOccurred  ( );
  void        resetBufferFullOccurred();
  char       *dataBuffer          ( );
  std::string firmwareVersionString( );
  int         firmwareMode        ( );
  int         numberOfChans       ( );
  int         blockSize           ( );
  void        setBlockSize        ( int size );
  bool        trailerIs32Bit      ( );

  // Statistics and info
  int64_t     bytesAvailable      ( );
  int64_t     bytesReceivedCount  ( );
  int64_t     bytesWrittenCount   ( );
  int64_t     bytesRecordedCount  ( );
  int64_t     bytesRecordedInRunCount( );
  int64_t     bytesFlushedCount   ( );
  int         bufferWrapCount     ( );
  int         bufferFullPercentage( );
  int         fileCount           ( );

  // Data checks
  int64_t     blocksChecked       ( );
  int64_t     blockErrors         ( );
  int64_t     trailerErrors       ( );
  int64_t     truncErrors         ( );
  int64_t     errErrors           ( );
  int64_t     crcErrors           ( );
  int64_t     lengthErrors        ( );
  int64_t     typeErrors          ( );
  int64_t     blockErrorIndex     ( );
  uint8_t    *blockErrorData      ( );
  int         blockErrorDataSize  ( );
  uint64_t    eLinkChunkCount     ( int elinknr );
  void        displayChunkCounts  ( );

 private:
  FlxReceiver *_dataReceiver;
  FileWriter  *_fileWriter;
};

#endif // FLXDAQ_H
