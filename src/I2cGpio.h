#ifndef I2CGPIO_H
#define I2CGPIO_H

#include "FlxUpload.h"
#include "FlxReceiver.h"

// ----------------------------------------------------------------------------

// Bits and bit masks
#define I2C_READ                    0x01
#define I2C_WRITE                   0x00
#define I2C_ADDR_MASK               0xFE

// Error ID bits
#define I2C_SCL_STUCK_LOW           0x01
#define I2C_SDA_STUCK_LOW           0x02
#define I2C_ACK_MISSING             0x04

// ----------------------------------------------------------------------------

class I2cGpio
{
 public:
  I2cGpio( FlxUpload &fup, FlxReceiver &frecv,
           int elinknr, int scl, int sda, int khz = 100 );
  ~I2cGpio();

 public:
  // I2C operations
  void init    ( );
  void i2cStart( );
  void i2cStop ( );
  void i2cWrite( u8 byte );
  void i2cRead ( bool last );
  int  execute ( bool debug = false );

  void getBits ( u8 **data, int *nbits,
                 u8 **ack,  int *nacks );
  void clearBits( );

  void displayReplies( std::string title = std::string() );

  void setDir ( uint32_t dir );
  void setGpio( uint32_t gpio );

 private:
  void setScl        ( );
  void setSda        ( );
  void setSclSda     ( );
  void clearScl      ( );
  void clearSda      ( );
  void setSclToOutput( );
  void setSdaToOutput( );
  void setSdaToInput ( );
  void readInputs    ( bool ack );

  void addDelay      ( );

  void mapInt2Bytes  ( uint32_t i, u8 *bytes );

 private:
  // For I2C via GBT-SCA GPIO
  int  _scl, _sclByte, _sclBit; // I/O bit for SCL
  int  _sda, _sdaByte, _sdaBit; // I/O bit for SDA
  int  _kHz;                    // I2C speed setting
  u8   _dir[4];                 // Other bits' I/O direction bits to maintain
  u8   _gpio[4];                // Other bits' I/O state to maintain

  // For GBT-SCA via FLX
  int        _elinknr;
  int        _trid;
  FlxUpload   &_fup;
  FlxReceiver &_frecv;

  // Space for received I2C ACK and data bits
  int _nAcks, _nBits;
  u8 _ackBits[64];
  u8 _dataBits[256];
};

// ----------------------------------------------------------------------------
#endif // I2CGPIO_H
