#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <string.h>
#include "I2cGpio.h"
#include "FlxDataChecker.h"
using namespace std;

// To map GPIO bit number to a byte index in the GPIO SCA command
const int GPIO_BYTE_MAP[] = { 2, 3, 0, 1 };

// ----------------------------------------------------------------------------

I2cGpio::I2cGpio( FlxUpload &fup, FlxReceiver &frecv,
                  int elinknr, int scl, int sda, int khz )
  : _scl( scl ),
    _sda( sda ),
    _kHz( khz ),
    _elinknr( elinknr ),
    _trid( 1 ),
    _fup( fup ),
    _frecv( frecv ),
    _nAcks( 0 ),
    _nBits( 0 )
{
  if( _kHz > 100 )
    _kHz = 200;
  else
    _kHz = 100;

  if( _scl < 0 || _scl > 31 )
    _scl = 0;

  if( _sda < 0 || _sda > 31 )
    _sda = 1;

  // GPIO byte and bit numbers to set and extract the selected I/O-pins
  // that carry the I2C SDA and SCL signal lines
  _sclByte = GPIO_BYTE_MAP[_scl/8];
  _sclBit  = 1 << (_scl & 0x7);
  _sdaByte = GPIO_BYTE_MAP[_sda/8];
  _sdaBit  = 1 << (_sda & 0x7);

  memset( _dir, 0, sizeof(_dir) );
  memset( _gpio, 0, sizeof(_gpio) );
  memset( _ackBits, 0, sizeof(_ackBits) );
  memset( _dataBits, 0, sizeof(_dataBits) );

  _fup.resetScaFrames();
}

// ----------------------------------------------------------------------------

I2cGpio::~I2cGpio()
{
}

// ----------------------------------------------------------------------------

void I2cGpio::init()
{
  // ###TODO: Should read and modify the current GBT-SCA channel enable mask
  // ...

  // Enable GBT-SCA GPIO (just in case)
  u8 data[2];
  data[0] = 0x00;
  data[1] = 1 << SCA_DEV_GPIO;
  _fup.addScaFrame( &_trid, SCA_DEV_CONFIG, 2, SCA_CONFIG_WR_B, data );
  // Twice... just in case
  _fup.addScaFrame( &_trid, SCA_DEV_CONFIG, 2, SCA_CONFIG_WR_B, data );

  setSclSda();
  setSclToOutput();
  setSdaToOutput();

  // To check the signal levels...
  //readInputs( true );

  execute();
}

// ----------------------------------------------------------------------------

void I2cGpio::i2cStart()
{
  // Generate I2C START condition
  setSclSda();
  addDelay();
  clearSda();
  addDelay();
  clearScl();
  addDelay();
}

// ----------------------------------------------------------------------------

void I2cGpio::i2cStop()
{
  // Generate I2C STOP condition
  clearSda();
  addDelay();
  setScl();
  addDelay();
  setSda();
  addDelay();
}

// ----------------------------------------------------------------------------

void I2cGpio::i2cWrite( u8 byte )
{
  // Send byte, MSB first
  for( int i=0; i<8; ++i )
    {
      if( byte & 0x80 )
	setSda();
      else
	clearSda();

      // Next bit
      byte <<= 1;

      setScl();
      addDelay();
      //if( !SCL_SET() ) I2C_Status |= I2C_SCL_STUCK_LOW;
      clearScl();
      addDelay();
    }
  // To receive the acknowledge, set SDA to input
  setSdaToInput();
  clearSda();

  // Get the acknowledge
  setScl();
  addDelay();
  readInputs( true ); //if( SDA_SET() ) I2C_Status |= I2C_ACK_MISSING;
  clearScl();
  addDelay();

  // Set SDA back to output
  setSdaToOutput();
}

// ----------------------------------------------------------------------------

void I2cGpio::i2cRead( bool last )
{
  // Set SDA to input
  setSdaToInput();

  // Going to receive byte, MSB first
  for( int i=0; i<8; ++i )
    {
      setScl();
      addDelay();
      //if( !SCL_SET() ) I2C_Status |= I2C_SCL_STUCK_LOW;

      // Next bit
      //byte <<= 1;

      readInputs( false ); //if( SDA_SET() ) ++byt;

      clearScl();
      addDelay();
    }

  // Get ready for acknowledge: is it the last byte to read ?
  if( last )
    setSda();
  else
    clearSda();

  // Set SDA to output
  setSdaToOutput();

  setScl();
  addDelay();
  clearScl();
  addDelay();
}

// ----------------------------------------------------------------------------

int I2cGpio::execute( bool debug )
{
  if( debug )
    {
      cout << "-> I2C-GPIO execute(), dump of SCA-frames:" << endl;
      _fup.dumpScaFrames();
    }

  int upload_sz = _fup.uploadScaFrames( _elinknr );
  _fup.resetScaFrames();
  return upload_sz;
}

// ----------------------------------------------------------------------------

void I2cGpio::getBits( u8 **data, int *nbits,
                       u8 **ack,  int *nacks )
{
  // Make sure the data has started arriving...
  usleep( 2000 );

  FlxDataChecker flxChecker;
  uint8_t *chunkdata = 0;
  uint32_t size;
  flxChecker.setReceiver( &_frecv );
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 2000, _elinknr ) )
    {
      if( size == 0 ) continue;
      if( size != 12 ) continue;

      // Extract chunks with SCA replies containing
      // our dedicated Transaction IDs

      // GBT-SCA reply parameters
      uint32_t trid = (uint32_t) chunkdata[2];
      //uint32_t chan = (uint32_t) chunkdata[3];
      //uint32_t err  = (uint32_t) chunkdata[4];
      //uint32_t len  = (uint32_t) chunkdata[5];

      // Extract the bit
      bool bitset = ((chunkdata[6+_sdaByte] & _sdaBit) != 0);

      // Is it an ACK bit or a DATA bit?
      if( trid == 0xFD /*&& chan == SCA_DEV_GPIO*/ )
        {
          // Reset byte
          if( (_nAcks & 7) == 0 )
            _ackBits[_nAcks/8] = 0;

          // Store the I2C ACK bit
          if( bitset )
            _ackBits[_nAcks/8] |= (1 << (_nAcks & 7));
          ++_nAcks;
        }
      else if( trid == 0xFE /*&& chan == SCA_DEV_GPIO*/ )
        {
          // Reset byte
          if( (_nBits & 7) == 0 )
            _dataBits[_nBits/8] = 0;

          // Store the I2C DATA bit (MSB first)
          _dataBits[_nBits/8] <<= 1;
          if( bitset )
            _dataBits[_nBits/8] |= 1;
          ++_nBits;
        }
    }

  *data  = _dataBits;
  *nbits = _nBits;
  *ack   = _ackBits;
  *nacks = _nAcks;
}

// ----------------------------------------------------------------------------

void I2cGpio::clearBits()
{
  _nAcks = 0; _nBits = 0;
}

// ----------------------------------------------------------------------------

void I2cGpio::displayReplies( std::string title )
{
  // Optionally have a title for the GBT-SCA replies displayed
  cout << endl;
  if( title.length() )
    cout << ">> " << title << " <<" << endl;

  // Go through the chunks of each block (skipping any TIMEOUT/NULL/OOB chunks)
  // displaying the data bytes, interpreting chunks as GBT-SCA replies,
  // counting the number of chunks
  FlxDataChecker flxChecker;
  int  timeout_us  = 200;
  bool display_sca = true;
  int  chunks      = 0;
  chunks += flxChecker.displayDataRecvd( &_frecv, timeout_us,
                                         _elinknr, display_sca );
}

// ----------------------------------------------------------------------------

void I2cGpio::setDir( uint32_t dir )
{
  // Preset GPIO direction bits you don't want to change
  mapInt2Bytes( dir, _dir );
}

// ----------------------------------------------------------------------------

void I2cGpio::setGpio( uint32_t gpio )
{
  // Preset GPIO output bits you don't want to change
  mapInt2Bytes( gpio, _gpio );
}

// ----------------------------------------------------------------------------

void I2cGpio::setScl()
{
  _gpio[_sclByte] |= _sclBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, _gpio );
}

// ----------------------------------------------------------------------------

void I2cGpio::setSda()
{
  _gpio[_sdaByte] |= _sdaBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, _gpio );
}

// ----------------------------------------------------------------------------

void I2cGpio::setSclSda()
{
  _gpio[_sclByte] |= _sclBit;
  _gpio[_sdaByte] |= _sdaBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, _gpio );
}

// ----------------------------------------------------------------------------

void I2cGpio::clearScl()
{
  _gpio[_sclByte] &= ~_sclBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, _gpio );
}

// ----------------------------------------------------------------------------

void I2cGpio::clearSda()
{
  _gpio[_sdaByte] &= ~_sdaBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, _gpio );
}

// ----------------------------------------------------------------------------

void I2cGpio::setSclToOutput()
{
  _dir[_sclByte] |= _sclBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, _dir );
}

// ----------------------------------------------------------------------------

void I2cGpio::setSdaToOutput()
{
  _dir[_sdaByte] |= _sdaBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, _dir );
}

// ----------------------------------------------------------------------------

void I2cGpio::setSdaToInput()
{
  _dir[_sdaByte] &= ~_sdaBit;
  _fup.addScaFrame( &_trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, _dir );
}

// ----------------------------------------------------------------------------

void I2cGpio::readInputs( bool ack )
{
  u8 data[2] = { 0, 0 };
  int recv_trid = 0xFE; // To be able to filter out replies of interest
  if( ack )
    recv_trid = 0xFD;   // For the ACK bit use a different TRID
  _fup.addScaFrame( &recv_trid, SCA_DEV_GPIO, 2, SCA_GPIO_RD_IN, data );
}

// ----------------------------------------------------------------------------

void I2cGpio::addDelay()
{
  // Delay count: 10 per microsecond
  // (2 bits per 25ns, or 4x25ns=0.1us per byte => 10 bytes/us)
  //_fup.addScaDelay( (10*1000)/(2*_kHz) ); // Half a bit time
  if( _kHz == 100 )
    _fup.addScaDelay( (4*1000)/(2*_kHz) ); // Half a bit time
}

// ----------------------------------------------------------------------------

void I2cGpio::mapInt2Bytes( uint32_t i, u8 *bytes )
{
  // Map integer to bytes in the order used in GBT-SCA GPIO messages
  bytes[2] = (u8) ((i >>  0) & 0xFF);
  bytes[3] = (u8) ((i >>  8) & 0xFF);
  bytes[0] = (u8) ((i >> 16) & 0xFF);
  bytes[1] = (u8) ((i >> 24) & 0xFF);
}

// ----------------------------------------------------------------------------
