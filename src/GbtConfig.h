#ifndef GBTCONFIG_H
#define GBTCONFIG_H

#include <cstdint>

#include "flxdefs.h"

// Map from Elink-PROC enabled bitmask to E-path (FIFO)
#if REGMAP_VERSION < 0x500
const int EPROC_TO_PATH[FLX_ELINK_PROCS] = { 3, 1, 5, 0, 2, 4, 6,
                                             0, 1, 2, 3, 4, 5, 6, 7 };
#else
const int EPROC_TO_PATH[FLX_ELINK_PROCS] = { 0, 0, 4, 0, 2, 4, 6,
                                             0, 1, 2, 3, 4, 5, 6, 7 };
#endif
// Map from Elink-PROC enabled bitmask to Elink number-of-bits width
const int EPROC_TO_WIDTH[FLX_ELINK_PROCS] = { 16, 8, 8, 4, 4, 4, 4,
                                               2, 2, 2, 2, 2, 2, 2, 2 };
// Map from Elink-PROC enabled bitmask to bit index
// (into a 16-bits emulator word)
const int EPROC_TO_BIT_I[FLX_ELINK_PROCS] = {  0, 0, 8, 0, 4,  8, 12,
                                               0, 2, 4, 6, 8, 10, 12, 14 };
enum {
  LINKMODE_GBT = 0,
  LINKMODE_GBTWIDE,
  LINKMODE_FULL,
  LINKMODE_LPGBT
};

typedef struct register_setting {
  std::string name;
  uint64_t value;
} regsetting_t;

// Structure/class describing a single GBT-link configuration for FELIX
class GbtConfig
{
 private:
  // E-link group E-processor enable configuration bitmask,
  // 7+1 (to Host) or 5+1 (from Host) e-groups per link
  uint32_t _enablesToHost[FLX_TOHOST_GROUPS];
  uint32_t _enablesFromHost[FLX_FROMHOST_GROUPS];

  // E-link group mode words, 7+1 (from GBT) or 5+1 (to GBT) e-groups per link
  uint64_t _modesToHost[FLX_TOHOST_GROUPS];
  uint64_t _modesFromHost[FLX_FROMHOST_GROUPS];

  // Maximum chunk length word (4x3 bits per GBT)
  uint32_t _maxChunkWord;

  // Link type: GBT normal, GBT wide or FULL
  uint32_t _linkMode;

  // Clock: TTC or local
  bool     _ttcClock;

  // Stream-ID indicator bits (1 per E-path, 7 groups * 8 bits)
  uint64_t _streamIdBits[FLX_TOHOST_GROUPS-1];

  // --------------------------------------------------------------------------

 public:
  GbtConfig() {
    // Default all 2-bit elinks, '8b10b' mode,
    // max chunks 512 bytes (one 'MAXCHUNK_UNIT'), standard-mode GBT
    // In case of full-mode: link enabled (bit 16 set in 'enables' word)
    int egroup;
    for( egroup=0; egroup<FLX_TOHOST_GROUPS-1; ++egroup )
      {
        _enablesToHost[egroup] = 0x7F80;
        //_modesToHost[egroup] = 0x000000000000000; // All 'direct'
        _modesToHost[egroup]   = 0x111111111111111; // All '8b10b'
      }

    // If FULLmode then link is enabled
    _enablesToHost[0] |= 0x10000;

    // EC+TTC2Host group
    //_enablesToHost[FLX_TOHOST_GROUPS-1] = 0x4400; // EC, TTC2Host
    _enablesToHost[FLX_TOHOST_GROUPS-1] = 0x4000; // EC, not TTC2Host
    _modesToHost[FLX_TOHOST_GROUPS-1]   = 0x222222222222222; // All 'HDLC'

    for( egroup=0; egroup<FLX_FROMHOST_GROUPS-1; ++egroup )
      {
        _enablesFromHost[egroup] = 0x7F80;
        //_modesFromHost[egroup] = 0x000000000000000; // All 'direct'
        _modesFromHost[egroup]   = 0x111111111111111; // All '8b10b'
      }
    // EC group
    _enablesFromHost[FLX_FROMHOST_GROUPS-1] = 0x4000; // EC
    _modesFromHost[FLX_FROMHOST_GROUPS-1]   = 0x222222222222222; // All 'HDLC'

    //maxChunkWord = 0x249; Or rather:
    //_maxChunkWord = (1<<(0*3)) | (1<<(1*3)) | (1<<(2*3)) | (1<<(3*3));
    // Max chunk size set to all 0
    _maxChunkWord = (0<<(0*3)) | (0<<(1*3)) | (0<<(2*3)) | (0<<(3*3));

    _linkMode = LINKMODE_GBT;

    _ttcClock = false;

    for( egroup=0; egroup<FLX_TOHOST_GROUPS-1; ++egroup )
      _streamIdBits[egroup] = 0;
  }
  ~GbtConfig() {}

  // --------------------------------------------------------------------------

  int numberOf2BitToHost()
  {
    if( _linkMode >= LINKMODE_FULL ) return 0;
    int cnt = 0;
    // Normal: 5, Wide: 7 groups
    uint32_t max_grp = (_linkMode == LINKMODE_GBTWIDE ? 7 : 5);
    for( uint32_t i=0; i<max_grp; ++i )
      {
        uint32_t mask = (_enablesToHost[i] & 0x7F80) >> 7;
        for( int j=0; j<8; ++j )
          if( mask & (1<<j) ) ++cnt;
      }
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOf4BitToHost()
  {
    if( _linkMode >= LINKMODE_FULL ) return 0;
    int cnt = 0;
    // Normal: 5, Wide: 7 groups
    uint32_t max_grp = (_linkMode == LINKMODE_GBTWIDE ? 7 : 5);
    for( uint32_t i=0; i<max_grp; ++i )
      {
        if( _enablesToHost[i] & 0x0008 ) ++cnt;
        if( _enablesToHost[i] & 0x0010 ) ++cnt;
        if( _enablesToHost[i] & 0x0020 ) ++cnt;
        if( _enablesToHost[i] & 0x0040 ) ++cnt;
      }
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOf8BitToHost()
  {
    if( _linkMode >= LINKMODE_FULL ) return 0;
    int cnt = 0;
    // Normal: 5, Wide: 7 groups
    uint32_t max_grp = (_linkMode == LINKMODE_GBTWIDE ? 7 : 5);
    for( uint32_t i=0; i<max_grp; ++i )
      {
        if( _enablesToHost[i] & 0x0002 ) ++cnt;
        if( _enablesToHost[i] & 0x0004 ) ++cnt;
      }
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOf16BitToHost()
  {
    if( _linkMode >= LINKMODE_FULL ) return 0;
    int cnt = 0;
    // Normal: 5, Wide: 7 groups
    uint32_t max_grp = (_linkMode == LINKMODE_GBTWIDE ? 7 : 5);
    for( uint32_t i=0; i<max_grp; ++i )
      if( _enablesToHost[i] & 0x0001 ) ++cnt;
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOfElinksToHost()
  {
    // Return the enabled ToHost (E-)link count
    if( _linkMode > LINKMODE_FULL ) return 0;
    int cnt = 0;
    if( _linkMode != LINKMODE_FULL )
      {
        // Normal: 5, Wide: 7 groups
        uint32_t max_grp = (_linkMode == LINKMODE_GBTWIDE ? 7 : 5);
        for( uint32_t i=0; i<max_grp; ++i )
          for( uint32_t j=0; j<FLX_ELINK_PROCS; ++j )
            if( _enablesToHost[i] & (1<<j) )
              ++cnt;
      }
    else
      {
        if( _enablesToHost[0] & 0x10000 )
          cnt = 1;
      }
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOfElinksFromHost()
  {
    // Return the enabled FromHost E-link count
    int cnt = 0;
    // Normal: 5, Wide: 3 groups
    uint32_t max_grp = (_linkMode == LINKMODE_GBTWIDE ? 3 : 5);
    for( uint32_t i=0; i<max_grp; ++i )
      for( uint32_t j=0; j<FLX_ELINK_PROCS; ++j )
        if( _enablesFromHost[i] & (1<<j) )
          ++cnt;
    return cnt;
  }

  // --------------------------------------------------------------------------

  int numberOfEcToHost()
  {
    // Return the enabled ToHost EC- (plus TTC2Host-)link count
    if( _linkMode > LINKMODE_FULL ) return 0;
    int cnt = 0;
    if( _linkMode != LINKMODE_FULL )
      {
        // EC + IC + AUX + TTC2Host
        for( uint32_t j=0; j<FLX_ELINK_PROCS; ++j )
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
    for( uint32_t j=0; j<FLX_ELINK_PROCS; ++j )
      if( _enablesFromHost[FLX_FROMHOST_GROUPS-1] & (1<<j) )
        ++cnt;
    return cnt;
  }

  // --------------------------------------------------------------------------

  uint32_t enablesToHost( uint32_t egroup )
  {
    if( egroup >= FLX_TOHOST_GROUPS ) return 0;
    return _enablesToHost[egroup];
  }

  void setEnablesToHost( uint32_t egroup, uint32_t enables )
  {
    if( egroup >= FLX_TOHOST_GROUPS ) return;
    _enablesToHost[egroup] = enables;
  }

  // --------------------------------------------------------------------------

  uint32_t enablesFromHost( uint32_t egroup )
  {
    if( egroup >= FLX_FROMHOST_GROUPS ) return 0;
    return _enablesFromHost[egroup];
  }

  void setEnablesFromHost( uint32_t egroup, uint32_t enables )
  {
    if( egroup >= FLX_FROMHOST_GROUPS ) return;
    _enablesFromHost[egroup] = enables;
  }

  // --------------------------------------------------------------------------

  uint64_t modesToHost( uint32_t egroup )
  {
    if( egroup >= FLX_TOHOST_GROUPS ) return 0;
    return _modesToHost[egroup];
  }

  void setModesToHost( uint32_t egroup, uint64_t modes )
  {
    if( egroup >= FLX_TOHOST_GROUPS ) return;
    _modesToHost[egroup] = modes;
  }

  // --------------------------------------------------------------------------

  uint64_t modesFromHost( uint32_t egroup )
  {
    if( egroup >= FLX_FROMHOST_GROUPS ) return 0;
    return _modesFromHost[egroup];
  }

  void setModesFromHost( uint32_t egroup, uint64_t modes )
  {
    if( egroup >= FLX_FROMHOST_GROUPS ) return;
    _modesFromHost[egroup] = modes;
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
    if( egroup >= FLX_TOHOST_GROUPS-1 ) return 0;
    return _streamIdBits[egroup];
  }

  void setStreamIdBits( uint32_t egroup, uint32_t bits )
  {
    if( egroup >= FLX_TOHOST_GROUPS-1 ) return;
    _streamIdBits[egroup] = bits;
  }

  // --------------------------------------------------------------------------

  int mode( int egroup, int epath, bool to_gbt = false )
  {
    // Returns the currently set mode for the given E-path

    uint32_t enables;
    uint64_t modes;
    if( to_gbt )
      {
        if( egroup >= FLX_FROMHOST_GROUPS ) return 0;
        enables = _enablesFromHost[egroup];
        modes   = _modesFromHost[egroup];
      }
    else
      {
        if( egroup >= FLX_TOHOST_GROUPS ) return 0;
        enables = _enablesToHost[egroup];
        modes   = _modesToHost[egroup];
      }

    uint32_t mode_per_epath =
      GbtConfig::epath4BitModeWord( enables, modes );

    return( (mode_per_epath >> (epath*4)) & 0xF );
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
        if( egroup >= FLX_FROMHOST_GROUPS ) return false;
        enable_bits = _enablesFromHost[egroup];
      }
    else
      {
        if( egroup >= FLX_TOHOST_GROUPS ) return false;
        enable_bits = _enablesToHost[egroup];
      }

    // Find first enabled E-proc that matches 'epath'
    bool enabled = false;
    if( nr_of_bits ) *nr_of_bits = 0;
    for( int i=0; i<FLX_ELINK_PROCS; ++i )
      if( enable_bits & (1<<i) )
        if( EPROC_TO_PATH[i] == epath )
          {
            enabled = true;
            if( nr_of_bits )
              {
                if( egroup == FLX_TOHOST_GROUPS-1 && epath == 3 )
                  *nr_of_bits = 0; // TTC2Host link
                else
                  *nr_of_bits = EPROC_TO_WIDTH[i];
              }
          }
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
      return false; // The number of bits cannot be located at this index

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
        if( nr_of_bits ==  2 && (enable_bits & (1<<7)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<3)) ) return true;
        if( nr_of_bits ==  8 && (enable_bits & (1<<1)) ) return true;
        if( nr_of_bits == 16 && (enable_bits & (1<<0)) ) return true;
        break;
      case 2:
        if( nr_of_bits ==  2 && (enable_bits & (1<<8)) ) return true;
        break;
      case 4:
        if( nr_of_bits ==  2 && (enable_bits & (1<<9)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<4)) ) return true;
        break;
      case 6:
        if( nr_of_bits ==  2 && (enable_bits & (1<<10)) ) return true;
        break;
      case 8:
        if( nr_of_bits ==  2 && (enable_bits & (1<<11)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<5)) )  return true;
        if( nr_of_bits ==  8 && (enable_bits & (1<<2)) )  return true;
        break;
      case 10:
        if( nr_of_bits ==  2 && (enable_bits & (1<<12)) ) return true;
        break;
      case 12:
        if( nr_of_bits ==  2 && (enable_bits & (1<<13)) ) return true;
        if( nr_of_bits ==  4 && (enable_bits & (1<<6)) )  return true;
        break;
      case 14:
        if( nr_of_bits ==  2 && (enable_bits & (1<<14)) ) return true;
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
#if REGMAP_VERSION < 0x500        
        if( elinkwidth == 2 )  return 0;
        if( elinkwidth == 4 )  return 0;
        if( elinkwidth == 8 )  return 1;
        if( elinkwidth == 16 ) return 3;
#else        
        return 0;
#endif
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
#if REGMAP_VERSION < 0x500        
        if( elinkwidth == 8 )  return 5;
#else
        if( elinkwidth == 8 )  return 4;
#endif
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

  static uint32_t epathBitsWord( uint32_t enables )
  {
    // Compile a 32-bit word in which each nibble indicates
    // the number of bits (minus 1) per E-path/FIFO, in order of E-paths
    // (zero means E-path/FIFO is not in use)
    uint32_t nbits_word = 0;
    for( int i=0; i<FLX_ELINK_PROCS; ++i )
      if( enables & (1<<i) )
        nbits_word |= (EPROC_TO_WIDTH[i]-1) << (EPROC_TO_PATH[i]*4);
    return nbits_word;
  }

  // --------------------------------------------------------------------------

  static uint32_t epathBitIndexWord( uint32_t enables )
  {
    // Compile a 32-bit word in which each nibble designates
    // the index in the 16-bit Egroup where an E-path/FIFO
    // gets its bits from (used in combination with epathBitsWord()
    // which indicates which E-paths are enabled)
    uint32_t nbits_index_word = 0;
    for( int i=0; i<FLX_ELINK_PROCS; ++i )
      if( enables & (1<<i) )
        nbits_index_word |= EPROC_TO_BIT_I[i] << (EPROC_TO_PATH[i]*4);
    return nbits_index_word;
  }

  // --------------------------------------------------------------------------

  static uint32_t epath4BitModeWord( uint32_t enables, uint64_t modes )
  {
    // Compile a 32-bit word with each nibble indicating
    // the mode identifier per E-path/FIFO, in order of E-paths
    // (as present in the FELIX E-group configuration register
    //  for 'FromHost' E-links)
    // Disabled paths are set to mode 1 = '8b10b' (1 Jun 2016)
    uint32_t mode_word = 0x11111111;
    for( int i=0; i<FLX_ELINK_PROCS; ++i )
      if( enables & (1<<i) )
        {
          mode_word &= ~(0xFULL << (EPROC_TO_PATH[i]*4));
          mode_word |= ((modes>>(i*4)) & 0xFULL) << (EPROC_TO_PATH[i]*4);
        }
    return mode_word;
  }

  static uint32_t epath2BitModeWord( uint32_t enables, uint64_t modes )
  {
    // Compile a 16-bit significant word with each 2 bits indicating
    // the mode identifier per E-path/FIFO, in order of E-paths
    // (as present in the FELIX E-group configuration register
    //  for 'ToHost' E-links)
    // Disabled paths are set to mode 1 = '8b10b' (1 Jun 2016)
    uint32_t mode_word = 0x5555;
    for( int i=0; i<FLX_ELINK_PROCS; ++i )
      if( enables & (1<<i) )
        {
          mode_word &= ~(0x3ULL << (EPROC_TO_PATH[i]*2));
          mode_word |= ((modes>>(i*4)) & 0x3ULL) << (EPROC_TO_PATH[i]*2);
        }
    return mode_word;
  }

  //static uint64_t epath4BitModeWordToModes( uint32_t enables,
  static uint64_t epath4BitModeWordToModes( uint64_t mode_word )
  {
    // Derive local 'modes' word (format: 4 bits per EPROC in EPROC order)
    // from the EPROC 'enables' bitmask and the 8x4-bit mode word, both as read
    // from the FELIX 'FromHost' E-group configuration register;
    // make that independent of 'enables' (2 Mar 2016)
    uint64_t modes = 0;
    for( int i=0; i<FLX_ELINK_PROCS; ++i )
      //if( enables & (1<<i) )
      modes |= (uint64_t) (mode_word>>(EPROC_TO_PATH[i]*4) & 0xF) << (i*4);
    return modes;
  }

  //static uint64_t epath2BitModeWordToModes( uint32_t enables,
  static uint64_t epath2BitModeWordToModes( uint64_t mode_word )
  {
    // Derive local 'modes' word (format: 4 bits per EPROC in EPROC order)
    // from the EPROC 'enables' bitmask and the 8x2-bit mode word, both as read
    // from the FELIX 'ToHost' E-group configuration register;
    // make that independent of 'enables' (2 Mar 2016)
    uint64_t modes = 0;
    for( int i=0; i<FLX_ELINK_PROCS; ++i )
      //if( enables & (1<<i) )
      modes |= (uint64_t) (mode_word>>(EPROC_TO_PATH[i]*2) & 0x3) << (i*4);
    return modes;
  }

  // --------------------------------------------------------------------------

  static bool enablesIsValid( uint32_t enables )
  {
    // Check if this Eproc enables mask is a valid one

    // Any illegal bits set ? (NB: added bit 0x10000 for FULLMODE)
    if( enables & 0xFFFE8000 ) return false;

    // Nothing set ? ==> that's okay...
    //if( enables == 0 ) return false;

    // 16-bit
    if( enables & 0x0001 )
      {
        // All other settings not allowed
        if( enables & 0x7FFE ) return false;
      }
    // 8-bit
    if( enables & 0x0002 )
      {
        // Some 2-bit and 4-bit not allowed
        if( enables & (0x0780 | 0x0018) ) return false;
      }
    // 8-bit
    if( enables & 0x0004 )
      {
        // Some 2-bit and 4-bit not allowed
        if( enables & (0x7800 | 0x0060) ) return false;
      }
    // 4-bit
    if( enables & 0x0008 )
      {
        // Some 2-bit not allowed
        if( enables & 0x0180 ) return false;
      }
    // 4-bit
    if( enables & 0x0010 )
      {
        // Some 2-bit not allowed
        if( enables & 0x0600 ) return false;
      }
    // 4-bit
    if( enables & 0x0020 )
      {
        // Some 2-bit not allowed
        if( enables & 0x1800 ) return false;
      }
    // 4-bit
    if( enables & 0x0040 )
      {
        // Some 2-bit not allowed
        if( enables & 0x6000 ) return false;
      }
    return true;
  }

  // --------------------------------------------------------------------------
};

#endif // GBTCONFIG_H
