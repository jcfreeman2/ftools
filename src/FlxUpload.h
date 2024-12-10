#ifndef FLXUPLOAD_H
#define FLXUPLOAD_H

#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <cstdint>

#include "cmem_rcc/cmem_rcc.h"
#include "flxcard/FlxCard.h"
#include "flxdefs.h"
#if REGMAP_VERSION < 0x500
#include "GbtConfig.h"
#else
#include "GbtConfig5.h"
#endif // REGMAP_VERSION

typedef int64_t  i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

// Upload buffer size (NB: for circular/continuous-DMA to work,
//                         must be >16MB; due to modification by Frans)
#define DFLT_SEND_BUF_SIZE  0x002000000 // 32 MByte

class FlxUpload
{
 public:
  // C'tor, d'tor
  FlxUpload( int card_nr, int64_t buffer_size, int dma_index = -1,
             int hdlc_seq_nr = 0 );
  ~FlxUpload();

  // Structure to contain a GBT-SCA message (also used for IC)
  typedef struct sca_msg
  {
    int len;
    // Addr+Ctrl byte, TRID/Chan/Len/CMD bytes, 4 data bytes, 2-byte CRC:
    u8  data[2+(4+4)+2];
  } sca_msg_t;

  // General
  void        stop                ( );
  int         classVersion        ( ); // Version of this class
  std::string errorString         ( );
  bool        hasError            ( );
  std::string debugString         ( );

  // Data upload
  void        setFanOutForDaq     ( );
  bool        fanOutIsLocked      ( );
  bool        fanOutLocked        ( ) { return fanOutIsLocked(); }
  i64         bufferSize          ( ) { return _bufferSize; }
  char       *buffer              ( ) { return _dataBuffer; }
  int         prepareData         ( int link_count, int *elink,
                                    int size, int pattern_id, int repeat );
  int         prepareData         ( int elink, int size,
                                    int pattern_id = 0,  int repeat = 1 );
  int         prepareData         ( int elink,
                                    const std::string &filename, int repeat,
                                    bool binary = true,
                                    bool display = false );
  int         prepareData         ( int elink, int size, u8 *data );
  int         prepareData         ( int elink, std::vector<sca_msg_t> &msgs );
  int         prepareDataRaw      ( int size, int pattern_id = 0 );
  int         prepareDataRaw      ( const std::string &filename,
                                    bool binary = true );
  bool        upload              ( int speed_factor = 1 );
  bool        uploadFinished      ( );
  u64         timestamp           ( ) { u64 t = _timestamp;
                                        _timestamp = 0; return t; }
  int         dmaIndex            ( ) { return _dmaIndex; }
  u64         dmaSize             ( ) { return _uploadDmaSize; }
  void        setDmaSize          ( u64 sz ) { _uploadDmaSize = sz; }
  void        setFillerByte       ( int f ) { _fillerByte = (char)(f & 0xFF); }
  void        setDmaCircular      ( bool b ) { _dmaCircular = b; }
  void        dmaCircularEnable   ( ) { dmaStart( 0, 0 ); }
  void        setDmaTlp           ( int tlp ) { _dmaTlp = tlp; }

  // FLX data blocks playback/upload from file
  bool        startPlayback       ( const std::string &filename,
                                    int blocksize = -1 );
  bool        playback            ( int elink_filter, int speed_factor = 1 );
  i64         playbackChunkCounter( ) { return _pbChunkCntr; }
  i64         playbackBlockCounter( ) { return _pbBlockCntr; }
  void        setPlaybackRandom   ( bool b ) { _pbRandomData = b; }

  // IC
  bool        writeIcTxtFile      ( int gbt, int i2c_addr,
                                    const std::string &filename,
                                    int *nbytes_file,
                                    bool ec = false, bool lpgbt_v1 = false );
  // IC via (virtual) E-link
  bool        writeIcChannel      ( int gbt, int i2c, int addr,
                                    int nbytes, u8 *data,
                                    bool ec = false, bool lpgbt_v1 = false );
  bool        readIcChannel       ( int gbt, int i2c, int addr,
                                    int nbytes,
                                    bool ec = false, bool lpgbt_v1 = false );
  bool        addIcFrame          ( bool read, int i2c_addr, int reg_addr,
                                    int  nbytes, u8 *data,
                                    bool lpgbt_v1 = false );
  int         uploadIcFrames( int elink ) { return uploadScaFrames(elink); }
  void        resetIcFrames       ( ) { resetScaFrames(); }
  void        dumpIcFrames        ( ) { dumpScaFrames(); }
  bool        addIcDelay          ( int len ) { return addScaDelay(len); }
  bool        addIcDelayUs        ( int us )  { return addScaDelayUs(us); }

  // EC/GBT-SCA
  int         uploadScaFrame      ( int elink,
                                    int *trid, int chan, int len, int cmd,
                                    u8  *data );
  void        resetScaFrames      ( );
  void        dumpScaFrames       ( );
  bool        addScaFrame         ( int *trid, int chan, int len,
                                    int cmd, u8 *data );
  bool        addScaDelay         ( int len );
  bool        addScaDelayUs       ( int microseconds );
  int         uploadScaFrames     ( int elink );
  int         scaConnect          ( int elink );
  int         scaReset            ( int elink );
  int         scaTest             ( int elink );

  // Statistics and info
  void        dumpData            ( int size, int offset = 0 );
  std::string firmwareVersion     ( );
  int         firmwareMode        ( );
  int         numberOfChans       ( );

  // Mapping for EC, IC, AUX (for RM5)
  int   mapElink( int elinknr );

 private:
  char *cmemAllocate  ( u64 buffer_size, int id = -1 );
  void  dmaStart      ( u64 upload_index, u64 size );
  void  dmaStop       ( );
  bool  dmaActive     ( );

  // For playback/upload
  int   formatBlockForUpload( char *block, int elink_filter );
  int   formatChunkForUpload( int hdr, int dest_index,
                              char *chunkdata, int chunksize );

  // For IC via (virtual) E-link (or optionally through EC for 2nd lpGBT)
  bool  uploadIcFrame( bool read,
                       int  gbt, int i2c_addr, int reg_addr,
                       int  nbytes, u8 *data,
                       bool ec, bool lpgbt_v1 );

  // For EC/GBT-SCA
  int   scaControl( int elink, int control );

  // Timing
  void  pause( int delay_us );
  int   minimumUsleep( );

 private:
  FlxCard   *_flx;
  int        _cardNr;
  int        _blockSize, _blockSizeCode;
  flxcard_bar2_regs_t *_bar2;
  FlxTrailerFormat _trailerFmt;
  bool       _has5BitFromHostLength;

  // String containing a description of the last error that occurred
  std::string _errString;

  // Buffer administration
  i64        _bufferSize;

  // Pointer to allocated data send buffer
  char      *_dataBuffer;

  // CMEM stuff
  int        _cmemHandle;
  cmem_rcc_t _cmemDescriptor;
  u64        _cmemStartAddr, _cmemEndAddr;

  // DMA
  int  _dmaIndex;
  bool _dmaCircular;
  int  _dmaTlp;
  u64  _dmaSoftwPtr;
  volatile flxcard_bar0_regs_t *_bar0;
  volatile dma_descriptor_t    *_dmaDesc;
  volatile dma_status_t        *_dmaStat;

  // Upload administration
  i64  _bytesToUpload;
  i64  _uploadDmaSize;
  u64  _uploadIndex;
  i64  _writeIndex;
  u64  _timestamp;
  char _fillerByte;
  typedef struct delay_struct
  {
    u32 index;
    int delay_us;
  } delay_t;
  std::list<delay_t> _delays;
  int _minimumUsleep; // Overhead in time of usleep(1)

  // GBT-SCA frames administration
  u64 _hdlcSeqNr;
  // Storage for GBT-SCA frames (or IC frames, with up to 4 data bytes)
  std::vector<sca_msg_t> _scaMsgList;

  // Playback administration
  std::ifstream _pbFile;
  i64    _pbChunkCntr;
  i64    _pbBlockCntr;
  bool   _pbRandomData;
  // Playback intermediate storage
  std::vector<char> _eBuf[FLX_MAX_ELINK_NR+1];
};

#endif // FLXUPLOAD_H
