#ifndef FILEWRITER_H
#define FILEWRITER_H

#include <thread>
#include "flxdefs.h"

typedef int64_t  i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

class FlxReceiver;

class FileWriter
{
 public:
  FileWriter( FlxReceiver *recvr );
  ~FileWriter( );

  void  stop          ( );
  void  run           ( );

  void  setFlush      ( bool enable )     { _flush = enable; }
  void  enableBackpressure( bool enable ) { _delayIt = enable; }
  void  setBlockSize  ( int size )        { _blockSize = size; }

  // File operations
  bool startRecording ( std::string filename,
                        int         runnr,
                        bool        file_per_elink = false );
  bool stopRecording  ( );
  i64  fileMaxSize    ( )             { return _fileMaxSize; }
  void setFileMaxSize ( i64 size )    { if( size > 0 ) _fileMaxSize = size; }
  i64  fileChunkSize  ( )             { return _fileChunkSize; }
  void setFileChunkSize( i64 size )   { if( size > 0 ) _fileChunkSize = size; }
  std::string fileName( )             { return _fileName; }
  int  fileCount      ( )             { return _fileCount; }
  void setAddDateTime ( bool b )      { _addDateTime = b; }

  // Statistics
  i64  bytesWritten   ( )             { return _bytesToFile; }
  i64  bytesRecorded  ( )             { return _bytesRecorded; }
  i64  bytesRecordedInRun( )          { return _bytesRecordedInRun; }
  i64  bytesFlushed   ( )             { return _bytesFlushed; }

  // Data check
  i64  blocksChecked  ( )             { return _blocksChecked; }
  i64  blockErrors    ( )             { return _blockErrors; }
  i64  trailerErrors  ( )             { return _trailerErrors; }
  i64  truncErrors    ( )             { return _truncErrors; }
  i64  errErrors      ( )             { return _errErrors; }
  i64  crcErrors      ( )             { return _crcErrors; }
  i64  lengthErrors   ( )             { return _lengthErrors; }
  i64  typeErrors     ( )             { return _typeErrors; }
  i64  blockErrorIndex( )             { return _blockErrorIndex; }
  u8  *blockErrorData ( )             { return _errorBlock; }
  int  blockErrorDataSize( )          { return _blockSize; }
                                        //sizeof(_errorBlock); }
  u64  chunkCount     ( int elinknr ) { return _chunkCount[elinknr]; }
  void displayChunkCounts( );

  // Error
  std::string errorString( );
  void        clearErrorString( )     { _errString.clear(); }
  std::string debugString( );

 private:
  bool        openFilePrivate ( );
  void        closeFilePrivate( );
  std::string makeFileName    ( int elink_nr = -1 );
  void        writeToEfiles   ( char *data, i64 bytes );
  void        blockCheck      ( unsigned char *data, i64 bytes );

 private:
  std::thread *_pThread;
  bool         _running;

  // Pointer to receiver
  FlxReceiver *_receiver;
  int          _blockSize, _blockSizeCode;
  FlxTrailerFormat _trailerFmt;

  // Data file writing
  FILE       *_file;
  bool        _fileOpen;      // A file is currently open
  std::string _fileDirName;   // File directory
  std::string _fileBaseName;  // File name without added runnr, datetime,
                              // counter and extension
  std::string _fileDateTime;  // String with start-of-run date+time
  std::string _fileExt;       // File extension to use
  std::string _fileName;      // Full current file name
  int         _fileCntr;      // Counter included in file name
  int         _fileCount;     // Number of files opened (and written to)
  bool        _recording;     // Whether data is written to file(s)
  bool        _flush;         // Flush or not, when data sampling
                              // is not enabled or file not opened
  i64         _fileChunkSize; // Max data chunk size to write in one go
  i64         _fileMaxSize;   // Maximum file size (before auto-opening new one)
  bool        _addDateTime;   // Whether to add datetime to file names
  bool        _addCounter;    // Whether to add a counter to file names

  // E-link data file writing
#define EFILES (FLX_MAX_ELINK_NR+1)
  bool        _filePerElink;
  FILE       *_eFile[EFILES];
  i64         _bytesToEfile[EFILES];
  int         _eFileCntr[EFILES];

  // Run info
  int         _runNr;

  // Statistics
  i64         _bytesToFile;
  i64         _bytesRecorded;
  i64         _bytesRecordedInRun;
  i64         _bytesFlushed;

  // Data block check
  i64         _blocksChecked, _blockErrors, _blockErrorIndex;
  i64         _trailerErrors;
  i64         _truncErrors, _errErrors, _crcErrors, _lengthErrors, _typeErrors;
  u8          _errorBlock[16384];
  bool        _delayIt; // For testing buffer overflow conditions
  int         _delayUs; // For testing buffer overflow conditions
  u64         _chunkCount[FLX_MAX_ELINK_NR+1];

  // String containing a description of the last error that occurred
  std::string _errString;
};

#endif // FILEWRITER_H
