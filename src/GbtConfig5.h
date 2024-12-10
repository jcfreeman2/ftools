#ifndef GBTCONFIG5_H
#define GBTCONFIG5_H

#include "flxdefs.h"
#include <cstdint>

enum {
  LINKMODE_GBT = 0,
  LINKMODE_GBTWIDE,
  LINKMODE_FULL,
  LINKMODE_LPGBT,
  LINKMODE_INVALID
};

typedef struct register_setting {
  std::string name;
  uint64_t value;
} regsetting_t;

// Structure/class describing a single (lp)GBT-link configuration for FELIX
class GbtConfig
{
 private:
  // Link type: GBT, GBT-wide, FULL or lpGBT
  uint32_t _linkMode;

  // E-link configuration bitmask, 7+1 (to Host) e-groups per link
  uint32_t _enablesToHost[FLX_TOHOST_GROUPS];
  // E-link group width (actually a width code: 0=2bits, 1=4, 2=8, 3=16, 4=32)
  // 7+1 (to Host) e-groups per link
  uint32_t _widthToHost[FLX_TOHOST_GROUPS];
  // E-link group mode words, 7+1 (to Host) e-groups per link
  uint64_t _modesToHost[FLX_TOHOST_GROUPS];
  // DMA index for ToHost E-links (1 nibble per E-path, 7+1 groups * 8)
  uint64_t _dmaIndices[FLX_TOHOST_GROUPS];
  // Stream-ID indicator bits (1 per ToHost E-path, 7 groups * 8 bits)
  uint64_t _streamIdBits[FLX_TOHOST_GROUPS-1];

  // E-link enable configuration bitmask, 5+1 (from Host) e-groups per link
  uint32_t _enablesFromHost[FLX_FROMHOST_GROUPS];
  // E-link group width (actually a width code: 0=2bits, 1=4, 2=8, 3=16, 4=32)
  // 5+1 (from Host) e-groups per link
  uint32_t _widthFromHost[FLX_FROMHOST_GROUPS];
  // E-link group mode words, 5+1 (from Host) e-groups per link
  uint64_t _modesFromHost[FLX_FROMHOST_GROUPS];

  // Maximum ToHost chunk length word (4x3 bits per GBT)
  uint32_t _maxChunkWord;

  // Clock: TTC or local
  bool     _ttcClock;

  // --------------------------------------------------------------------------

 public:
  GbtConfig()
  {
    _linkMode = LINKMODE_GBT;

    // Default all 2-bit elinks, '8b10b' mode, standard-mode GBT
    // In case of full-mode: link enabled (bit 16 set in 'enables' word)
    int egroup;
    for( egroup=0; egroup<FLX_TOHOST_GROUPS-1; ++egroup )
      {
        _enablesToHost[egroup] = 0xFF;
        _widthToHost[egroup]   = 0; // 2-bit
        _modesToHost[egroup]   = 0x11111111; // All '8b10b'
        _dmaIndices[egroup]    = 0;
      }

    // If FULL mode then link is enabled
    _enablesToHost[0] |= 0x10000;

    // EC+IC+AUX+TTC2Host group
    //_enablesToHost[FLX_TOHOST_GROUPS-1] = 0x4400; // EC, TTC2Host
    //_enablesToHost[FLX_TOHOST_GROUPS-1] = 0x6400; // EC, IC, TTC2Host
    //_enablesToHost[FLX_TOHOST_GROUPS-1] = 0x7400; // EC, IC, AUX, TTC2Host
    _enablesToHost[FLX_TOHOST_GROUPS-1] = 0x4000; // EC, not TTC2Host
    _widthToHost[FLX_TOHOST_GROUPS-1]   = 0;
    _modesToHost[FLX_TOHOST_GROUPS-1]   = ((uint64_t)2 << (14*4)); // EC 'HDLC'
    _dmaIndices[FLX_TOHOST_GROUPS-1]    = 0;

    for( egroup=0; egroup<FLX_FROMHOST_GROUPS-1; ++egroup )
      {
        _enablesFromHost[egroup] = 0xFF;
        _widthFromHost[egroup]   = 0; // 2-bit
        _modesFromHost[egroup]   = 0x11111111; // All '8b10b'
      }
    // EC+IC+AUX group
    _enablesFromHost[FLX_FROMHOST_GROUPS-1] = 0x4000; // EC
    _widthFromHost[FLX_FROMHOST_GROUPS-1]   = 0;
    _modesFromHost[FLX_FROMHOST_GROUPS-1]   = ((uint64_t)2 << (14*4)); // EC 'HDLC'

    //maxChunkWord = 0x249; Or rather (512 bytes max chunksize):
    //_maxChunkWord = (1<<(0*3)) | (1<<(1*3)) | (1<<(2*3)) | (1<<(3*3));

    // Max chunk size set to all 0
    _maxChunkWord = (0<<(0*3)) | (0<<(1*3)) | (0<<(2*3)) | (0<<(3*3));

    for( egroup=0; egroup<FLX_TOHOST_GROUPS-1; ++egroup )
      _streamIdBits[egroup] = 0;
  }
  ~GbtConfig() {}

  // --------------------------------------------------------------------------

  int numberOfElinksToHost()
  {
    // Return the enabled ToHost (E-)link count
    if( _linkMode >= LINKMODE_INVALID )
      return 0;

    int cnt = 0;
    if( _linkMode != LINKMODE_FULL )
      {
        // GBT: 5, GBT-wide: 7, lpGBT: 7 groups
        uint32_t grp_cnt, path_cnt;
        if( _linkMode == LINKMODE_LPGBT )
          {
            grp_cnt = 7;
            path_cnt = 4;
          }
        else if( _linkMode == LINKMODE_GBTWIDE )
          {
            grp_cnt = 7;
            path_cnt = 8;
          }
        else
          {
            grp_cnt = 5;
            path_cnt = 8;
          }

        for( uint32_t i=0; i<grp_cnt; ++i )
          for( uint32_t j=0; j<path_cnt; ++j )
            if( _enablesToHost[i] & (1<<j) )
              ++cnt;
      }
    else
      {
        // FULLMODE link
        if( _enablesToHost[0] & 0x10000 )
          cnt = 1;
      }
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOfElinksFromHost()
  {
    // Return the enabled FromHost E-link count
    // GBT: 5, GBT-wide: 3, lpGBT: 4 groups
    uint32_t grp_cnt, path_cnt;
    if( _linkMode == LINKMODE_LPGBT )
      {
        grp_cnt = 4;
        path_cnt = 4;
      }
    else if( _linkMode == LINKMODE_GBTWIDE )
      {
        grp_cnt = 3;
        path_cnt = 8;
      }
    else
      {
        grp_cnt = 5;
        path_cnt = 8;
      }

    int cnt = 0;
    for( uint32_t i=0; i<grp_cnt; ++i )
      for( uint32_t j=0; j<path_cnt; ++j )
        if( _enablesFromHost[i] & (1<<j) )
          ++cnt;
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOfEcToHost()
  {
    // Return the enabled ToHost EC- (plus TTC2Host-)link count
    if( _linkMode >= LINKMODE_INVALID )
      return 0;

    int cnt = 0;
    if( _linkMode != LINKMODE_FULL )
      {
        // EC + IC + AUX + TTC2Host
        for( uint32_t j=0; j<16; ++j )
          if( _enablesToHost[FLX_TOHOST_GROUPS-1] & (1<<j) )
            ++cnt;
      }
    else
      {
        // TTC2Host only
        if( _enablesToHost[FLX_TOHOST_GROUPS-1] & 0x0400 )
          ++cnt;
      }
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOfEcFromHost()
  {
    // Return the enabled FromHost EC-link count
    int cnt = 0;
    // EC + IC + AUX
    for( uint32_t j=0; j<16; ++j )
      if( _enablesFromHost[FLX_FROMHOST_GROUPS-1] & (1<<j) )
        ++cnt;
    return cnt;
  }

  // --------------------------------------------------------------------------

  uint32_t enablesToHost( uint32_t egroup )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return 0;
    return _enablesToHost[egroup];
  }

  void setEnablesToHost( uint32_t egroup, uint32_t enables )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return;
    _enablesToHost[egroup] = enables;
  }

  // --------------------------------------------------------------------------

  uint32_t enablesFromHost( uint32_t egroup )
  {
    if( egroup >= FLX_FROMHOST_GROUPS )
      return 0;
    return _enablesFromHost[egroup];
  }

  void setEnablesFromHost( uint32_t egroup, uint32_t enables )
  {
    if( egroup >= FLX_FROMHOST_GROUPS )
      return;
    _enablesFromHost[egroup] = enables;
  }

  // --------------------------------------------------------------------------

  uint32_t widthToHost( uint32_t egroup )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return 0;
    return _widthToHost[egroup];
  }

  void setWidthToHost( uint32_t egroup, uint32_t width )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return;
    _widthToHost[egroup] = width;
  }

  // --------------------------------------------------------------------------

  uint32_t widthFromHost( uint32_t egroup )
  {
    if( egroup >= FLX_FROMHOST_GROUPS )
      return 0;
    return _widthFromHost[egroup];
  }

  void setWidthFromHost( uint32_t egroup, uint32_t width )
  {
    if( egroup >= FLX_FROMHOST_GROUPS )
      return;
    _widthFromHost[egroup] = width;
  }

  // --------------------------------------------------------------------------

  uint64_t modesToHost( uint32_t egroup )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return 0;
    return _modesToHost[egroup];
  }

  void setModesToHost( uint32_t egroup, uint64_t modes )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return;
    _modesToHost[egroup] = modes;
  }

  // --------------------------------------------------------------------------

  uint64_t modesFromHost( uint32_t egroup )
  {
    if( egroup >= FLX_FROMHOST_GROUPS )
      return 0;
    return _modesFromHost[egroup];
  }

  void setModesFromHost( uint32_t egroup, uint64_t modes )
  {
    if( egroup >= FLX_FROMHOST_GROUPS )
      return;
    _modesFromHost[egroup] = modes;
  }

  // --------------------------------------------------------------------------

  uint32_t dmaIndices( uint32_t egroup )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return 0;
    return _dmaIndices[egroup];
  }

  void setDmaIndices( uint32_t egroup, uint32_t indices )
  {
    if( egroup >= FLX_TOHOST_GROUPS )
      return;
    _dmaIndices[egroup] = indices;
  }

  // --------------------------------------------------------------------------

  uint32_t maxChunkWord()
  {
    return _maxChunkWord;
  }

  void setMaxChunkWord( uint32_t chunkword )
  {
    _maxChunkWord = chunkword;
  }

  void setMaxChunkSize( int index, int size )
  {
    _maxChunkWord &= ~(7 << (index*3));
    _maxChunkWord |= (size/FLX_MAXCHUNK_UNIT) << (index*3);
  }

  // --------------------------------------------------------------------------

  uint32_t linkMode()
  {
    return _linkMode;
  }

  void setLinkMode( uint32_t mode )
  {
    _linkMode = mode;
    if( mode == LINKMODE_LPGBT )
      {
        for( int i=0; i<FLX_TOHOST_GROUPS-1; ++i )
          if( _widthToHost[i] < 2 ) // Invalid
            _widthToHost[i] = 2; // lpGBT: 8-bit ToHost

        for( int i=0; i<FLX_TOHOST_GROUPS-1; ++i )
          _enablesToHost[i] &= ~0xF0; // Only 4 paths per ToHost group

        for( int i=0; i<FLX_FROMHOST_GROUPS-1; ++i )
          _enablesFromHost[i] &= ~0xF0; // Only 4 paths per FromHost group
      }
  }

  // --------------------------------------------------------------------------

  bool ttcClock()
  {
    return _ttcClock;
  }

  void setTtcClock( bool b )
  {
    _ttcClock = b;
  }

  // --------------------------------------------------------------------------

  uint32_t streamIdBits( uint32_t egroup )
  {
    if( egroup >= FLX_TOHOST_GROUPS-1 )
      return 0;
    return _streamIdBits[egroup];
  }

  void setStreamIdBits( uint32_t egroup, uint32_t bits )
  {
    if( egroup >= FLX_TOHOST_GROUPS-1 )
      return;
    _streamIdBits[egroup] = bits;
  }

  // --------------------------------------------------------------------------

  int mode( int egroup, int epath, bool to_gbt = false )
  {
    // Returns the currently set mode for the given E-path
    uint64_t modes;
    if( to_gbt )
      {
        if( egroup >= FLX_FROMHOST_GROUPS )
          return 0;
        modes = _modesFromHost[egroup];
      }
    else
      {
        if( egroup >= FLX_TOHOST_GROUPS )
          return 0;
        modes = _modesToHost[egroup];
      }

    return( (modes >> (epath*4)) & 0xF );
  }

  // --------------------------------------------------------------------------

  bool isEnabled( int egroup, int epath, int *nr_of_bits, bool to_gbt = false )
  {
    // Determine whether a certain E-link number (defined by its 'egroup' and
    // 'path') in this GbtConfig is enabled, and if so, how many bits wide it is
    // (returned in 'nr_of_bits')
    uint32_t enable_bits;
    if( to_gbt )
      {
        if( egroup >= FLX_FROMHOST_GROUPS )
          return false;
        enable_bits = _enablesFromHost[egroup];
      }
    else
      {
        if( egroup >= FLX_TOHOST_GROUPS )
          return false;
        enable_bits = _enablesToHost[egroup];
      }

    bool enabled = ((enable_bits & (1 << epath)) != 0);
    *nr_of_bits = GbtConfig::widthToBits( _widthFromHost[egroup] );

    return enabled;
  }

  // --------------------------------------------------------------------------

  bool isEnabled( int bit_index, int nr_of_bits, bool to_gbt = false )
  {
    // Determine whether a certain E-link number (defined by its GBT-frame
    // bit index (0..111 or 0..79) 'bit_index' and E-link width 'nr_of_bits')
    // in this GbtConfig is enabled.

    // Sanity checks on bit_index and nr_of_bits and their combination
    if( (!to_gbt && bit_index >= 7*16) ||
        (to_gbt  && bit_index >= 5*16) || bit_index < 0 )
      return false;
    if( !(nr_of_bits == 2 || nr_of_bits == 4 ||
          nr_of_bits == 8 || nr_of_bits == 16) )
      return false;
    if( (bit_index & (nr_of_bits-1)) != 0 )
      return false; // That number of bits cannot be located at this index

    int egroup = bit_index / 16;
    bit_index -= (egroup * 16);
    uint32_t enable_bits;
    if( to_gbt )
      enable_bits = _enablesFromHost[egroup];
    else
      enable_bits = _enablesToHost[egroup];

    switch( bit_index )
      {
      case 0:
        if( nr_of_bits ==  2 && (enable_bits & (1<<0)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<0)) ) return true;
        if( nr_of_bits ==  8 && (enable_bits & (1<<0)) ) return true;
        if( nr_of_bits == 16 && (enable_bits & (1<<0)) ) return true;
        break;
      case 2:
        if( nr_of_bits ==  2 && (enable_bits & (1<<1)) ) return true;
        break;
      case 4:
        if( nr_of_bits ==  2 && (enable_bits & (1<<2)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<2)) ) return true;
        break;
      case 6:
        if( nr_of_bits ==  2 && (enable_bits & (1<<3)) ) return true;
        break;
      case 8:
        if( nr_of_bits ==  2 && (enable_bits & (1<<4)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<4)) ) return true;
        if( nr_of_bits ==  8 && (enable_bits & (1<<4)) ) return true;
        break;
      case 10:
        if( nr_of_bits ==  2 && (enable_bits & (1<<5)) ) return true;
        break;
      case 12:
        if( nr_of_bits ==  2 && (enable_bits & (1<<6)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<6)) ) return true;
        break;
      case 14:
        if( nr_of_bits ==  2 && (enable_bits & (1<<7)) ) return true;
        break;
      }
    return false;
  }

  // --------------------------------------------------------------------------

  static int toEpath( int eindex, int elinkwidth )
  {
    // Translate an e-link identified by its position within the GBT frame
    // (parameter 'eindex') and its width (parameter 'elinkwidth') to
    // to the FLX-card e-path it uses
    eindex %= 16;
    switch( eindex )
      {
      case 0:
        return 0;
        break;
      case 2:
        if( elinkwidth == 2 )  return 1;
        break;
      case 4:
        if( elinkwidth == 2 )  return 2;
        if( elinkwidth == 4 )  return 2;
        break;
      case 6:
        if( elinkwidth == 2 )  return 3;
        break;
      case 8:
        if( elinkwidth == 2 )  return 4;
        if( elinkwidth == 4 )  return 4;
        if( elinkwidth == 8 )  return 4;
        break;
      case 10:
        if( elinkwidth == 2 )  return 5;
        break;
      case 12:
        if( elinkwidth == 2 )  return 6;
        if( elinkwidth == 4 )  return 6;
        break;
      case 14:
        if( elinkwidth == 2 )  return 7;
        break;
      }
    return -1;
  }

  // --------------------------------------------------------------------------

  static uint64_t epathBitsWord( uint32_t enables, uint32_t width )
  {
    // Compile a 64-bit word in which each byte indicates the number
    // of bits per E-path/FIFO, in order of (upto 8) E-paths,
    // zero meaning E-path/FIFO is not in use
    uint64_t nbits_word = 0;
    for( int i=0; i<8; ++i )
      if( enables & (1<<i) )
        nbits_word |= ((uint64_t) width) << (i*8);
    return nbits_word;
  }

  // --------------------------------------------------------------------------

  static bool enablesIsValid( uint32_t enables )
  {
    // Check if this (Eproc) enables mask is a valid one

    // Any illegal bits set ? (NB: added bit 0x10000 for FULLMODE)
    if( enables & 0xFFFEFF00 ) return false;

    // Nothing set ? ==> that's okay...
    //if( enables == 0 ) return false;

    return true;
  }

  // --------------------------------------------------------------------------

  static int widthToBits( uint32_t width )
  {
    if( width == 0 )      return 2;
    else if( width == 1 ) return 4;
    else if( width == 2 ) return 8;
    else if( width == 3 ) return 16;
    else if( width == 4 ) return 32;
    return 0;
  }

  // --------------------------------------------------------------------------
};

#endif // GBTCONFIG5_H
