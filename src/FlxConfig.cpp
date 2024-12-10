#include "FlxConfig.h"
#include "flxcard/FlxException.h"
#include "EmuDataGenerator.h"
#include "crc.h"

#include <iostream>
#include <iomanip>
#include <sstream> // for ostringstream
#include <fstream> // for ifstream
using namespace std;

#include "nlohmann/json.hpp"
using namespace nlohmann;
#include "yaml-cpp/yaml.h"

// ----------------------------------------------------------------------------

FlxConfig::FlxConfig( int card_nr )
  : _flx( 0 ),
    _cardNr( card_nr ),
    _bar2( 0 ),
    _firmwareMode( 0xFF )
{
  // Open FELIX FLX-card
  _flx = new FlxCard;
  try {
    uint32_t lockbits = LOCK_NONE;
    _flx->card_open( _cardNr, lockbits );
  }
  catch( FlxException &ex ) {
    if( !_errString.empty() ) _errString += "; ";
    _errString += "FlxCard open: ";
    _errString += ex.what();
    delete _flx;
    _flx = 0;
    return;
  }

  // Get a pointer to the BAR2 registers
  _bar2 = (flxcard_bar2_regs_t *) _flx->bar2Address();

  // Read the card's (To-GBT/From-Host) e-link configuration
  this->readLinkConfig();
}

// ----------------------------------------------------------------------------

FlxConfig::~FlxConfig()
{
  // Close the FLX-card
  try {
    if( _flx )
      _flx->card_close();
  }
  catch( FlxException &ex ) {
    // Do something..?
  }
  delete _flx;
}

// ----------------------------------------------------------------------------

std::string FlxConfig::errorString()
{
  if( _errString.empty() )
    return std::string( "" );

  std::ostringstream oss;
  oss << "FlxConfig#" << _cardNr << ": " << _errString;

  // Clear the error string
  _errString.clear();

  return oss.str();
}

// ----------------------------------------------------------------------------

bool FlxConfig::hasError()
{
  return !_errString.empty();
}

// ----------------------------------------------------------------------------

std::string FlxConfig::firmwareVersion()
{
  if( _flx == 0 ) return std::string();

  std::ostringstream oss;
  uint64_t ts  = _flx->cfg_get_reg( REG_BOARD_ID_TIMESTAMP );
  uint64_t fw  = _flx->cfg_get_reg( REG_FIRMWARE_MODE );
  uint64_t chn = _flx->cfg_get_reg( REG_NUM_OF_CHANNELS );
  uint64_t typ = _flx->cfg_get_reg( REG_CARD_TYPE );

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

  uint64_t git = _flx->cfg_get_reg( REG_GIT_TAG );
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

void FlxConfig::setFanOutForDaq()
{
  if( _flx == 0 ) return;

  // (Set "downlink fanout select")
  bool locked = this->fanOutIsLocked();
  if( !locked )
    {
      _flx->cfg_set_option( BF_GBT_TOFRONTEND_FANOUT_SEL, 0 );
      _flx->cfg_set_option( BF_GBT_TOHOST_FANOUT_SEL, 0 );
    }
}

// ----------------------------------------------------------------------------

bool FlxConfig::fanOutIsLocked()
{
  if( _flx == 0 ) return false;

  // Check for the FANOUT_LOCK bit(s)
  bool locked = false;
  if( _flx->cfg_get_option( BF_GBT_TOHOST_FANOUT_LOCK ) == 1 )
    locked = true;
  if( _flx->cfg_get_option( BF_GBT_TOFRONTEND_FANOUT_LOCK ) == 1 )
    locked = true;
  return locked;
}

// ----------------------------------------------------------------------------

bool FlxConfig::isEnabledElink( int gbt, int egroup, int epath,
                                int *elinkwidth )
{
  bool to_gbt = true;
  if( egroup == 7 && epath == 7 ) egroup = 5; // for FromHost EC-link
  if( egroup == 7 && epath == 6 ) egroup = 5; // for FromHost IC-link
  if( egroup == 7 && epath == 5 ) egroup = 5; // for FromHost AUX-link
  return _gbtConfig[gbt].isEnabled( egroup, epath, elinkwidth, to_gbt );
}

// ----------------------------------------------------------------------------

bool FlxConfig::isEnabledElink( int gbt, int eindex, int elinkwidth )
{
  bool to_gbt = true;
  return _gbtConfig[gbt].isEnabled( eindex, elinkwidth, to_gbt );
}

// ----------------------------------------------------------------------------

bool FlxConfig::isEnabledElink( int elink, int *elinkwidth )
{
  // Derive GBT, e-group and e-path numbers from the given e-link number
  int gbt    = (elink & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
  int egroup = (elink & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
  int epath  = (elink & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
  bool to_gbt = true;
  return _gbtConfig[gbt].isEnabled( egroup, epath, elinkwidth, to_gbt );
}

// ----------------------------------------------------------------------------
#if REGMAP_VERSION < 0x500
// ----------------------------------------------------------------------------

// The keys of the {key,value}-pairs in JSON/YAML files
static const char *KEY_LINK     = "Link";
static const char *KEY_LINKMODE = "LinkMode";
static const char *KEY_TTCCLOCK = "SetTtcClock";
static const char *KEY_CHUNKMAX = "LinkChunkMaxSize";
static const char *KEY_EGROUPS  = "TheEgroups";
static const char *KEY_EGROUP   = "Egroup";
static const char *KEY_ENA_FH   = "EnableFromHost";
static const char *KEY_ENA_TH   = "EnableToHost";
static const char *KEY_MODE_FH  = "ModeFromHost";
static const char *KEY_MODE_TH  = "ModeToHost";
static const char *KEY_STREAMID = "StreamId";
static const char *KEY_SETTINGS = "RegisterSettings";
static const char *KEY_FIELDNAME= "Name";
static const char *KEY_FIELDVAL = "Value";

bool FlxConfig::writeLinkConfig( const std::string &filename,
                                 bool write_it,
                                 int  emu_chunksize,
                                 bool emu_ewidth_dependent_size,
                                 bool emu_random_size,
                                 int  emu_idle_chars,
                                 bool emu_with_streamid,
                                 bool lsb_first,
                                 bool do_regsettings,
                                 int *elinks_th, int *eclinks_th,
                                 int *elinks_fh, int *eclinks_fh,
                                 int *link_mode, int *settings_cnt )
{
  // Open file
  std::ifstream file( filename );
  if( !file.is_open() )
    {
      _errString = "Failed to open ELC file: ";
      _errString += filename;
      return false;
    }

  // File name extension
  std::string ext;
  size_t pos = filename.find_last_of( "." );
  if( pos != string::npos )
    ext = filename.substr( pos+1 );

  // Read file contents (ASCII)
  int32_t  linknr = 0;
  uint32_t egroup = 0;
  uint32_t enables, chunksizes, linkmode, streamid;
  uint64_t modes;
  bool     ttcclock;
  std::vector<regsetting_t> regsettings;
  if( settings_cnt )
    *settings_cnt = 0;
  if( ext == std::string("jelc") )
    {
      try {
        // Read JSON formatted file
        json j;
        file >> j;
        for( json::iterator it = j.begin(); it != j.end(); ++it )
          {
            json lnk = *it;
            linknr = lnk[KEY_LINK];
            if( linknr > FLX_LINKS ) // Shouldn't read in past this number!
              continue;
            if( linknr == -1 ) // Emulator configuration
              linknr = FLX_LINKS;

            chunksizes = lnk[KEY_CHUNKMAX];
            _gbtConfig[linknr].setMaxChunkWord( chunksizes );

            linkmode = lnk[KEY_LINKMODE];
            _gbtConfig[linknr].setLinkMode( linkmode );

            if( linknr == 0 )
              {
                if( lnk.find( KEY_TTCCLOCK ) != lnk.end() )
                  ttcclock = lnk[KEY_TTCCLOCK];
                else
                  ttcclock = false;
                _gbtConfig[linknr].setTtcClock( ttcclock );
              }

            json egrps = lnk[KEY_EGROUPS];
            istringstream iss;
            for( json::iterator it2 = egrps.begin(); it2 != egrps.end(); ++it2 )
              {
                json egrp = *it2;
                egroup = egrp[KEY_EGROUP];

                // Convert to integers (from a hexadecimal representation)
                enables = 0; modes = 0;
                iss.clear();
                iss.str( egrp[KEY_ENA_TH] );
                iss >> hex >> enables;
                iss.clear();
                iss.str( egrp[KEY_MODE_TH] );
                iss >> hex >> modes;
                _gbtConfig[linknr].setEnablesToHost( egroup, enables );
                _gbtConfig[linknr].setModesToHost( egroup, modes );

                enables = 0; modes = 0;
                iss.clear();
                iss.str( egrp[KEY_ENA_FH] );
                iss >> hex >> enables;
                iss.clear();
                iss.str( egrp[KEY_MODE_FH] );
                iss >> hex >> modes;
                _gbtConfig[linknr].setEnablesFromHost( egroup, enables );
                _gbtConfig[linknr].setModesFromHost( egroup, modes );

                // Optional item (for backwards-compatibility)
                if( egrp.find( KEY_STREAMID ) != egrp.end() )
                  {
                    streamid = 0;
                    iss.clear();
                    iss.str( egrp[KEY_STREAMID] );
                    iss >> hex >> streamid;
                    _gbtConfig[linknr].setStreamIdBits( egroup, streamid );
                  }
              }

            // Any additional FELIX register/bitfield settings present ?
            if( lnk.find( KEY_SETTINGS ) != lnk.end() )
              {
                regsetting_t setting;
                json jregs = lnk[KEY_SETTINGS];
                json::iterator it;
                for( it = jregs.begin(); it != jregs.end(); ++it )
                  {
                    json jsetting = *it;
                    setting.name = jsetting[KEY_FIELDNAME];
                    istringstream iss;
                    iss.str( jsetting[KEY_FIELDVAL] );
                    iss >> hex >> setting.value;

                    // Add the setting to our list
                    regsettings.push_back( setting );
                  }
                if( settings_cnt )
                  *settings_cnt = regsettings.size();
              }
          }
      }
      catch( std::exception &ex ) {
        _errString += "Error while reading JELC-file:\n    ";
        _errString += ex.what();
        return false;
      }
    }
  else
    {
      file >> linknr;
      while( !file.eof() )
        {
          if( linknr > FLX_LINKS )
            break; // Shouldn't read in more than this!

          if( linknr == -1 ) // Emulator configuration
            linknr = FLX_LINKS;

          file >> egroup >> std::hex >> enables >> modes >> std::dec;

          //cout << "*** " << linknr << ", " << egroup << hex
          //     << ", 0x" << enables << ", 0x" << modes
          //     << std::dec << endl;

          if( file.fail() )
            {
              std::ostringstream oss;
              oss << "ELC file: something wrong @ Link " << linknr;
              _errString = oss.str();
              file.close();
              return false;
            }

          if( !GbtConfig::enablesIsValid( enables ) )
            {
              std::ostringstream oss;
              oss << "ELC file: Link " << linknr << ", Egroup " << egroup
                  << ": illegal value";
              _errString = oss.str();
              file.close();
              return false;
            }

          if( egroup < FLX_TOHOST_GROUPS )
            {
              _gbtConfig[linknr].setEnablesToHost( egroup, enables );
              _gbtConfig[linknr].setModesToHost( egroup, modes );
            }
          else
            {
              _gbtConfig[linknr].setEnablesFromHost( egroup-FLX_TOHOST_GROUPS,
                                                     enables );
              _gbtConfig[linknr].setModesFromHost( egroup-FLX_TOHOST_GROUPS,
                                                   modes );
            }

          // 7+1 From-GBT groups followed by 5+1 To-GBT groups
          // followed by chunksize word and linkmode parameter (0, 1 or 2)
          if( egroup == FLX_TOHOST_GROUPS + FLX_FROMHOST_GROUPS - 1 )
            {
              file >> std::hex >> chunksizes >> std::dec;
              _gbtConfig[linknr].setMaxChunkWord( chunksizes );

              // Linkmode value is incremented by 1000
              // to indicate TTC Clock is to be selected
              file >> linkmode;
              if( linknr == 0 )
                {
                  ttcclock = (linkmode >= 1000);
                  linkmode %= 1000;
                  _gbtConfig[linknr].setTtcClock( ttcclock );
                }
              _gbtConfig[linknr].setLinkMode( linkmode );
 
              // Next GBT?
              if( !file.eof() )
                file >> linknr;
            }
        }
    }
  file.close();

  if( _flx == 0 ) return false;

  uint64_t chans = FLX_LINKS;
  chans = _bar2->NUM_OF_CHANNELS;
  //if( _bar2->CARD_TYPE == 711 || _bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  // Check firmware/configuration compatibility
  uint32_t mode = _gbtConfig[0].linkMode();
  uint64_t fw   = _firmwareMode;//_flx->cfg_get_reg( REG_FIRMWARE_MODE );
  if( link_mode ) *link_mode = mode;
  if( (mode == LINKMODE_FULL &&
       (fw != FIRMW_FULL && fw != FIRMW_FMEMU && fw != FIRMW_MROD)) ||
      (mode != LINKMODE_FULL &&
       (fw == FIRMW_FULL || fw == FIRMW_FMEMU || fw == FIRMW_MROD)) )
    {
      _errString = "E-link config GBT<->FULLMODE: "
        "incompatible firmware and configuration!";
      return false;
    }

  // Configure requested 8b10b word bit order (to- and from-host direction)
  if( lsb_first )
    _flx->cfg_set_reg( REG_CR_REVERSE_10B, 0 );
  else
    _flx->cfg_set_reg( REG_CR_REVERSE_10B, 3 );

  if( elinks_th )
    {
      *elinks_th = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *elinks_th += _gbtConfig[gbt].numberOfElinksToHost();
    }
  if( eclinks_th )
    {
      *eclinks_th = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *eclinks_th += _gbtConfig[gbt].numberOfEcToHost();
    }
  if( elinks_fh )
    {
      *elinks_fh = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *elinks_fh += _gbtConfig[gbt].numberOfElinksFromHost();
    }
  if( eclinks_fh )
    {
      *eclinks_fh = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *eclinks_fh += _gbtConfig[gbt].numberOfEcFromHost();
    }

  if( write_it )
    {
      this->writeLinkConfig();
      if( mode != LINKMODE_FULL )
        this->writeGbtEmulatorData( emu_chunksize, emu_ewidth_dependent_size,
                                    emu_random_size, emu_idle_chars,
                                    emu_with_streamid,
                                    lsb_first );
      else
        this->writeFmEmulatorData( emu_chunksize,
                                   emu_random_size, emu_idle_chars,
                                   emu_with_streamid );

      // And finally the additional register settings provided, if any
      if( do_regsettings )
        for( regsetting_t &r : regsettings )
          _flx->cfg_set_option( r.name.c_str(), r.value );
    }

  return true;
}

// ----------------------------------------------------------------------------
#else
// ----------------------------------------------------------------------------

// The YAML file keys of the {key,value}-pairs
static const char *KEY_FORMAT     = "Format";
static const char *KEY_LINKS      = "Links";
static const char *KEY_LINK       = "Link";
static const char *KEY_LINKMODE   = "LinkMode";
static const char *KEY_TTCCLOCK   = "SetTtcClock";
static const char *KEY_CHUNKMAX   = "LinkChunkMaxSize";
//static const char *KEY_EGROUPS    = "Egroups";    // .jelc backwards compat
//static const char *KEY_EGROUPS_X  = "TheEgroups"; // .jelc backwards compat
static const char *KEY_EGROUP     = "Egroup";
static const char *KEY_EGROUPS_TH = "EgroupsToHost";
static const char *KEY_WIDTH_TH   = "WidthToHost";
static const char *KEY_ENA_TH     = "EnableToHost";
static const char *KEY_MODE_TH    = "ModeToHost";
static const char *KEY_STREAMID   = "StreamId";
static const char *KEY_DMAINDICES = "DmaIndices";
static const char *KEY_EGROUPS_FH = "EgroupsFromHost";
static const char *KEY_WIDTH_FH   = "WidthFromHost";
static const char *KEY_ENA_FH     = "EnableFromHost";
static const char *KEY_MODE_FH    = "ModeFromHost";
static const char *KEY_SETTINGS   = "RegisterSettings";
static const char *KEY_FIELDNAME  = "Name";
static const char *KEY_FIELDVAL   = "Value";

bool FlxConfig::writeLinkConfig( const std::string &filename,
                                 bool write_it,
                                 int  emu_chunksize,
                                 bool emu_ewidth_dependent_size,
                                 bool emu_random_size,
                                 int  emu_idle_chars,
                                 bool emu_with_streamid,
                                 bool lsb_first,
                                 bool do_regsettings,
                                 int *elinks_th, int *eclinks_th,
                                 int *elinks_fh, int *eclinks_fh,
                                 int *link_mode, int *settings_cnt )
{
  // Open file
  std::ifstream file( filename );
  if( !file.is_open() )
    {
      _errString = "Failed to open YELC file: ";
      _errString += filename;
      return false;
    }

  // File name extension
  std::string ext;
  size_t pos = filename.find_last_of( "." );
  if( pos != string::npos )
    ext = filename.substr( pos+1 );

  // Read file contents (ASCII)
  int32_t  gbt = 0;
  uint32_t egroup = 0;
  uint32_t width, enables, chunksizes, linkmode, streamid, dmaindices;
  uint64_t modes;
  bool     ttcclock;
  std::vector<regsetting_t> regsettings;
  if( settings_cnt )
    *settings_cnt = 0;
  if( ext == std::string("yelc") )
    {
      try {
        YAML::Node yaml = YAML::Load( file );
        if( !yaml[KEY_FORMAT] )
          {
            _errString += "YELC-file has wrong format (\"Format\" missing)";
            return false;
          }
        int format = yaml[KEY_FORMAT].as<int>();
        if( format != 2 )
          {
            _errString += "YELC-file: \"Format\" 2 required";
            return false;
          }

        YAML::Node links = yaml[KEY_LINKS];
        for( YAML::const_iterator it = links.begin(); it != links.end(); ++it )
          {
            YAML::Node lnk = *it;
            gbt = lnk[KEY_LINK].as<int>();
            if( gbt > FLX_LINKS ) // Shouldn't read in past this number!
              continue;
            if( gbt == -1 ) // Emulator configuration
              gbt = FLX_LINKS;

            chunksizes = lnk[KEY_CHUNKMAX].as<int>();
            _gbtConfig[gbt].setMaxChunkWord( chunksizes );

            linkmode = lnk[KEY_LINKMODE].as<int>();
            _gbtConfig[gbt].setLinkMode( linkmode );

            if( gbt == 0 )
              {
                if( lnk[KEY_TTCCLOCK] )
                  ttcclock = lnk[KEY_TTCCLOCK].as<bool>();
                else
                  ttcclock = false;
                _gbtConfig[gbt].setTtcClock( ttcclock );
              }

            YAML::Node egrps = lnk[KEY_EGROUPS_TH];
            istringstream iss;
            for( YAML::const_iterator it2 = egrps.begin();
                 it2 != egrps.end(); ++it2 )
              {
                YAML::Node egrp = *it2;
                egroup = egrp[KEY_EGROUP].as<int>();

                // Convert to integers (some from a hexadecimal representation)
                width = 0, enables = 0; modes = 0;
                width = egrp[KEY_WIDTH_TH].as<int>();
                iss.clear();
                iss.str( egrp[KEY_ENA_TH].as<std::string>() );
                iss >> hex >> enables;
                iss.clear();
                iss.str( egrp[KEY_MODE_TH].as<std::string>() );
                iss >> hex >> modes;
                _gbtConfig[gbt].setWidthToHost( egroup, width );
                _gbtConfig[gbt].setEnablesToHost( egroup, enables );
                _gbtConfig[gbt].setModesToHost( egroup, modes );

                // Optional item (for backwards-compatibility)
                if( egrp[KEY_DMAINDICES] )
                  {
                    dmaindices = 0;
                    iss.clear();
                    iss.str( egrp[KEY_DMAINDICES].as<std::string>() );
                    iss >> hex >> dmaindices;
                    _gbtConfig[gbt].setDmaIndices( egroup, dmaindices );
                  }

                streamid = 0;
                iss.clear();
                iss.str( egrp[KEY_STREAMID].as<std::string>() );
                iss >> hex >> streamid;
                _gbtConfig[gbt].setStreamIdBits( egroup, streamid );
              }

            egrps = lnk[KEY_EGROUPS_FH];
            for( YAML::const_iterator it2 = egrps.begin();
                 it2 != egrps.end(); ++it2 )
              {
                YAML::Node egrp = *it2;
                egroup = egrp[KEY_EGROUP].as<int>();

                // Convert to integers (some from a hexadecimal representation)
                width = 0; enables = 0; modes = 0;
                width = egrp[KEY_WIDTH_FH].as<int>();
                iss.clear();
                iss.str( egrp[KEY_ENA_FH].as<std::string>() );
                iss >> hex >> enables;
                iss.clear();
                iss.str( egrp[KEY_MODE_FH].as<std::string>() );
                iss >> hex >> modes;
                _gbtConfig[gbt].setWidthFromHost( egroup, width );
                _gbtConfig[gbt].setEnablesFromHost( egroup, enables );
                _gbtConfig[gbt].setModesFromHost( egroup, modes );
              }
          }

        YAML::Node settings = yaml[KEY_SETTINGS];
        regsetting_t regset;
        for( YAML::const_iterator it = settings.begin();
             it != settings.end(); ++it )
          {
            YAML::Node setting = *it;
            std::string field = setting[KEY_FIELDNAME].as<std::string>();
            uint64_t value = setting[KEY_FIELDVAL].as<uint64_t>();
            regset.name = field;
            regset.value = value;
            regsettings.push_back( regset );
          }
        if( settings_cnt )
          *settings_cnt = regsettings.size();
      }
      catch( std::exception &ex ) {
        _errString += "Error while reading YAML-file:\n    ";
        _errString += ex.what();
        return false;
      }
    }
  else
    {
      _errString += "File format not supported: .";
      _errString += ext;
      return false;
    }
  file.close();

  if( _flx == 0 ) return false;

  uint64_t chans = FLX_LINKS;
  chans = _bar2->NUM_OF_CHANNELS;
  //if( _bar2->CARD_TYPE == 711 || _bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  // Check firmware/configuration compatibility
  uint32_t mode = _gbtConfig[0].linkMode();
  uint64_t fw   = _firmwareMode;//_flx->cfg_get_reg( REG_FIRMWARE_MODE );
  if( link_mode ) *link_mode = mode;
  if( (mode == LINKMODE_FULL &&
       (fw != FIRMW_FULL && fw != FIRMW_FMEMU && fw != FIRMW_MROD)) ||
      (mode != LINKMODE_FULL &&
       (fw == FIRMW_FULL || fw == FIRMW_FMEMU || fw == FIRMW_MROD)) )
    {
      _errString = "E-link config GBT<->FULLMODE: "
        "incompatible firmware and configuration!";
      return false;
    }
  else if( mode == LINKMODE_LPGBT &&
           (fw != FIRMW_PIXEL && fw != FIRMW_STRIP && fw != FIRMW_LPGBT) )
    {
      _errString = "E-link config lpGBT: "
        "incompatible firmware and configuration!";
      return false;
    }

  // Configure requested 8b10b word bit order (to- and from-host direction)
  if( lsb_first )
    {
      _flx->cfg_set_option( BF_ENCODING_REVERSE_10B, 0 );
      _flx->cfg_set_option( BF_DECODING_REVERSE_10B, 0 );
    }
  else
    {
      _flx->cfg_set_option( BF_ENCODING_REVERSE_10B, 1 );
      _flx->cfg_set_option( BF_DECODING_REVERSE_10B, 1 );
    }

  if( elinks_th )
    {
      *elinks_th = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *elinks_th += _gbtConfig[gbt].numberOfElinksToHost();
    }
  if( eclinks_th )
    {
      *eclinks_th = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *eclinks_th += _gbtConfig[gbt].numberOfEcToHost();
    }
  if( elinks_fh )
    {
      *elinks_fh = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *elinks_fh += _gbtConfig[gbt].numberOfElinksFromHost();
    }
  if( eclinks_fh )
    {
      *eclinks_fh = 0;
      for( uint32_t gbt=0; gbt<chans; ++gbt )
        *eclinks_fh += _gbtConfig[gbt].numberOfEcFromHost();
    }

  if( write_it )
    {
      this->writeLinkConfig();
      if( mode != LINKMODE_FULL )
        this->writeGbtEmulatorData( emu_chunksize, emu_ewidth_dependent_size,
                                    emu_random_size, emu_idle_chars,
                                    emu_with_streamid,
                                    lsb_first );
      else
        this->writeFmEmulatorData( emu_chunksize,
                                   emu_random_size, emu_idle_chars,
                                   emu_with_streamid );

      // And finally the additional register settings provided, if any
      if( do_regsettings )
        for( regsetting_t &r : regsettings )
          _flx->cfg_set_option( r.name.c_str(), r.value );
    }

  return true;
}

// ----------------------------------------------------------------------------
#endif //REGMAP_VERSION
// ---------------------------------------------------------------------------- 
/*
int FlxConfig::firmwareMode()
{
  if( _flx == 0 ) return 0xFF;
  return _flx->cfg_get_reg( REG_FIRMWARE_MODE );
}

// ----------------------------------------------------------------------------

int FlxConfig::numberOfChans()
{
  if( _flx == 0 ) return 0;
  return _flx->cfg_get_reg( REG_NUM_OF_CHANNELS );
}
*/
// ----------------------------------------------------------------------------
// Private functions
// ----------------------------------------------------------------------------

void FlxConfig::readLinkConfig()
{
  if( _flx == 0 ) return;

  // Number of channels
  uint64_t chans = _bar2->NUM_OF_CHANNELS;
  //if( _bar2->CARD_TYPE == 711 || _bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  // Link mode
  uint32_t linkmode;
  _firmwareMode = _bar2->FIRMWARE_MODE;
  uint64_t fw = _firmwareMode;
  if( fw == FIRMW_FULL || fw == FIRMW_FMEMU || fw == FIRMW_MROD )
    {
      linkmode = LINKMODE_FULL;
    }
#if REGMAP_VERSION >= 0x500
  else if( fw == FIRMW_PIXEL || fw == FIRMW_STRIP || fw == FIRMW_LPGBT )
    {
      linkmode = LINKMODE_LPGBT;
    }
#endif // REGMAP_VERSION
  else
    {
      fw = _bar2->WIDE_MODE;
      if( fw == 1 )
        linkmode = LINKMODE_GBTWIDE;
      else
        linkmode = LINKMODE_GBT;
    }
  for( uint64_t gbt=0; gbt<chans; ++gbt )
    _gbtConfig[gbt].setLinkMode( linkmode );

  uint64_t gbt, egroup;
  uint32_t enables, truncation;
  uint64_t modes;
  // To Host:
  for( gbt=0; gbt<chans; ++gbt )
    {
      truncation = 0;
      for( egroup=0; egroup<FLX_TOHOST_GROUPS-1; ++egroup )
        {
#if REGMAP_VERSION < 0x500
          flxcard_cr_tohost_egroup_ctrl_t config =
                     _bar2->CR_GBT_CTRL[gbt].EGROUP_TOHOST[egroup].TOHOST;
          uint64_t modebits;
          enables  = (uint32_t) config.EPROC_ENA;
          modebits = config.PATH_ENCODING; // 2 bits per E-path!
          truncation = (uint32_t) config.MAX_CHUNK_LEN;
          if( egroup == 0 )
            _gbtConfig[gbt].setMaxChunkWord( truncation );

          // Translate 'mode' word from register to local format 'modes' word,
          // i.e. 4 bits per E-PROC
          //modes = GbtConfig::epath2BitModeWordToModes( enables, modebits );
          modes = GbtConfig::epath2BitModeWordToModes( modebits );

          // FULL mode?
          if( egroup == 0 && linkmode == LINKMODE_FULL )
            if( (_bar2->CR_FM_PATH_ENA & (1<<gbt)) != 0 )
              enables |= 0x10000;
#else
          flxcard_decoding_egroup_ctrl_t config =
            _bar2->DECODING_EGROUP_CTRL_GEN[gbt].DECODING_EGROUP[egroup].EGROUP;
          uint32_t width;
          // There are only 8 enable bits: the width setting determines
          // what the E-links width is (they are the same within an E-group)
          enables = (uint32_t) config.EPATH_ENA;
          width   = (uint32_t) config.EPATH_WIDTH;
          modes   = config.PATH_ENCODING; // 4 bits per E-path!
          // Collect HDLC truncation bits
          truncation |= (config.ENABLE_TRUNCATION << (egroup+3));

          // FULL mode?
          if( egroup == 0 && linkmode == LINKMODE_FULL )
            if( (_bar2->DECODING_EGROUP_CTRL_GEN[gbt].DECODING_EGROUP[0].EGROUP.EPATH_ENA & 1) != 0 )
              enables |= 0x10000;

          _gbtConfig[gbt].setWidthToHost( egroup, width );
#endif // REGMAP_VERSION

          // Store in our local configuration structure
          _gbtConfig[gbt].setEnablesToHost( egroup, enables );
          _gbtConfig[gbt].setModesToHost( egroup, modes );
        }

      // and the EC+IC+AUX+TTC egroup
      enables = 0x0000;

#if REGMAP_VERSION < 0x500
      flxcard_ec_tohost_t *ec_tohost =
                     &_bar2->MINI_EGROUP_CTRL[gbt].EC_TOHOST;

      // To-host EC enabled/disabled
      if( ec_tohost->ENABLE )
        enables |= 0x4000;

      // To-host EC encoding:
      // translate 'mode' word from register to local format 'modes' word,
      // i.e. 4 bits per E-PROC
      uint64_t modebits;
      modebits = ec_tohost->ENCODING << (7*2);
      modes = GbtConfig::epath2BitModeWordToModes( modebits );

      // To-host IC enabled/disabled
      if( ec_tohost->IC_ENABLE )
        enables |= 0x2000;

      // To-host AUX enabled/disabled
      if( ec_tohost->SCA_AUX_ENABLE )
        enables |= 0x1000;

      // TTC-to-Host enabled/disabled
      if( gbt == 0 && _bar2->CR_TTC_TOHOST.ENABLE )
        enables |= 0x0400;
#else
      flxcard_mini_egroup_tohost_t *mini_egroup_tohost =
        &_bar2->MINI_EGROUP_TOHOST_GEN[gbt].MINI_EGROUP_TOHOST;

      // To-host EC enabled/disabled
      if( mini_egroup_tohost->EC_ENABLE )
        enables |= 0x4000;

      // To-host EC encoding: 4 bits
      modes = (uint64_t) mini_egroup_tohost->EC_ENCODING << (14*4);

      // To-host IC enabled/disabled
      if( mini_egroup_tohost->IC_ENABLE )
        enables |= 0x2000;

      // To-host AUX enabled/disabled
      if( mini_egroup_tohost->AUX_ENABLE )
        enables |= 0x1000;

      // TTC-to-Host enabled/disabled
      if( gbt == 0 && _bar2->TTC_TOHOST_ENABLE )
        enables |= 0x0400;

      // Collect HDLC truncation bits
      if( mini_egroup_tohost->ENABLE_EC_TRUNCATION )
        truncation |= (1<<0);
      if( mini_egroup_tohost->ENABLE_IC_TRUNCATION )
        truncation |= (1<<1);
      if( mini_egroup_tohost->ENABLE_AUX_TRUNCATION )
        truncation |= (1<<2);
      // ..and configure
      _gbtConfig[gbt].setMaxChunkWord( truncation );
#endif // REGMAP_VERSION

      // Store in our local configuration structure
      _gbtConfig[gbt].setEnablesToHost( FLX_TOHOST_GROUPS-1, enables );
      _gbtConfig[gbt].setModesToHost( FLX_TOHOST_GROUPS-1, modes );

      // TTC Clock setting
      if( gbt == 0 )
        {
          bool ttcclock = (_bar2->MMCM_MAIN.LCLK_SEL == 0);
          _gbtConfig[gbt].setTtcClock( ttcclock );
        }

      // Stream ID bits
#if REGMAP_VERSION < 0x500
      uint64_t *p = (uint64_t *) &_bar2->PATH_HAS_STREAM_ID[gbt].TOHOST;
#else
      uint64_t *p = (uint64_t *) &_bar2->PATH_HAS_STREAM_ID[gbt];
#endif
      uint64_t streamid = *p;
      for( egroup=0; egroup<FLX_TOHOST_GROUPS-1; ++egroup )
        {
          _gbtConfig[gbt].setStreamIdBits( egroup, streamid & 0xFF );
          streamid >>= 8;
        }
    }

  // From Host:
  for( gbt=0; gbt<chans; ++gbt )
    {
      for( egroup=0; egroup<FLX_FROMHOST_GROUPS-1; ++egroup )
        {
#if REGMAP_VERSION < 0x500
          flxcard_cr_fromhost_egroup_ctrl_t config =
                     _bar2->CR_GBT_CTRL[gbt].EGROUP_FROMHOST[egroup].FROMHOST;
          enables  = (uint32_t) config.EPROC_ENA;
          uint64_t modebits;
          modebits = config.PATH_ENCODING; // 4 bits per E-path!

          // Translate 'mode' word from register to local format
          // 'modes' word i.e. 4 bits per E-PROC
          //modes = GbtConfig::epath4BitModeWordToModes( enables, modewrd );
          modes = GbtConfig::epath4BitModeWordToModes( modebits );
#else
          flxcard_encoding_egroup_ctrl_t config =
            _bar2->ENCODING_EGROUP_CTRL_GEN[gbt].ENCODING_EGROUP[egroup].ENCODING_EGROUP_CTRL;
          uint32_t width;
          enables = (uint32_t) config.EPATH_ENA;
          width   = (uint32_t) config.EPATH_WIDTH;
          modes   = config.PATH_ENCODING; // 4 bits per E-path!

          _gbtConfig[gbt].setWidthFromHost( egroup, width );
#endif // REGMAP_VERSION

          // Store in our local configuration structure
          _gbtConfig[gbt].setEnablesFromHost( egroup, enables );
          _gbtConfig[gbt].setModesFromHost( egroup, modes );
        }

      // and the EC+IC+AUX egroup
      enables = 0x0000;

#if REGMAP_VERSION < 0x500
      flxcard_ec_fromhost_t *ec_fromhost =
                     &_bar2->MINI_EGROUP_CTRL[gbt].EC_FROMHOST;

      // From-host EC enabled/disabled
      if( ec_fromhost->ENABLE )
        enables |= 0x4000;

      // From-host EC encoding
      //modebits = ((config & 0x1E) >> 1) << (7*4);
      uint64_t modebits;
      modebits = ec_fromhost->ENCODING << (7*4);
      modes = GbtConfig::epath4BitModeWordToModes( modebits );

      // From-host IC enabled/disabled
      if( ec_fromhost->IC_ENABLE )
        enables |= 0x2000;

      // From-host AUX enabled/disabled
      if( ec_fromhost->SCA_AUX_ENABLE )
        enables |= 0x1000;
#else
      flxcard_mini_egroup_fromhost_t *mini_egroup_fromhost =
        &_bar2->MINI_EGROUP_FROMHOST_GEN[gbt].MINI_EGROUP_FROMHOST;

      // From-host EC enabled/disabled
      if( mini_egroup_fromhost->EC_ENABLE )
        enables |= 0x4000;

      // From-host EC encoding: 4 bits
      modes = (uint64_t) mini_egroup_fromhost->EC_ENCODING << (14*4);

      // From-host IC enabled/disabled
      if( mini_egroup_fromhost->IC_ENABLE )
        enables |= 0x2000;

      // From-host AUX enabled/disabled
      if( mini_egroup_fromhost->AUX_ENABLE)
        enables |= 0x1000;
#endif // REGMAP_VERSION

      // Store in our local configuration structure
      _gbtConfig[gbt].setEnablesFromHost( FLX_FROMHOST_GROUPS-1, enables );
      _gbtConfig[gbt].setModesFromHost( FLX_FROMHOST_GROUPS-1, modes );
    }
}

// ----------------------------------------------------------------------------

#define EGROUP_CTRL_MODE_SHIFT     15
#define EGROUP_CTRL_MAXCHUNK_SHIFT 31
#define EMU_CONFIG_ADDR_SHIFT      32

void FlxConfig::writeLinkConfig()
{
  if( _flx == 0 ) return;

  // Disable any active emulators
#if REGMAP_VERSION < 0x500
  _flx->cfg_set_reg( REG_GBT_EMU_ENA, 0 );
  _flx->cfg_set_reg( REG_GBT_FM_EMU_ENA_TOHOST, 0 );
#else
  _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOHOST, 0 );
  _flx->cfg_set_option( BF_FE_EMU_ENA_EMU_TOFRONTEND, 0 );
#endif // REGMAP_VERSION

  int chans = _bar2->NUM_OF_CHANNELS;
  //if( _bar2->CARD_TYPE == 711 || _bar2->CARD_TYPE == 712 ) chans /= 2;
  if( chans > FLX_LINKS ) chans = FLX_LINKS;

  // Write configuration directly into the selected FLX-card
  GbtConfig *cfg;
  uint32_t enables;
  uint64_t modes;
#if REGMAP_VERSION < 0x0500
  uint32_t modes_word;
#endif // REGMAP_VERSION
  uint32_t truncation_word;

  // To Host:
  int lnk, max_grp = FLX_TOHOST_GROUPS-1;
  _dmaIndexInvalidCount = 0;
  for( lnk=0; lnk<chans; ++lnk )
    {
      cfg = &_gbtConfig[lnk];
      if( cfg->linkMode() == LINKMODE_FULL )
        {
          // FULL mode: do egroup configuration eventhough it is meaningless..
          // (the FULL mode enable bit is 'hidden' in group 0)
          for( int grp=0; grp<max_grp; ++grp )
            {
              enables = 0;
              if( grp == 0 )
                {
#if REGMAP_VERSION < 0x500                    
                  if( (cfg->enablesToHost( grp ) & 0x10000) != 0 )
                    //enables = 0x0001;
                    _bar2->CR_FM_PATH_ENA |= (1 << lnk);
                  else
                    //enables = 0;
                    _bar2->CR_FM_PATH_ENA &= ~(1 << lnk);
#else
                  if( (cfg->enablesToHost( grp ) & 0x10000) != 0 )
                    _bar2->DECODING_EGROUP_CTRL_GEN[lnk].DECODING_EGROUP[0].EGROUP.EPATH_ENA = 1;
                  else
                    _bar2->DECODING_EGROUP_CTRL_GEN[lnk].DECODING_EGROUP[0].EGROUP.EPATH_ENA = 0;
#endif // REGMAP_VERSION
                }
#if REGMAP_VERSION < 0x0500
              modes             = cfg->modesToHost( grp );
              modes_word        = GbtConfig::epath2BitModeWord( enables, modes );
              truncation_word = cfg->maxChunkWord();

              flxcard_cr_tohost_egroup_ctrl_t *ctrl =
                &_bar2->CR_GBT_CTRL[lnk].EGROUP_TOHOST[grp].TOHOST;
              ctrl->EPROC_ENA     = enables;
              ctrl->PATH_ENCODING = modes_word;
              ctrl->MAX_CHUNK_LEN = truncation_word;
#else
              // Do nothing...
#endif // REGMAP_VERSION
            }
        }
      else
        {
          for( int grp=0; grp<max_grp; ++grp )
            {
              enables         = cfg->enablesToHost( grp );
              modes           = cfg->modesToHost( grp );
              truncation_word = cfg->maxChunkWord();
#if REGMAP_VERSION < 0x0500
              modes_word        = GbtConfig::epath2BitModeWord( enables, modes );
              flxcard_cr_tohost_egroup_ctrl_t *ctrl =
                &_bar2->CR_GBT_CTRL[lnk].EGROUP_TOHOST[grp].TOHOST;
              ctrl->EPROC_ENA     = enables;
              ctrl->PATH_ENCODING = modes_word;
              ctrl->MAX_CHUNK_LEN = truncation_word;
#else
              flxcard_decoding_egroup_ctrl_t *ctrl =
                &_bar2->DECODING_EGROUP_CTRL_GEN[lnk].DECODING_EGROUP[grp].EGROUP;
              uint32_t width      = cfg->widthToHost( grp );
              ctrl->EPATH_ENA     = enables;
              ctrl->EPATH_WIDTH   = width;
              ctrl->PATH_ENCODING = modes;
              // For now: set HDLC truncation globally
              if( truncation_word > 0 )
                ctrl->ENABLE_TRUNCATION = 1;
              else
                ctrl->ENABLE_TRUNCATION = 0;
#endif // REGMAP_VERSION
            }
          // and the EC+IC+AUX+TTC group
          enables = cfg->enablesToHost( FLX_TOHOST_GROUPS-1 );
          modes   = cfg->modesToHost( FLX_TOHOST_GROUPS-1 );

#if REGMAP_VERSION < 0x500
          flxcard_ec_tohost_t *ec_tohost =
            &_bar2->MINI_EGROUP_CTRL[lnk].EC_TOHOST;

          // To-host EC enabled/disabled
          if( enables & 0x4000 )
            ec_tohost->ENABLE = 1;
          else
            ec_tohost->ENABLE = 0;

          // To-host EC mode
          ec_tohost->ENCODING = (modes >>(14*4)) & 0xF;

          // To-host IC enabled/disabled
          if( enables & 0x2000 )
            ec_tohost->IC_ENABLE = 1;
          else
            ec_tohost->IC_ENABLE = 0;

          // To-host SCA-AUX enabled/disabled
          if( enables & 0x1000 )
            ec_tohost->SCA_AUX_ENABLE = 1;
          else
            ec_tohost->SCA_AUX_ENABLE = 0;
#else
          flxcard_mini_egroup_tohost_t *mini_egroup_tohost =
            &_bar2->MINI_EGROUP_TOHOST_GEN[lnk].MINI_EGROUP_TOHOST;

          // To-host EC enabled/disabled
          if( enables & 0x4000 )
            mini_egroup_tohost->EC_ENABLE = 1;
          else
            mini_egroup_tohost->EC_ENABLE = 0;

          // To-host EC mode
          mini_egroup_tohost->EC_ENCODING = (modes >>(14*4)) & 0xF;

          // To-host IC enabled/disabled
          if( enables & 0x2000 )
            mini_egroup_tohost->IC_ENABLE = 1;
          else
            mini_egroup_tohost->IC_ENABLE = 0;

          // To-host SCA-AUX enabled/disabled
          if( enables & 0x1000 )
            mini_egroup_tohost->AUX_ENABLE = 1;
          else
            mini_egroup_tohost->AUX_ENABLE = 0;

          // For now: set HDLC truncation globally
          if( truncation_word > 0 )
            {
              // Don't truncate IC
              mini_egroup_tohost->ENABLE_EC_TRUNCATION = 1;
              mini_egroup_tohost->ENABLE_IC_TRUNCATION = 0;
              mini_egroup_tohost->ENABLE_AUX_TRUNCATION = 1;
            }
          else
            {
              mini_egroup_tohost->ENABLE_EC_TRUNCATION = 0;
              mini_egroup_tohost->ENABLE_IC_TRUNCATION = 0;
              mini_egroup_tohost->ENABLE_AUX_TRUNCATION = 0;
            }
#endif // REGMAP_VERSION
        }

      // To-host TTC enabled/disabled
      if( lnk == 0 )
        {
          enables = cfg->enablesToHost( FLX_TOHOST_GROUPS-1 );
#if REGMAP_VERSION < 0x500
          if( enables & 0x0400 )
            _bar2->CR_TTC_TOHOST.ENABLE = 1;
          else
            _bar2->CR_TTC_TOHOST.ENABLE = 0;
#else
          if( enables & 0x0400 )
            _bar2->TTC_TOHOST_ENABLE = 1;
          else
            _bar2->TTC_TOHOST_ENABLE = 0;
#endif // REGMAP_VERSION
        }

      // Select TTC Clock ?
      if( lnk == 0 )
        {
          if( cfg->ttcClock() )
            _bar2->MMCM_MAIN.LCLK_SEL = 0;
        }

#if REGMAP_VERSION >= 0x0500
      // E-link DMA controller indices
      int elinknr, dma_index;
      int max_pathnr;
      if( cfg->linkMode() == LINKMODE_LPGBT )
        max_pathnr = 4;
      else
        max_pathnr = 8;
      // DMA count includes the FromHost DMA
      int dma_count = _bar2->GENERIC_CONSTANTS.DESCRIPTORS - 1;
      for( int group=0; group<max_grp; ++group )
        {
          uint32_t dma_indices = cfg->dmaIndices( group );
          for( int path=0; path<max_pathnr; ++path )
            {
              if( cfg->linkMode() == LINKMODE_LPGBT )
                elinknr = ((lnk << BLOCK_GBT_SHIFT) |
                           (group << (BLOCK_EGROUP_SHIFT-1)) | // Epath: 1 bit less 
                           (path << BLOCK_EPATH_SHIFT));
              else
                elinknr = ((lnk << BLOCK_GBT_SHIFT) |
                           (group << BLOCK_EGROUP_SHIFT) |
                           (path << BLOCK_EPATH_SHIFT));

              // Set 'address' (and read current DMA index setting)
              _bar2->CRTOHOST_DMA_DESCRIPTOR_2.AXIS_ID = elinknr;
              // Write DMA index setting, but check for a valid number
              dma_index = (dma_indices >> (path*4)) & 0xF;
              if( dma_index > dma_count-1 )
                {
                  dma_index = 0; // Use DMA index 0 instead
                  ++_dmaIndexInvalidCount;
                }
              _bar2->CRTOHOST_DMA_DESCRIPTOR_1.DESCR = dma_index;
            }
        }
      // EC E-link DMA index
      uint32_t dma_indices = cfg->dmaIndices( FLX_TOHOST_GROUPS-1 );
      elinknr = (lnk << BLOCK_GBT_SHIFT) | _bar2->AXI_STREAMS_TOHOST.EC_INDEX;
      _bar2->CRTOHOST_DMA_DESCRIPTOR_2.AXIS_ID = elinknr;
      // Setting stored in Egroup 7, path 7
      dma_index = (dma_indices >> (7*4)) & 0xF;
      if( dma_index > dma_count-1 )
        {
          dma_index = 0; // Use DMA index 0 instead
          ++_dmaIndexInvalidCount;
        }
      _bar2->CRTOHOST_DMA_DESCRIPTOR_1.DESCR = dma_index;
      // IC E-link DMA index
      elinknr = (lnk << BLOCK_GBT_SHIFT) | _bar2->AXI_STREAMS_TOHOST.IC_INDEX;
      _bar2->CRTOHOST_DMA_DESCRIPTOR_2.AXIS_ID = elinknr;
      // Setting stored in Egroup 7, path 7
      dma_index = (dma_indices >> (6*4)) & 0xF;
      if( dma_index > dma_count-1 )
        {
          dma_index = 0; // Use DMA index 0 instead
          ++_dmaIndexInvalidCount;
        }
      _bar2->CRTOHOST_DMA_DESCRIPTOR_1.DESCR = dma_index;
      // AUX E-link DMA index
      elinknr = (lnk << BLOCK_GBT_SHIFT) | (_bar2->AXI_STREAMS_TOHOST.IC_INDEX + 1);
      _bar2->CRTOHOST_DMA_DESCRIPTOR_2.AXIS_ID = elinknr;
      // Setting stored in Egroup 7, path 5
      dma_index = (dma_indices >> (5*4)) & 0xF;
      if( dma_index > dma_count-1 )
        {
          dma_index = 0; // Use DMA index 0 instead
          ++_dmaIndexInvalidCount;
        }
      _bar2->CRTOHOST_DMA_DESCRIPTOR_1.DESCR = dma_index;
#endif // REGMAP_VERSION
      
      // Stream ID bits
#if REGMAP_VERSION < 0x500
      uint64_t *p = (uint64_t *) &_bar2->PATH_HAS_STREAM_ID[lnk].TOHOST;
#else
      uint64_t *p = (uint64_t *) &_bar2->PATH_HAS_STREAM_ID[lnk];
#endif // REGMAP_VERSION
      uint64_t val = *p;
      for( int grp=0; grp<max_grp; ++grp )
        {
          // Clear and set Egroup's streamid bits
          val &= ~(0xFFULL << (grp*8));
          val |= (((uint64_t) cfg->streamIdBits( grp ) & 0xFFULL) << (grp*8));
        }
      *p = val;
    }

  // From Host:
  max_grp = FLX_FROMHOST_GROUPS-1;
  for( lnk=0; lnk<chans; ++lnk )
    {
      cfg = &_gbtConfig[lnk];
      for( int grp=0; grp<max_grp; ++grp )
        {
          enables = cfg->enablesFromHost( grp );
          modes   = cfg->modesFromHost( grp );
#if REGMAP_VERSION < 0x500
          modes_word = GbtConfig::epath4BitModeWord( enables, modes );
          flxcard_cr_fromhost_egroup_ctrl_t *ctrl =
            &_bar2->CR_GBT_CTRL[lnk].EGROUP_FROMHOST[grp].FROMHOST;
          ctrl->EPROC_ENA     = enables;
          ctrl->PATH_ENCODING = modes_word;
#else
          flxcard_encoding_egroup_ctrl_t *ctrl =
            &_bar2->ENCODING_EGROUP_CTRL_GEN[lnk].ENCODING_EGROUP[grp].ENCODING_EGROUP_CTRL;
          uint32_t width      = cfg->widthFromHost( grp );
          ctrl->EPATH_ENA     = enables;
          ctrl->EPATH_WIDTH   = width;
          ctrl->PATH_ENCODING = modes;
#endif // REGMAP_VERSION
        }
      // and the EC+IC+AUX group
      enables = cfg->enablesFromHost( FLX_FROMHOST_GROUPS-1 );
      modes   = cfg->modesFromHost( FLX_FROMHOST_GROUPS-1 );

#if REGMAP_VERSION < 0x500
      flxcard_ec_fromhost_t *ec_fromhost =
        &_bar2->MINI_EGROUP_CTRL[lnk].EC_FROMHOST;

      // From-host EC enabled/disabled
      if( enables & 0x4000 )
        ec_fromhost->ENABLE = 1;
      else
        ec_fromhost->ENABLE = 0;

      // From-host EC mode
      ec_fromhost->ENCODING = (modes>>(14*4)) & 0xF;

      // From-host IC enabled/disabled
      if( enables & 0x2000 )
        ec_fromhost->IC_ENABLE = 1;
      else
        ec_fromhost->IC_ENABLE = 0;

      // From-host SCA-AUX enabled/disabled
      if( enables & 0x1000 )
        ec_fromhost->SCA_AUX_ENABLE = 1;
      else
        ec_fromhost->SCA_AUX_ENABLE = 0;
#else
      flxcard_mini_egroup_fromhost_t *mini_egroup_fromhost =
        &_bar2->MINI_EGROUP_FROMHOST_GEN[lnk].MINI_EGROUP_FROMHOST;

      // From-host EC enabled/disabled
      if( enables & 0x4000 )
        mini_egroup_fromhost->EC_ENABLE = 1;
      else
        mini_egroup_fromhost->EC_ENABLE = 0;

      // From-host EC mode
      mini_egroup_fromhost->EC_ENCODING = (modes>>(14*4)) & 0xF;

      // From-host IC enabled/disabled
      if( enables & 0x2000 )
        mini_egroup_fromhost->IC_ENABLE = 1;
      else
        mini_egroup_fromhost->IC_ENABLE = 0;

      // From-host SCA-AUX enabled/disabled
      if( enables & 0x1000 )
        mini_egroup_fromhost->AUX_ENABLE = 1;
      else
        mini_egroup_fromhost->AUX_ENABLE = 0;
#endif // REGMAP_VERSION
    }

  _flx->soft_reset();
}

// ----------------------------------------------------------------------------

void FlxConfig::writeGbtEmulatorData( uint32_t req_chunksize,
                                      bool     width_prop_sz,
                                      bool     random_sz,
                                      uint32_t idle_chars,
                                      bool     use_streamid,
                                      bool     lsb_first )
{
  EmuDataGenerator emugenerator;
  uint32_t   emudata[16384];
  uint32_t   emusize = 16384;
  uint64_t   nbits_word;
  uint64_t   nbits_index_word;
  uint32_t   modes_word;
  uint32_t   truncation_word;
  //uint32_t req_chunksize = 32;
  uint32_t   pattern_id    = 0;
  //uint32_t idle_chars    = 8;
  //bool     lsb_first     = true;
  //bool     width_prop_sz = false;
  //bool     random_sz     = false;
  GbtConfig *cfg;

  if( _flx == 0 ) return;

  // Added 18 Dec 2018
  //bool emuram_msb_first = false;
  //if( _flx->cfg_get_reg( REG_REG_MAP_VERSION ) > 0x404 ||
  //    (_flx->cfg_get_reg( REG_REG_MAP_VERSION ) == 0x404 &&
  //     _flx->cfg_get_reg( REG_GIT_COMMIT_NUMBER ) >= 119) )
  //  emuram_msb_first = !lsb_first;
  // Above changed to this (24 Sep 2021)
  bool emuram_msb_first = !lsb_first;

  // Generate emulator data based on the configuration of GBT link #0
  //cfg = &_gbtConfig[0];
  // Generate emulator data based on the configuration of GBT link EMU
  cfg = &_gbtConfig[FLX_LINKS];
  uint32_t enables;
  uint64_t modes;
  // GBT Normal: 5, GBT Wide or lpGBT: 7 groups
  uint32_t max_grp = ((cfg->linkMode() == LINKMODE_GBTWIDE ||
                       cfg->linkMode() == LINKMODE_LPGBT) ? 7 : 5);
  uint32_t egroup;
  for( egroup=0; egroup<max_grp; ++egroup )
    {
      enables = cfg->enablesToHost( egroup );
      modes   = cfg->modesToHost( egroup );
#if REGMAP_VERSION < 0x500
      nbits_word = GbtConfig::epathBitsWord( enables );
      modes_word = GbtConfig::epath4BitModeWord( enables, modes );
      nbits_index_word  = GbtConfig::epathBitIndexWord( enables );
#else
      uint32_t width_in_bits = GbtConfig::widthToBits( cfg->widthToHost(egroup) );
      nbits_word = GbtConfig::epathBitsWord( enables, width_in_bits );
      modes_word = modes;
      if( cfg->linkMode() == LINKMODE_LPGBT )
        nbits_index_word = 0x0000000018100800;
      else
        nbits_index_word = 0x0E0C0A0806040200;
#endif // REGMAP_VERSION
      truncation_word = cfg->maxChunkWord();
      emugenerator.generate( emudata, emusize,
                             nbits_word, nbits_index_word, modes_word,
                             truncation_word, req_chunksize, egroup,
                             pattern_id, idle_chars, lsb_first,
                             emuram_msb_first,
                             width_prop_sz, random_sz, use_streamid );

      _emulatorChunkCount = emugenerator.chunkCount();
      _emulatorChunkRate = (((double) _emulatorChunkCount * 40079.0) /
                            ((double) emusize)); // Clock 40.079 MHz

      // Upload generated emulator data for this egroup into the FLX-card
      uint64_t egroup_bitmask = (1 << egroup);
      uint64_t addr = 0;
      uint32_t i;
#if REGMAP_VERSION < 0x500
      flxcard_gbt_emu_config_t *emuconfig = &_bar2->GBT_EMU_CONFIG;
      for( i=0; i<emusize; ++i, addr+=1ULL )
        {
          emuconfig->WRADDR = addr;
          emuconfig->WRDATA = emudata[i];
          _bar2->GBT_EMU_CONFIG_WE_ARRAY = egroup_bitmask;
          _bar2->GBT_EMU_CONFIG_WE_ARRAY = 0;
        }
      //for( i=0; i<emusize; ++i, addr+=(1ULL<<EMU_CONFIG_ADDR_SHIFT) )
      //  {
      //    uint64_t addr_plus_data = emudata[i] | addr;
      //    _flx->cfg_set_reg( REG_GBT_EMU_CONFIG, addr_plus_data );
      //    _flx->cfg_set_reg( REG_GBT_EMU_CONFIG_WE_ARRAY, egroup_bitmask );
      //    _flx->cfg_set_reg( REG_GBT_EMU_CONFIG_WE_ARRAY, 0 );
      //  }
#else
      flxcard_fe_emu_config_t *emuconfig = &_bar2->FE_EMU_CONFIG;
      for( i=0; i<emusize; ++i, addr+=1ULL )
        {
          emuconfig->WRADDR = addr;
          emuconfig->WRDATA = emudata[i];
          emuconfig->WE     = egroup_bitmask;
          emuconfig->WE     = 0;
          //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WRADDR, addr );
          //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WRDATA, emudata[i] );
          //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WE, egroup_bitmask );
          //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WE, 0 );
        }
#endif // REGMAP_VERSION
    }
}

// ----------------------------------------------------------------------------

void FlxConfig::writeFmEmulatorData( uint32_t req_chunksize,
                                     bool     random_sz,
                                     uint32_t idle_chars,
                                     bool     use_streamid )
{
  // Generate FULLMODE emulator data
  EmuDataGenerator datagenerator;
  uint64_t emudata[8192];
  uint64_t emusize    = 8192;
  uint32_t pattern_id = 0;
  bool     crc_new    = false;

  if( _flx == 0 ) return;

  // CRC version to use
  if( _flx->cfg_get_reg( REG_REG_MAP_VERSION ) > 0x402 ||
      (_flx->cfg_get_reg( REG_REG_MAP_VERSION ) == 0x402 &&
       _flx->cfg_get_reg( REG_GIT_COMMIT_NUMBER ) >= 47) )
    crc_new = true;

  datagenerator.generateFm( emudata, emusize, req_chunksize,
                            pattern_id, idle_chars, random_sz, crc_new,
                            use_streamid );

  _emulatorChunkCount = datagenerator.chunkCount();
  _emulatorChunkRate = (((double) _emulatorChunkCount * 40079.0) /
                        ((double) emusize)); // Clock 40.079 MHz

  // Write emulator data into the FLX card
  uint64_t addr = 0;
  uint64_t i;
#if REGMAP_VERSION < 0x500
  flxcard_gbt_fm_emu_config_t *emuconfig = &_bar2->GBT_FM_EMU_CONFIG;
  for( i=0; i<emusize; ++i, addr+=1ULL )
    {
      emuconfig->WRADDR = addr;
      emuconfig->WRDATA = emudata[i];
      _bar2->GBT_FM_EMU_CONFIG_WE_ARRAY = 1;
      _bar2->GBT_FM_EMU_CONFIG_WE_ARRAY = 0;
    }
  //const uint64_t FM_EMU_CONFIG_ADDR_SHIFT = 40;
  //for( i=0; i<emusize; ++i, addr+=(1ULL<<FM_EMU_CONFIG_ADDR_SHIFT) )
  //  {
  //    uint64_t addr_plus_data = emudata[i] | addr;
  //    _flx->cfg_set_reg( REG_GBT_FM_EMU_CONFIG, addr_plus_data );
  //    _flx->cfg_set_reg( REG_GBT_FM_EMU_CONFIG_WE_ARRAY, 1 );
  //    _flx->cfg_set_reg( REG_GBT_FM_EMU_CONFIG_WE_ARRAY, 0 );
  //  }
#else
  flxcard_fe_emu_config_t *emuconfig = &_bar2->FE_EMU_CONFIG;
  for( i=0; i<emusize; ++i, addr+=1ULL )
    {
      emuconfig->WRADDR = addr;
      emuconfig->WRDATA = emudata[i];
      emuconfig->WE     = 1;
      emuconfig->WE     = 0;
      //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WRADDR, addr );
      //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WRDATA, emudata[i] );
      //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WE, 1 );
      //_flx->cfg_set_option( BF_FE_EMU_CONFIG_WE, 0 );
    }
#endif // REGMAP_VERSION
}

// ----------------------------------------------------------------------------
