#include <sstream> // for ostringstream
#include <iostream> // for displayChunkCounts()
#include <iomanip>
#include <string>
#include <sys/stat.h> // for stat
#include <string.h>   // for memcpy
#ifdef WIN32
#include <direct.h>   // for _mkdir
#endif

#include "FileWriter.h"
#include "FlxReceiver.h"
#include "flxdefs.h" // For data block check

// ----------------------------------------------------------------------------

FileWriter::FileWriter( FlxReceiver *recvr )
  : _pThread( 0 ),
    _running( true ),
    _receiver( recvr ),
    _blockSize( 1024 ),
    _file( 0 ),
    _fileOpen( false ),
    _fileDirName( "." ),
    _fileBaseName( "tpx" ),
    _fileDateTime( "" ),
    _fileExt( "dat" ),
    _fileName( "" ),
    _fileCntr( 1 ),
    _fileCount( 0 ),
    _recording( false ),
    _flush( true ),
    _fileChunkSize( 0x1000000 ), // 16 MB max per write operation
    _fileMaxSize( 0x100000000 ),  // 4 GB file size
    _addDateTime( true ),
    _addCounter( true ),
    _filePerElink( false ),
    _runNr( 0 ),
    _bytesToFile( 0 ),
    _bytesRecorded( 0 ),
    _bytesRecordedInRun( 0 ),
    _bytesFlushed( 0 ),
    _blocksChecked( 0 ),
    _blockErrors( 0 ),
    _blockErrorIndex( -1 ),
    _trailerErrors( 0 ),
    _truncErrors( 0 ),
    _errErrors( 0 ),
    _crcErrors( 0 ),
    _lengthErrors( 0 ),
    _typeErrors( 0 ),
    _delayIt( false ),// For testing buffer overflow conditions
    _delayUs( 1 )     // For testing buffer overflow conditions
{
  if( _receiver )
    {
      _blockSize = _receiver->blockSize();
      _trailerFmt.setTrailerIs32Bit( _receiver->trailerIs32Bit() );
    }
  _blockSizeCode = (_blockSize/1024-1) << 8;

  // Initialize per-elink stuff
  for( int i=0; i<EFILES; ++i )
    {
      _eFile[i]        = 0;
      _bytesToEfile[i] = 0;
      _eFileCntr[i]    = 1;
      _chunkCount[i]   = 0;
    }

  // Start the thread (see run())
  _pThread = new std::thread( [this](){ this->run(); } );
}

// ----------------------------------------------------------------------------

FileWriter::~FileWriter()
{
  // In case the thread is still running...
  this->stop();
}

// ----------------------------------------------------------------------------

void FileWriter::stop()
{
  _running = false;

  if( _pThread )
    // Wait until the receive thread (i.e. function run()) exits
    _pThread->join();
  _pThread = 0;
}

// ----------------------------------------------------------------------------

void FileWriter::run()
{
  i64 bytes;

  if( _receiver == 0 )
    _running = false;

  while( _running )
    {
      // Handle file opening and closing
      if( _recording )
        {
          if( !_fileOpen || _bytesToFile > _fileMaxSize )
            this->openFilePrivate();
        }
      else
        {
          if( _fileOpen )
            this->closeFilePrivate();
        }

      if( _receiver->hasData() )
        {
          if( _fileOpen )
            {
              // File is open: write data to file

              // Adhere to file-write chunk size
              bytes = _receiver->bytesAvailable();
              if( bytes > _fileChunkSize )
                bytes = _fileChunkSize;

              if( _delayIt )
                bytes = _blockSize; // One block at a time

              if( _filePerElink )
                {
                  this->writeToEfiles( _receiver->data(), bytes );
                }
              else
                {
                  bytes = fwrite( _receiver->data(), 1, bytes, _file );
                  fflush( _file );
                  _bytesToFile += bytes;
                  if( _delayIt )
                    std::this_thread::
                      sleep_for( std::chrono::microseconds(_delayUs) );
                }
              _bytesRecorded += bytes;
              _bytesRecordedInRun += bytes;

              // Notify the receiver
              _receiver->updateBytesConsumed( bytes );
            }
          else
            {
              // If not writing to file, flush the data
              // if flushing is enabled, otherwise keep it stored in buffer
              // NB: do a basic check on the data blocks when flushing
              //     (added 20 Apr 2016)
              if( _flush )
                {
                  bytes = _receiver->bytesAvailable();
                  if( _delayIt )
                    bytes = _blockSize; // One block at a time
                  this->blockCheck( (unsigned char *)_receiver->data(), bytes );
                  _receiver->updateBytesConsumed( bytes );
                  _bytesFlushed += bytes;
                }
            }
        }
      else
        {
          // Doze off briefly, while waiting for new data...
          std::this_thread::sleep_for( std::chrono::milliseconds(20) );
        }
    }
  this->closeFilePrivate(); // In case a file is still open
}

// ----------------------------------------------------------------------------

bool FileWriter::startRecording( std::string filename,
                                 int         runnr,
                                 bool        file_per_elink )
{
  if( !this->stopRecording() ) return false;

  _filePerElink = file_per_elink;

  // Separate given name in a path (optional), a file name and
  // an extension (optional)
  size_t last_i;
  std::string fname = filename;
  for( unsigned int i=0; i<fname.length(); ++i )
    if( fname[i] == '\\' ) fname[i] = '/';
  // Path name present?
  if( fname.find( '/' ) != std::string::npos )
    {
      last_i        = fname.find_last_of( '/' );
      _fileDirName  = fname.substr( 0, last_i );
      _fileBaseName = fname.substr( last_i+1 );
    }
  else
    {
      _fileDirName  = ".";
      _fileBaseName = fname;
    }
  // Extension present?
  if( _fileBaseName.find( '.' ) != std::string::npos )
    {
      last_i        = _fileBaseName.find_last_of( '.' );
      _fileExt      = _fileBaseName.substr( last_i+1 );
      _fileBaseName = _fileBaseName.substr( 0, last_i );
    }
  else
    {
      _fileExt = "dat";
    }
  _fileDateTime.clear();

  // Check existence of the directory and create if necessary
  // (empty _fileDirName equals ".")
  if( !(_fileDirName == ".") )
    {
      struct stat info;
      if( stat(_fileDirName.c_str(),&info) != 0 )
        {
#ifndef WIN32
          if( mkdir(_fileDirName.c_str(), 0777) == -1 )
#else
          if( _mkdir(_fileDirName.c_str()) == -1 )
#endif
            {
              _errString = "Failed to create dir \"" + _fileDirName + "\"";
              return false;
            }
        }
      else
        {
          if( !(info.st_mode & S_IFDIR) )
            {
              _errString = "File \"" + _fileDirName +
                "\" exists, not a directory";
              return false;
            }
        }
    }

  // Initialize file counter when the run number changes
  if( runnr != _runNr )
    {
      _fileCntr = 1;
      _fileCount = 0;
      for( int i=0; i<EFILES; ++i )
        _eFileCntr[i] = 1;
      _bytesRecordedInRun = 0;
    }
  _bytesRecorded = 0;

  // Remember run number (for file names)
  _runNr = runnr;

  // Instruct the thread to open a file
  _recording = true;

  // Wait for file opened, with time-out
  int cnt = 0;
  while( !_fileOpen && cnt < 200 )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds(10) );
      ++cnt;
    }
  if( cnt >= 200 )
    {
      _errString = "Time-out (>2s) opening file in dir \"" +
        _fileDirName + "\"";  
      return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

bool FileWriter::stopRecording()
{
  // Return false only in case of time-out on closing the file

  if( !_recording ) return true;

  // Instruct the thread to close any open file
  _recording = false;

  // Wait for file closed, with time-out (3 secs?)
  int cnt = 0;
  while( _fileOpen && cnt < 1000 )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds(10) );
      ++cnt;
    }
  if( cnt >= 1000 )
    {
      _errString = "Time-out (>10 s) closing file";  
      return false;
    }
  return true;
}

// ----------------------------------------------------------------------------

std::string FileWriter::errorString()
{
  if( _errString.empty() ) return std::string( "" );
  return( "Filewriter: " + _errString );
}

// ----------------------------------------------------------------------------

std::string FileWriter::debugString()
{
  std::string str;
  return str;
}

// ----------------------------------------------------------------------------

void FileWriter::displayChunkCounts()
{
  if( _blocksChecked == 0 )
    {
      std::cout << "=> Elink chunk counts: none" << std::endl;
      return;
    }
  std::cout << "Elink Lnk-i  Chunks" << std::endl
            << "----------- --------" << std::endl
            << std::setfill( '0' );
  for( int elinknr=0; elinknr<=FLX_MAX_ELINK_NR; ++elinknr )
    if( _chunkCount[elinknr] > 0 )
      {
        std::cout << "0x" << std::hex << std::setw(3) << elinknr
                  << std::dec << " " << std::setw(2)
                  << (elinknr >> BLOCK_GBT_SHIFT) << "-" << std::setw(2)
                  << ((elinknr & (BLOCK_EGROUP_MASK | BLOCK_EPATH_MASK)) >>
                      BLOCK_EPATH_SHIFT) << "  "
                  << _chunkCount[elinknr]
                  << std::endl;
      }
}

// ----------------------------------------------------------------------------
// Private functions
// ----------------------------------------------------------------------------

bool FileWriter::openFilePrivate()
{
  // Open file function called by thread
  // (this function is to be called from within the thread only)
  this->closeFilePrivate();

  if( _filePerElink )
    {
      // Files for individual e-links are opened when data for one arrives
      _fileOpen = true;
      return true;
    }

  _fileName = this->makeFileName();
  //_file.open( _fileName.c_str(), std::ofstream::binary );
  //if( _file.is_open() )
  _file = fopen( _fileName.c_str(), "w" );
  if( _file )
    {
      _bytesToFile = 0;
      _fileOpen = true;
      ++_fileCount;
      return true;
    }
  else
    {
      _errString = "Failed to open file \"" + _fileName + "\"";  
      return false;
    }
}

// ----------------------------------------------------------------------------

void FileWriter::closeFilePrivate()
{
  // Close file(s) function called by thread
  // (this function is to be called from within the thread only)

  if( _filePerElink )
    {
      for( int i=0; i<EFILES; ++i )
        if( _eFile[i] )
          {
            fclose( _eFile[i] );
            _eFile[i] = 0;
          }
    }

  if( _file )
    {
      fclose( _file );
      _file = 0;
    }
  _fileOpen = false;

  // ### Keep these counters up-to-date rather than updating
  // ### once when closing a file (changed 14 Oct 2014)
  //_bytesRecorded += _bytesToFile;
  //_bytesRecordedInRun += _bytesToFile;
}

// ----------------------------------------------------------------------------

std::string FileWriter::makeFileName( int elink_nr )
{
  // File name is composed like this:
  // "<dir>/<basename>-<year><month><day>-<hour><min><sec>-<runnr>-<cnt>.<ext>"
  //
  // In case of one file per e-link, the e-link number is inserted between
  // <basename> and <year> (and there is a <cnt> per e-link).

  // The directory
  std::string name = _fileDirName;
  if( _fileDirName.length() > 0 ) name += '/';
  name += _fileBaseName;

  // Add e-link number if applicable
  int file_cntr;
  if( elink_nr >= 0 )
    {
      std::ostringstream oss;
      oss << "-" << std::hex << std::setfill('0') << std::uppercase
          << std::setw(3) << elink_nr;
      name += oss.str();
      file_cntr = _eFileCntr[elink_nr];
      ++_eFileCntr[elink_nr];
    }
  else
    {
      file_cntr = _fileCntr;
      ++_fileCntr;
    }

  // Add date and time (optional)
  if( _addDateTime )
    {
      if( _fileDateTime.empty() )
        {
          char timestr[32];
          time_t rawtime;
          time( &rawtime ); // Get current time
          struct tm *timeinfo;
          timeinfo = localtime( &rawtime );
          strftime( timestr, sizeof(timestr), "-%y%m%d-%H%M%S", timeinfo );
          _fileDateTime = std::string( timestr );
        }
      name += _fileDateTime;
    }

  std::ostringstream oss;
  if( _addCounter )
    {
      if( _runNr != 0 )
        // Add run number, a counter and the extension
        oss << "-" << _runNr << "-" << file_cntr << "." << _fileExt;
      else
        // Add a counter and the extension
        oss << "-" << file_cntr << "." << _fileExt;
    }
  else
    {
      if( _runNr != 0 )
        // Add run number and the extension
        oss << "-" << _runNr << "." << _fileExt;
      else
        // Add the extension
        oss << "." << _fileExt;
    }
  name += oss.str();

  return name;
}

// ----------------------------------------------------------------------------

void FileWriter::writeToEfiles( char *data, i64 bytes )
{
  // Write data to individual files per e-link number, so block-by-block
  int  elinknr;
  u16 *data_s;
  i64  b, blocks = bytes/_blockSize;
  for( b=0; b<blocks; ++b )
    {
      // Extract e-link number from block header
      data_s = (u16 *) data;
      elinknr = (*data_s) & BLOCK_ELINK_MASK;

      // Starting acquisition or file size reached?
      if( _eFile[elinknr] == 0 || _bytesToEfile[elinknr] > _fileMaxSize )
        {
          // Close this data file
          if( _eFile[elinknr] != 0 )
            {
              fclose( _eFile[elinknr] );
              _eFile[elinknr] = 0;
            }

          // Open a file for E-link number 'elinknr'
          std::string file_name = this->makeFileName( elinknr );
          FILE *file = fopen( file_name.c_str(), "w" );
          if( file )
            {
              _eFile[elinknr] = file;
              ++_fileCount;
            }
          else
            {
              _errString = "Failed to open file \"" + file_name + "\"";
            }

          _bytesToEfile[elinknr] = 0;
        }

      if( _eFile[elinknr] )
        {
          fwrite( data, 1, _blockSize, _eFile[elinknr] );
          _bytesToEfile[elinknr] += _blockSize;
        }

      data += _blockSize; // Next data block
    }
}

// ----------------------------------------------------------------------------

void FileWriter::blockCheck( unsigned char *data, i64 bytes )
{
  u16 *data_s;
  i64  b, blocks = bytes/_blockSize;
  for( b=0; b<blocks; ++b )
    {
      // Check for 0xABCD or 0xABCE or 0xC0CE in block header
      data_s = (u16 *) data;
      if( !(data_s[1] == BLOCK_ID ||
	    data_s[1] == _trailerFmt.blockId ||
	    data_s[1] == (BLOCK_ID_PHASE2 | _blockSizeCode)) )
        {
          ++_blockErrors;
          if( _blockErrorIndex < 0 )
            {
              // Copy data blocks around first found error, for inspection
              _blockErrorIndex = _blocksChecked + b; // 'Index' of the block
              //_blockErrorIndex = (i64) data_s[1]; // FOR DEBUGGING
              //memcpy( _errorBlock,
              //      (data-sizeof(_errorBlock)/2), sizeof(_errorBlock) );
              memcpy( _errorBlock, data, _blockSize );
            }
        }
      else
        {
          // Block header okay, now check chunk trailers
          uint32_t elinknr = data_s[0] & BLOCK_ELINK_MASK;

          // Go through the chunks in the block, from *end-of-block* to *begin*,
          uint32_t i, trailer, length, type;
          bool trunc_or_err, crc_err;
          int index = _blockSize - _trailerFmt.nbytes;
          while( index > BLOCKHEADER_BYTES-1 )
            {
              trailer = 0;
              for( i=0; i<_trailerFmt.nbytes; ++i )
                trailer |= (data[index+i] << (i*8));

              length  = trailer & _trailerFmt.lengthMsk;

              type = ((trailer & _trailerFmt.typeMsk) >> _trailerFmt.typeShift);

              // Out-Of-Band or Null chunk trailer implies: no payload data
              // (Null chunk trailer length may be used for debug purposes!)
              if( type == CHUNKTYPE_OUTOFBAND || type == CHUNKTYPE_NULLFILL )
                length = 0;

              trunc_or_err = ((trailer & (_trailerFmt.truncMsk |
                                          _trailerFmt.errorMsk )) != 0);
              crc_err      = ((trailer & _trailerFmt.crcerrMsk) != 0);

              if( trunc_or_err ||
                  type == CHUNKTYPE_UNDEFINED6 )
                {
                  ++_trailerErrors;
                  if( trailer & _trailerFmt.truncMsk )
                    ++_truncErrors;
                  if( trailer & _trailerFmt.errorMsk )
                    ++_errErrors;
                  if( type == CHUNKTYPE_UNDEFINED6 )
                    ++_typeErrors;
                  // In case we exert backpressure (_delayIt==true) we only
                  // want to see a blockheader error, not this trailer error?
                  if( _blockErrorIndex < 0 && _delayIt == false )
                    {
                      // Copy data blocks around first found error,
                      // for later inspection
                      _blockErrorIndex = index;
                      //memcpy( _errorBlock,
                      //      (data-sizeof(_errorBlock)/2),
                      //      sizeof(_errorBlock) );
                      memcpy( _errorBlock, data, _blockSize );
                    }
                }

              // Count CRC errors separately
              if( crc_err )
                ++_crcErrors;

              // Count (complete) chunks
              if( type == CHUNKTYPE_BOTH || type == CHUNKTYPE_LAST )
                ++_chunkCount[elinknr];

              // Go to the start of the current chunk;
              // account for possible padding bytes
              if( (length & (_trailerFmt.nbytes-1)) != 0 )
                index -= (length + _trailerFmt.nbytes -
                          (length & (_trailerFmt.nbytes-1)));
              else
                index -= length;

              // Move to the preceeding trailer
              index -= _trailerFmt.nbytes;
            }

          // It's an error if we don't end up at the datablock header
          if( index != (int) (BLOCKHEADER_BYTES - _trailerFmt.nbytes) )
            {
              ++_trailerErrors;
              ++_lengthErrors;
              // In case we exert backpressure (_delayIt==true) we only
              // want to see a blockheader error, not this trailer error?
              if( _blockErrorIndex < 0 && _delayIt == false )
                {
                  // Copy data blocks around first found error,
                  // for later inspection
                  _blockErrorIndex = index;
                  //memcpy( _errorBlock,(data-sizeof(_errorBlock)/2),
                  //      sizeof(_errorBlock) );
                  memcpy( _errorBlock, data, _blockSize );
                }
            }
        }
      data += _blockSize; // Point to next data block
    }
  _blocksChecked += blocks;
  if( _delayIt )
    // ### DEBUG: delay to force buffer overflow
    std::this_thread::sleep_for( std::chrono::microseconds(_delayUs) );
}

// ----------------------------------------------------------------------------
