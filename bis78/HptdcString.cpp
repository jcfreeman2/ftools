#include <iostream>
using namespace std;

#include "HptdcString.h"

//-----------------------------------------------------------------------------

const JTAG_VAL_DESCR HPTDC_SETUP_DESCR[] =
{
    { "HPTDC_TEST_SELECT_I              ", HPTDC_TEST_SELECT_I,                 HPTDC_TEST_SELECT_BITS },
    { "HPTDC_ENABLE_ERROR_MARK          ", HPTDC_ENABLE_ERROR_MARK,             1 },
    { "HPTDC_ENABLE_ERROR_BYPASS        ", HPTDC_ENABLE_ERROR_BYPASS,           1 },
    { "HPTDC_ENABLE_ERROR_I             ", HPTDC_ENABLE_ERROR_I,                HPTDC_ENABLE_ERROR_BITS },
    { "HPTDC_RO_SCYCLE_SPEED_I          ", HPTDC_RO_SCYCLE_SPEED_I,             HPTDC_RO_SCYCLE_SPEED_BITS },
    { "HPTDC_SERIAL_DELAY_I             ", HPTDC_SERIAL_DELAY_I,                HPTDC_SERIAL_DELAY_BITS },
    { "HPTDC_STROBE_SELECT_I            ", HPTDC_STROBE_SELECT_I,               HPTDC_STROBE_SELECT_BITS },
    { "HPTDC_READOUT_SPEED_SELECT       ", HPTDC_READOUT_SPEED_SELECT,          1 },
    { "HPTDC_TOKEN_DELAY_I              ", HPTDC_TOKEN_DELAY_I,                 HPTDC_TOKEN_DELAY_BITS },
    { "HPTDC_ENABLE_LOCAL_TRAILER       ", HPTDC_ENABLE_LOCAL_TRAILER,          1 },
    { "HPTDC_ENABLE_LOCAL_HEADER        ", HPTDC_ENABLE_LOCAL_HEADER,           1 },
    { "HPTDC_ENABLE_GLOBAL_TRAILER      ", HPTDC_ENABLE_GLOBAL_TRAILER,         1 },
    { "HPTDC_ENABLE_GLOBAL_HEADER       ", HPTDC_ENABLE_GLOBAL_HEADER,          1 },
    { "HPTDC_KEEP_TOKEN                 ", HPTDC_KEEP_TOKEN,                    1 },
    { "HPTDC_MASTER                     ", HPTDC_MASTER,                        1 },
    { "HPTDC_ENABLE_BYTEWISE            ", HPTDC_ENABLE_BYTEWISE,               1 },
    { "HPTDC_ENABLE_SERIAL              ", HPTDC_ENABLE_SERIAL,                 1 },
    { "HPTDC_ENABLE_JTAG_READOUT        ", HPTDC_ENABLE_JTAG_READOUT,           1 },
    { "HPTDC_TDC_ID_I                   ", HPTDC_TDC_ID_I,                      HPTDC_TDC_ID_BITS},
    { "HPTDC_SELECT_BYPASS_INPUTS       ", HPTDC_SELECT_BYPASS_INPUTS,          1 },
    { "HPTDC_READOUT_FIFO_SIZE_I        ", HPTDC_READOUT_FIFO_SIZE_I,           HPTDC_READOUT_FIFO_SIZE_BITS },
    { "HPTDC_REJECT_COUNT_OFFSET_I      ", HPTDC_REJECT_COUNT_OFFSET_I,         HPTDC_REJECT_COUNT_OFFSET_BITS },
    { "HPTDC_SEARCH_WINDOW_I            ", HPTDC_SEARCH_WINDOW_I,               HPTDC_SEARCH_WINDOW_BITS},
    { "HPTDC_MATCH_WINDOW_I             ", HPTDC_MATCH_WINDOW_I,                HPTDC_MATCH_WINDOW_BITS},
    { "HPTDC_LEADING_RESOLUTION_I       ", HPTDC_LEADING_RESOLUTION_I,          HPTDC_LEADING_RESOLUTION_BITS },
    { "HPTDC_FIXED_PATTERN_I            ", HPTDC_FIXED_PATTERN_I,               HPTDC_FIXED_PATTERN_BITS },
    { "HPTDC_ENABLE_FIXED_PATTERN       ", HPTDC_ENABLE_FIXED_PATTERN,          1 },
    { "HPTDC_MAX_EVENT_SIZE_I           ", HPTDC_MAX_EVENT_SIZE_I,              HPTDC_MAX_EVENT_SIZE_BITS },
    { "HPTDC_REJECT_READOUT_FIFO_FULL   ", HPTDC_REJECT_READOUT_FIFO_FULL,      1 },
    { "HPTDC_ENABLE_READOUT_OCCUPANCY   ", HPTDC_ENABLE_READOUT_OCCUPANCY,      1 },
    { "HPTDC_ENABLE_READOUT_SEPARATOR   ", HPTDC_ENABLE_READOUT_SEPARATOR,      1 },
    { "HPTDC_ENABLE_OVERFLOW_DETECT     ", HPTDC_ENABLE_OVERFLOW_DETECT,        1 },
    { "HPTDC_ENABLE_RELATIVE            ", HPTDC_ENABLE_RELATIVE,               1 },
    { "HPTDC_ENABLE_AUTOMATIC_REJECT    ", HPTDC_ENABLE_AUTOMATIC_REJECT,       1 },
    { "HPTDC_EVENT_COUNT_OFFSET_I       ", HPTDC_EVENT_COUNT_OFFSET_I,          HPTDC_EVENT_COUNT_OFFSET_BITS },
    { "HPTDC_TRIGGER_COUNT_OFFSET_I     ", HPTDC_TRIGGER_COUNT_OFFSET_I,        HPTDC_TRIGGER_COUNT_OFFSET_BITS },
    { "HPTDC_EN_SET_CNT_ON_BUNCH_RESET  ", HPTDC_EN_SET_CNT_ON_BUNCH_RESET,     1 },
    { "HPTDC_ENABLE_MASTER_RESET_CODE   ", HPTDC_ENABLE_MASTER_RESET_CODE,      1 },
    { "HPTDC_EN_MRESET_ON_EVENT_RESET   ", HPTDC_EN_MRESET_ON_EVENT_RESET,      1 },
    { "HPTDC_EN_RES_CHBUF_WHEN_SEPARATOR", HPTDC_EN_RES_CHBUF_WHEN_SEPARATOR,   1 },
    { "HPTDC_EN_SEPARATOR_ON_EVENT_RESET", HPTDC_EN_SEPARATOR_ON_EVENT_RESET,   1 },
    { "HPTDC_EN_SEPARATOR_ON_BUNCH_RESET", HPTDC_EN_SEPARATOR_ON_BUNCH_RESET,   1 },
    { "HPTDC_ENABLE_DIRECT_EVENT_RESET  ", HPTDC_ENABLE_DIRECT_EVENT_RESET,     1 },
    { "HPTDC_ENABLE_DIRECT_BUNCH_RESET  ", HPTDC_ENABLE_DIRECT_BUNCH_RESET,     1 },
    { "HPTDC_ENABLE_DIRECT_TRIGGER      ", HPTDC_ENABLE_DIRECT_TRIGGER,         1 },
    { "HPTDC_OFFSET31_I                 ", HPTDC_OFFSET31_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET30_I                 ", HPTDC_OFFSET30_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET29_I                 ", HPTDC_OFFSET29_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET28_I                 ", HPTDC_OFFSET28_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET27_I                 ", HPTDC_OFFSET27_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET26_I                 ", HPTDC_OFFSET26_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET25_I                 ", HPTDC_OFFSET25_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET24_I                 ", HPTDC_OFFSET24_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET23_I                 ", HPTDC_OFFSET23_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET22_I                 ", HPTDC_OFFSET22_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET21_I                 ", HPTDC_OFFSET21_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET20_I                 ", HPTDC_OFFSET20_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET19_I                 ", HPTDC_OFFSET19_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET18_I                 ", HPTDC_OFFSET18_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET17_I                 ", HPTDC_OFFSET17_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET16_I                 ", HPTDC_OFFSET16_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET15_I                 ", HPTDC_OFFSET15_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET14_I                 ", HPTDC_OFFSET14_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET13_I                 ", HPTDC_OFFSET13_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET12_I                 ", HPTDC_OFFSET12_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET11_I                 ", HPTDC_OFFSET11_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET10_I                 ", HPTDC_OFFSET10_I,                    HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET9_I                  ", HPTDC_OFFSET9_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET8_I                  ", HPTDC_OFFSET8_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET7_I                  ", HPTDC_OFFSET7_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET6_I                  ", HPTDC_OFFSET6_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET5_I                  ", HPTDC_OFFSET5_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET4_I                  ", HPTDC_OFFSET4_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET3_I                  ", HPTDC_OFFSET3_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET2_I                  ", HPTDC_OFFSET2_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET1_I                  ", HPTDC_OFFSET1_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_OFFSET0_I                  ", HPTDC_OFFSET0_I,                     HPTDC_OFFSET_BITS },
    { "HPTDC_COARSE_COUNT_OFFSET_I      ", HPTDC_COARSE_COUNT_OFFSET_I,         HPTDC_COARSE_COUNT_OFFSET_BITS },
    { "HPTDC_DLL_TAP_ADJUST31_I         ", HPTDC_DLL_TAP_ADJUST31_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST30_I         ", HPTDC_DLL_TAP_ADJUST30_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST29_I         ", HPTDC_DLL_TAP_ADJUST29_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST28_I         ", HPTDC_DLL_TAP_ADJUST28_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST27_I         ", HPTDC_DLL_TAP_ADJUST27_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST26_I         ", HPTDC_DLL_TAP_ADJUST26_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST25_I         ", HPTDC_DLL_TAP_ADJUST25_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST24_I         ", HPTDC_DLL_TAP_ADJUST24_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST23_I         ", HPTDC_DLL_TAP_ADJUST23_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST22_I         ", HPTDC_DLL_TAP_ADJUST22_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST21_I         ", HPTDC_DLL_TAP_ADJUST21_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST20_I         ", HPTDC_DLL_TAP_ADJUST20_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST19_I         ", HPTDC_DLL_TAP_ADJUST19_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST18_I         ", HPTDC_DLL_TAP_ADJUST18_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST17_I         ", HPTDC_DLL_TAP_ADJUST17_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST16_I         ", HPTDC_DLL_TAP_ADJUST16_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST15_I         ", HPTDC_DLL_TAP_ADJUST15_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST14_I         ", HPTDC_DLL_TAP_ADJUST14_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST13_I         ", HPTDC_DLL_TAP_ADJUST13_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST12_I         ", HPTDC_DLL_TAP_ADJUST12_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST11_I         ", HPTDC_DLL_TAP_ADJUST11_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST10_I         ", HPTDC_DLL_TAP_ADJUST10_I,            HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST9_I          ", HPTDC_DLL_TAP_ADJUST9_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST8_I          ", HPTDC_DLL_TAP_ADJUST8_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST7_I          ", HPTDC_DLL_TAP_ADJUST7_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST6_I          ", HPTDC_DLL_TAP_ADJUST6_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST5_I          ", HPTDC_DLL_TAP_ADJUST5_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST4_I          ", HPTDC_DLL_TAP_ADJUST4_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST3_I          ", HPTDC_DLL_TAP_ADJUST3_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST2_I          ", HPTDC_DLL_TAP_ADJUST2_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST1_I          ", HPTDC_DLL_TAP_ADJUST1_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_DLL_TAP_ADJUST0_I          ", HPTDC_DLL_TAP_ADJUST0_I,             HPTDC_DLL_TAP_ADJUST_BITS },
    { "HPTDC_RC_ADJUST_I                ", HPTDC_RC_ADJUST_I,                   HPTDC_RC_ADJUST_BITS },
    { "HPTDC_NOT_USED_I                 ", HPTDC_NOT_USED_I,                    HPTDC_NOT_USED_BITS },
    { "HPTDC_LOW_POWER_MODE             ", HPTDC_LOW_POWER_MODE,                1 },
    { "HPTDC_WIDTH_SELECT_I             ", HPTDC_WIDTH_SELECT_I,                HPTDC_WIDTH_SELECT_BITS },
    { "HPTDC_VERNIER_OFFSET_I           ", HPTDC_VERNIER_OFFSET_I,              HPTDC_VERNIER_OFFSET_BITS },
    { "HPTDC_DLL_CONTROL_I              ", HPTDC_DLL_CONTROL_I,                 HPTDC_DLL_CONTROL_BITS },
    { "HPTDC_DEAD_TIME_I                ", HPTDC_DEAD_TIME_I,                   HPTDC_DEAD_TIME_BITS },
    { "HPTDC_TEST_INVERT                ", HPTDC_TEST_INVERT,                   1 },
    { "HPTDC_TEST_MODE                  ", HPTDC_TEST_MODE,                     1 },
    { "HPTDC_ENABLE_TRAILING            ", HPTDC_ENABLE_TRAILING,               1 },
    { "HPTDC_ENABLE_LEADING             ", HPTDC_ENABLE_LEADING,                1 },
    { "HPTDC_MODE_RC_COMPRESSION        ", HPTDC_MODE_RC_COMPRESSION,           1 },
    { "HPTDC_MODE_RC                    ", HPTDC_MODE_RC,                       1 },
    { "HPTDC_DLL_MODE_I                 ", HPTDC_DLL_MODE_I,                    HPTDC_DLL_MODE_BITS },
    { "HPTDC_PLL_CONTROL_I              ", HPTDC_PLL_CONTROL_I,                 HPTDC_PLL_CONTROL_BITS },
    { "HPTDC_SERIAL_CLOCK_DELAY_I       ", HPTDC_SERIAL_CLOCK_DELAY_I,          HPTDC_SERIAL_CLOCK_DELAY_BITS },
    { "HPTDC_IO_CLOCK_DELAY_I           ", HPTDC_IO_CLOCK_DELAY_I,              HPTDC_IO_CLOCK_DELAY_BITS },
    { "HPTDC_CORE_CLOCK_DELAY_I         ", HPTDC_CORE_CLOCK_DELAY_I,            HPTDC_CORE_CLOCK_DELAY_BITS },
    { "HPTDC_DLL_CLOCK_DELAY_I          ", HPTDC_DLL_CLOCK_DELAY_I,             HPTDC_DLL_CLOCK_DELAY_BITS },
    { "HPTDC_SERIAL_CLOCK_SOURCE_I      ", HPTDC_SERIAL_CLOCK_SOURCE_I,         HPTDC_SERIAL_CLOCK_SOURCE_BITS },
    { "HPTDC_IO_CLOCK_SOURCE_I          ", HPTDC_IO_CLOCK_SOURCE_I,             HPTDC_IO_CLOCK_SOURCE_BITS },
    { "HPTDC_CORE_CLOCK_SOURCE_I        ", HPTDC_CORE_CLOCK_SOURCE_I,           HPTDC_CORE_CLOCK_SOURCE_BITS },
    { "HPTDC_DLL_CLOCK_SOURCE_I         ", HPTDC_DLL_CLOCK_SOURCE_I,            HPTDC_DLL_CLOCK_SOURCE_BITS },
    { "HPTDC_ROLL_OVER_I                ", HPTDC_ROLL_OVER_I,                   HPTDC_ROLL_OVER_BITS },
    { "HPTDC_ENABLE_MATCHING            ", HPTDC_ENABLE_MATCHING,               1 },
    { "HPTDC_ENABLE_PAIR                ", HPTDC_ENABLE_PAIR,                   1 },
    { "HPTDC_ENABLE_TTL_SERIAL          ", HPTDC_ENABLE_TTL_SERIAL,             1 },
    { "HPTDC_ENABLE_TTL_CONTROL         ", HPTDC_ENABLE_TTL_CONTROL,            1 },
    { "HPTDC_ENABLE_TTL_RESET           ", HPTDC_ENABLE_TTL_RESET,              1 },
    { "HPTDC_ENABLE_TTL_CLOCK           ", HPTDC_ENABLE_TTL_CLOCK,              1 },
    { "HPTDC_ENABLE_TTL_HIT             ", HPTDC_ENABLE_TTL_HIT,                1 },
    { "HPTDC_SETUP_PARITY               ", HPTDC_SETUP_PARITY,                  1 }
};

//-----------------------------------------------------------------------------

const JTAG_VAL_DESCR HPTDC_CONTROL_DESCR[] = {
  { "HPTDC_ENABLE_PATTERN_I   ", HPTDC_ENABLE_PATTERN_I,  HPTDC_ENABLE_PATTERN_BITS },
  { "HPTDC_GLOBAL_RESET       ", HPTDC_GLOBAL_RESET,      1 },
  { "HPTDC_ENABLE_CHANNEL_I   ", HPTDC_ENABLE_CHANNEL_I,  HPTDC_ENABLE_CHANNEL_BITS },
  { "HPTDC_DLL_RESET          ", HPTDC_DLL_RESET,         1 },
  { "HPTDC_PLL_RESET          ", HPTDC_PLL_RESET,         1 },
  { "HPTDC_CONTROL_PARITY     ", HPTDC_CONTROL_PARITY,    1 },
};

//-----------------------------------------------------------------------------

const JTAG_VAL_DESCR HPTDC_STATUS_DESCR[] = {
  { "HPTDC_ERROR_I            ", HPTDC_ERROR_I,           HPTDC_ERROR_BITS },
  { "HPTDC_HAVE_TOKEN         ", HPTDC_HAVE_TOKEN,        1 },
  { "HPTDC_ROFIFO_OCCUPANCY_I ", HPTDC_ROFIFO_OCCUPANCY_I,HPTDC_ROFIFO_OCCUPANCY_BITS },
  { "HPTDC_ROFIFO_FULL        ", HPTDC_ROFIFO_FULL,       1 },
  { "HPTDC_ROFIFO_EMPTY       ", HPTDC_ROFIFO_EMPTY,      1 },
  { "HPTDC_G3_L1_OCCUPANCY_I  ", HPTDC_G3_L1_OCCUPANCY_I, HPTDC_G_L1_OCCUPANCY_BITS },
  { "HPTDC_G2_L1_OCCUPANCY_I  ", HPTDC_G2_L1_OCCUPANCY_I, HPTDC_G_L1_OCCUPANCY_BITS },
  { "HPTDC_G1_L1_OCCUPANCY_I  ", HPTDC_G1_L1_OCCUPANCY_I, HPTDC_G_L1_OCCUPANCY_BITS },
  { "HPTDC_G0_L1_OCCUPANCY_I  ", HPTDC_G0_L1_OCCUPANCY_I, HPTDC_G_L1_OCCUPANCY_BITS },
  { "HPTDC_TFIFO_OCCUPANCY_I  ", HPTDC_TFIFO_OCCUPANCY_I, HPTDC_TFIFO_OCCUPANCY_BITS },
  { "HPTDC_TFIFO_FULL         ", HPTDC_TFIFO_FULL,        1 },
  { "HPTDC_TFIFO_EMPTY        ", HPTDC_TFIFO_EMPTY,       1 },
  { "HPTDC_DLL_LOCK           ", HPTDC_DLL_LOCK,          1 },
  { "HPTDC_NOT_USED           ", HPTDC_NOT_USED,          1 }
};

//-----------------------------------------------------------------------------

void HptdcString::setParity()
{
  // Count 1s in the string
  int bit1count = 0;
  for( int i=0; i<this->length()-1; ++i )
    if( this->bits( i, 1 ) )
      ++bit1count;

  // Last bit in string becomes the parity bit
  this->setBits( this->length()-1, 1, bit1count & 1 );
}

//-----------------------------------------------------------------------------

int HptdcString::clockSpeed()
{
  return this->bits( HPTDC_READOUT_SPEED_SELECT, 1 );
}

//-----------------------------------------------------------------------------

int HptdcString::hardErrBits()
{
  return this->bits( HPTDC_ERROR_I, HPTDC_ERROR_BITS );
}

//-----------------------------------------------------------------------------

int HptdcString::instruction( int instr_id )
{
  // Translate instruction 'ID' to JTAG instruction bits
  switch( instr_id )
    {
    case BYPASS_INSTR:
      return 0x0F;
    case IDCODE_INSTR:
      return 0x11;
    case HPTDC_SETUP_INSTR:
      return 0x18;
    case HPTDC_STATUS_INSTR:
      return 0x0A;
    default:
      return 0x09;
    }
  return 0x00;
}

//-----------------------------------------------------------------------------

void HptdcString::show( bool string_only )
{
  cout << "==> HPTDC string ";
  int len = length();
  if( len == HPTDC_SETUP_STRING_SIZE )
    cout << "Setup:";
  else if( len == HPTDC_CNTRL_STRING_SIZE )
    cout << "Cntrl:";
  else if( len == HPTDC_STATUS_STRING_SIZE )
    cout << "Status:";
  else
    cout << "????:";
  cout << endl;

  JtagString::show();
  if( string_only ) return;

  cout.fill( ' ' );

  int max_i;
  JTAG_VAL_DESCR const *val_descr;
  if( len == HPTDC_SETUP_STRING_SIZE )
    {
      max_i = sizeof( HPTDC_SETUP_DESCR ) / sizeof( JTAG_VAL_DESCR );
      val_descr = HPTDC_SETUP_DESCR;
    }
  else if( len == HPTDC_CNTRL_STRING_SIZE )
    {
      max_i = sizeof( HPTDC_CONTROL_DESCR ) / sizeof( JTAG_VAL_DESCR );
      val_descr = HPTDC_CONTROL_DESCR;
    }
  else if( len == HPTDC_STATUS_STRING_SIZE )
    {
      max_i = sizeof( HPTDC_STATUS_DESCR ) / sizeof( JTAG_VAL_DESCR );
      val_descr = HPTDC_STATUS_DESCR;
    }
  else
    return;

  JTAG_VAL_DESCR const *descr;
  for( int i=0; i<max_i; ++i )
    {
      descr = &val_descr[i];
      int val = this->bits( descr->index, descr->no_of_bits );
      cout << descr->name << " (" << dec;
      cout.width( 3 );
      cout << descr->index << ",";
      cout.width( 2 );
      cout << descr->no_of_bits << "): "
           << hex << val << dec << " (" << val << ")" << endl;
    }
}

//-----------------------------------------------------------------------------
