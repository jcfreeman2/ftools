#ifndef FLXTRAILERFORMAT_H
#define FLXTRAILERFORMAT_H

#include <cstdint>

class FlxTrailerFormat
{
 public:
  FlxTrailerFormat( )
    : nbytes     ( TRAILER_BYTES ),
      lengthMsk  ( TRAILER_LENGTH_MSK ),
      busyMsk    ( 0x0 ),
      crcerrMsk  ( TRAILER_CRCERR_MSK ),
      errorMsk   ( TRAILER_ERROR_MSK ),
      truncMsk   ( TRAILER_TRUNC_MSK ),
      typeMsk    ( TRAILER_TYPE_MSK ),
      crcerrShift( TRAILER_CRCERR_SHIFT ),
      errorShift ( TRAILER_ERROR_SHIFT ),
      truncShift ( TRAILER_TRUNC_SHIFT ),
      typeShift  ( TRAILER_TYPE_SHIFT ),
      blockId    ( BLOCK_ID )
  { }
  ~FlxTrailerFormat( ) { }

  void setTrailerIs32Bit( bool b )
  {
    if( b ) {
      nbytes      = TRAILER_BYTES_32B;
      lengthMsk   = TRAILER_LENGTH_MSK_32B;
      busyMsk     = TRAILER_BUSY_MSK_32B;
      crcerrMsk   = TRAILER_CRCERR_MSK_32B;
      errorMsk    = TRAILER_ERROR_MSK_32B;
      truncMsk    = TRAILER_TRUNC_MSK_32B;
      typeMsk     = TRAILER_TYPE_MSK_32B;
      crcerrShift = TRAILER_CRCERR_SHIFT_32B;
      errorShift  = TRAILER_ERROR_SHIFT_32B;
      truncShift  = TRAILER_TRUNC_SHIFT_32B;
      typeShift   = TRAILER_TYPE_SHIFT_32B;
      blockId     = BLOCK_ID_32B;
    }
    else {
      nbytes      = TRAILER_BYTES;
      lengthMsk   = TRAILER_LENGTH_MSK;
      busyMsk     = 0x0;
      crcerrMsk   = TRAILER_CRCERR_MSK;
      errorMsk    = TRAILER_ERROR_MSK;
      truncMsk    = TRAILER_TRUNC_MSK;
      typeMsk     = TRAILER_TYPE_MSK;
      crcerrShift = TRAILER_CRCERR_SHIFT;
      errorShift  = TRAILER_ERROR_SHIFT;
      truncShift  = TRAILER_TRUNC_SHIFT;
      typeShift   = TRAILER_TYPE_SHIFT;
      blockId     = BLOCK_ID;
    }
  }

  uint32_t nbytes;
  uint32_t lengthMsk;
  uint32_t busyMsk;
  uint32_t crcerrMsk;
  uint32_t errorMsk;
  uint32_t truncMsk;
  uint32_t typeMsk;
  uint32_t crcerrShift;
  uint32_t errorShift;
  uint32_t truncShift;
  uint32_t typeShift;
  uint32_t blockId;
};

#endif // FLXTRAILERFORMAT_H
