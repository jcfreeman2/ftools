#ifndef FLXCONFIG_H
#define FLXCONFIG_H

#include <string>

#include "flxcard/FlxCard.h"
#include "flxdefs.h"
#if REGMAP_VERSION < 0x500
#include "GbtConfig.h"
#else
#include "GbtConfig5.h"
#endif // REGMAP_VERSION

class FlxConfig
{
 public:
  // C'tor, d'tor
  FlxConfig( int card_nr );
  ~FlxConfig();

  // General
  int         classVersion        ( ); // Version of this class
  std::string errorString         ( );
  bool        hasError            ( );
  std::string debugString         ( );

  // Configuration
  std::string firmwareVersion     ( );
  void        setFanOutForDaq     ( );
  bool        fanOutIsLocked      ( );
  bool        isEnabledElink      ( int gbt, int egroup, int epath,
                                    int *elinkwidth );
  bool        isEnabledElink      ( int gbt, int eindex, int elinkwidth );
  bool        isEnabledElink      ( int elink, int *elinkwidth );
  void        readLinkConfig      ( );
  bool        writeLinkConfig     ( const std::string &filename,
                                    bool write_it = true,
                                    int  emu_chunksize = 32,
                                    bool emu_ewidth_dependent_size = false,
                                    bool emu_random_size = false,
                                    int  emu_idle_chars = 8,
                                    bool emu_with_streamid = false,
                                    bool lsb_first = false,
                                    bool do_regsettings = true,
                                    int *elinks_th  = 0,
                                    int *eclinks_th = 0,
                                    int *elinks_fh  = 0,
                                    int *eclinks_fh = 0,
                                    int *linkmode   = 0,
                                    int *settings_c = 0 );
  int    emulatorChunkCount( ) { return _emulatorChunkCount; }
  double emulatorChunkRate( )  { return _emulatorChunkRate; }
  int    dmaIndexInvalidCount(){ return _dmaIndexInvalidCount; }

 private:
  void  writeLinkConfig     ( );
  void  writeGbtEmulatorData( uint32_t req_chunksize = 32,
                              bool     width_prop_sz = false,
                              bool     random_sz     = false,
                              uint32_t idle_chars    = 8,
                              bool     use_streamid  = false,
                              bool     lsb_first     = false );
  void  writeFmEmulatorData ( uint32_t req_chunksize,
                              bool     random_sz,
                              uint32_t idle_chars,
                              bool     use_streamid  = false );

 private:
  FlxCard   *_flx;
  int        _cardNr;
  flxcard_bar2_regs_t *_bar2;

  // Space to store the card's current e-link configuration, + Emulator 'link'
  GbtConfig  _gbtConfig[FLX_LINKS + 1];
  uint64_t   _firmwareMode;
  int        _dmaIndexInvalidCount;

  // The emulator data configuration
  int    _emulatorChunkCount;
  double _emulatorChunkRate;

  // String containing a description of the last error that occurred
  std::string _errString;
};

#endif // FLXCONFIG_H
