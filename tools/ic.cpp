#include <iostream>
#include <iomanip>
using namespace std;

#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxParser.h"
#include "ic.h"

// ----------------------------------------------------------------------------

bool writeIcRegs( FlxUpload *fup, FlxReceiver *frecvr,
                  int linknr, int i2c_addr,
                  int reg_addr, int nbytes, uint8_t *reg_vals,
                  bool use_ec, bool lpgbt_v1,
                  bool debug, bool display )
{
  if( !fup->writeIcChannel( linknr, i2c_addr, reg_addr,
                            nbytes, reg_vals, use_ec, lpgbt_v1 ) )
    cout << "### writeIcRegs(0x" << hex << reg_addr << dec
         << ",nbytes=" << nbytes << "): "
         << fup->errorString() << endl;

  // Receive the reply (containing nbytes bytes) and display
  bool reply_received = receiveIcReply( frecvr, linknr,
                                        reg_addr, nbytes, reg_vals,
                                        use_ec, lpgbt_v1, debug, display );
  if( !reply_received )
    cout << "### Reply not received (addr=0x" << hex << reg_addr << ")" << endl;

  if( debug ) return true;
  return reply_received;
}

// ----------------------------------------------------------------------------

bool readIcRegs( FlxUpload *fup, FlxReceiver *frecvr,
                 int linknr, int i2c_addr,
                 int reg_addr, int nbytes, uint8_t *reg_vals,
                 bool use_ec, bool lpgbt_v1,
                 bool debug, bool display )
{
  if( !fup->readIcChannel( linknr, i2c_addr, reg_addr,
                           nbytes, use_ec, lpgbt_v1 ) )
    cout << "### readIcRegs(0x" << hex << reg_addr << dec
         << ",nbytes=" << nbytes << "): "
         << fup->errorString() << endl;

  // Receive the reply (containing nbytes bytes) and display
  bool reply_received = receiveIcReply( frecvr, linknr,
                                        reg_addr, nbytes, reg_vals,
                                        use_ec, lpgbt_v1, debug, display );
  if( !reply_received )
    cout << "### Reply not received (addr=0x" << hex << reg_addr << ")" << endl;

  if( debug ) return true;
  return reply_received;
}

// ----------------------------------------------------------------------------

bool receiveIcReply( FlxReceiver *frecvr, int linknr,
                     int reg_addr, int nbytes, uint8_t *reg_vals,
                     bool use_ec, bool lpgbt_v1,
                     bool debug, bool display )
{
  // Function expects to read a single reply
  // with one or more register values returned

  // The IC (or EC) elink number from the original GBT firmware
  // (maintained within the software; will be mapped to the real number
  //  for Phase2 firmwares, internally)
  int egroup = 7;
  int epath  = 6;
  if( use_ec )
    epath = 7;
  int elinknr = ((linknr << BLOCK_GBT_SHIFT) |
                 (egroup << BLOCK_EGROUP_SHIFT) |
                 (epath  << BLOCK_EPATH_SHIFT));

  FlxParser fparser;
  fparser.setReceiver( frecvr );

  uint8_t *chunkdata = 0;
  uint32_t size, chunknr = 1;
  bool     reply_received = false;
  uint32_t parity_start_index = (lpgbt_v1 ? 0 : 2);
  int      offset = (lpgbt_v1 ? 0 : 1);
  while( fparser.nextChunkRecvd( &chunkdata, &size, 8000, elinknr ) )
    {
      if( size == 0 ) continue;

      // Process chunk from the IC (or EC) channel
      uint32_t frame_len = size;
      uint8_t *frame     = chunkdata;
      // Check reply frame CRC
      // (do not include I2C-address and zero-byte, in case of lpGBTv0)
      if( frame_len > 2 )
        {
          if( debug )
            cout << "Chunk " << chunknr << ": ";
          uint8_t parity = 0;
          for( uint32_t i=parity_start_index; i<frame_len-1; ++i )
            parity ^= frame[i];
          if( parity != frame[frame_len-1] )
            {
              cout << hex << setfill('0')
                   << "### Parity "
                   << setw(2) << (uint32_t) frame[frame_len-1]
                   << ", expected "
                   << setw(2) << (uint32_t) parity
                   << dec << setfill(' ');
              if( !debug ) cout << endl;
              //continue;
            }
          else
            {
              if( debug )
                cout << "parity OK";
            }

          if( debug )
            {
              if( frame[0] & 1 )
                cout << ",  READ:";
              else
                cout << ", WRITE:";
              //cout << endl;
            }
        }

      if( debug )
        {
          // Display IC frame bytes
          cout << hex << uppercase << setfill('0');
          for( uint32_t i=0; i<frame_len; ++i )
            cout << " " << setw(2) << (uint32_t) frame[i];
          cout << endl << dec << setfill(' ');
        }

      // Display returned register values in IC frame
      // (I2C-address, zero-byte (lpGBTv0), command, #bytes, #address, parity:
      //  7 (v1) or 8 (v0) bytes in total) (###TBD: define v0 and v1 structs?)
      int data_len = frame_len - (offset+1+1+2+2+1);
      if( data_len > 0 )
        {
          uint32_t len  = ((uint32_t) frame[offset+2] +
                           ((uint32_t) frame[offset+3]<<8));
          uint32_t addr = ((uint32_t) frame[offset+4] +
                           ((uint32_t) frame[offset+5]<<8));

          cout << hex << uppercase << setfill('0');
          if( len == (uint32_t) nbytes && addr == (uint32_t) reg_addr )
            {
              if( display )
                {
                  cout << "Address 0x" << setw(3) << addr << ":";
                  for( int i=0; i<nbytes; ++i )
                    {
                      if( i > 0 && (i & 0xF) == 0 )
                        cout << endl << "              ";
                      cout << " 0x" << setw(2) << (uint32_t) frame[offset+6+i];
                    }
                  cout << endl;
                }
              reply_received = true;
            }
          else
            {
              cout << "### Unexpected length or address: " << dec << len
                   << hex << ", 0x" << setw(3) << addr << endl;
            }
          cout << dec << setfill(' ');

          // Extract the register values to return
          for( int i=0; i<nbytes; ++i )
            reg_vals[i] = frame[offset+6+i];
        }
      ++chunknr;
    }

  return reply_received;
}

// ----------------------------------------------------------------------------
