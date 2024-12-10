#ifndef _HPTDCSTRING_H_
#define _HPTDCSTRING_H_

#include "JtagString.h"

// Bit string sizes
#define HPTDC_INSTR_SIZE              5
#define HPTDC_SETUP_STRING_SIZE       647
#define HPTDC_CNTRL_STRING_SIZE       40
#define HPTDC_STATUS_STRING_SIZE      62

// General JTAG instruction IDs
// (to be translated to actual instruction bits for the current device type)
#define BYPASS_INSTR                  1
#define PROGRAM_INSTR                 2
#define STATUS_INSTR                  3
#define IDCODE_INSTR                  4

// HPTDC-specific JTAG 'instruction' IDs
// (to be translated to actual HPTDC instruction bits)
#define HPTDC_CONTROL_1_INSTR         0x80
#define HPTDC_CONTROL_2_INSTR         0x81
#define HPTDC_CONTROL_3_INSTR         0x82
#define HPTDC_CONTROL_4_INSTR         0x83
#define HPTDC_CONTROL_5_INSTR         0x84
#define HPTDC_SETUP_INSTR             0x85
#define HPTDC_STATUS_INSTR            0x86

// HPTDC-specific JTAG 'action' IDs
// (TODO: to replace 'instructions' above)
#define HPTDC_ALL_RESETS_ON_INSTR     0x80
#define HPTDC_DLL_RESET_ON_INSTR      0x81
#define HPTDC_ALL_RESETS_OFF_INSTR    0x82
#define HPTDC_GLOBAL_RESET_ON_INSTR   0x83
#define HPTDC_PATTERN_5_INSTR         0x84
#define HPTDC_PLL_DLL_RESET_ON_INSTR  0x90

//-----------------------------------------------------------------------------

class HptdcString : public JtagString
{
 public:
  HptdcString( int nbits, const unsigned char *str ) : JtagString( nbits, str ) {}
  HptdcString( int nbits ) : JtagString( nbits ) {}
  ~HptdcString() {}

  void setParity();
  int  clockSpeed();
  int  hardErrBits();

  static int instruction( int instr_id );

  void show( bool string_only = false );
};

//-----------------------------------------------------------------------------

// SETUP string
#define HPTDC_TEST_SELECT_I                 0
#define HPTDC_TEST_SELECT_BITS              4
#define HPTDC_ENABLE_ERROR_MARK             4
#define HPTDC_ENABLE_ERROR_BYPASS           5
#define HPTDC_ENABLE_ERROR_I                6
#define HPTDC_ENABLE_ERROR_BITS             11
#define HPTDC_RO_SCYCLE_SPEED_I             17
#define HPTDC_RO_SCYCLE_SPEED_BITS          3
#define HPTDC_SERIAL_DELAY_I                20
#define HPTDC_SERIAL_DELAY_BITS             4
#define HPTDC_STROBE_SELECT_I               24
#define HPTDC_STROBE_SELECT_BITS            2
#define HPTDC_READOUT_SPEED_SELECT          26
#define HPTDC_TOKEN_DELAY_I                 27
#define HPTDC_TOKEN_DELAY_BITS              4
#define HPTDC_ENABLE_LOCAL_TRAILER          31
#define HPTDC_ENABLE_LOCAL_HEADER           32
#define HPTDC_ENABLE_GLOBAL_TRAILER         33
#define HPTDC_ENABLE_GLOBAL_HEADER          34
#define HPTDC_KEEP_TOKEN                    35
#define HPTDC_MASTER                        36
#define HPTDC_ENABLE_BYTEWISE               37
#define HPTDC_ENABLE_SERIAL                 38
#define HPTDC_ENABLE_JTAG_READOUT           39
#define HPTDC_TDC_ID_I                      40
#define HPTDC_TDC_ID_BITS                   4
#define HPTDC_SELECT_BYPASS_INPUTS          44
#define HPTDC_READOUT_FIFO_SIZE_I           45
#define HPTDC_READOUT_FIFO_SIZE_BITS        3
#define HPTDC_REJECT_COUNT_OFFSET_I         48
#define HPTDC_REJECT_COUNT_OFFSET_BITS      12
#define HPTDC_SEARCH_WINDOW_I               60
#define HPTDC_SEARCH_WINDOW_BITS            12
#define HPTDC_MATCH_WINDOW_I                72
#define HPTDC_MATCH_WINDOW_BITS             12
#define HPTDC_LEADING_RESOLUTION_I          84
#define HPTDC_LEADING_RESOLUTION_BITS       3
#define HPTDC_FIXED_PATTERN_I               87
#define HPTDC_FIXED_PATTERN_BITS            28
#define HPTDC_ENABLE_FIXED_PATTERN          115
#define HPTDC_MAX_EVENT_SIZE_I              116
#define HPTDC_MAX_EVENT_SIZE_BITS           4
#define HPTDC_REJECT_READOUT_FIFO_FULL      120
#define HPTDC_ENABLE_READOUT_OCCUPANCY      121
#define HPTDC_ENABLE_READOUT_SEPARATOR      122
#define HPTDC_ENABLE_OVERFLOW_DETECT        123
#define HPTDC_ENABLE_RELATIVE               124
#define HPTDC_ENABLE_AUTOMATIC_REJECT       125
#define HPTDC_EVENT_COUNT_OFFSET_I          126
#define HPTDC_EVENT_COUNT_OFFSET_BITS       12
#define HPTDC_TRIGGER_COUNT_OFFSET_I        138
#define HPTDC_TRIGGER_COUNT_OFFSET_BITS     12
#define HPTDC_EN_SET_CNT_ON_BUNCH_RESET     150
#define HPTDC_ENABLE_MASTER_RESET_CODE      151
#define HPTDC_EN_MRESET_ON_EVENT_RESET      152
#define HPTDC_EN_RES_CHBUF_WHEN_SEPARATOR   153
#define HPTDC_EN_SEPARATOR_ON_EVENT_RESET   154
#define HPTDC_EN_SEPARATOR_ON_BUNCH_RESET   155
#define HPTDC_ENABLE_DIRECT_EVENT_RESET     156
#define HPTDC_ENABLE_DIRECT_BUNCH_RESET     157
#define HPTDC_ENABLE_DIRECT_TRIGGER         158
#define HPTDC_OFFSET31_I                    159
#define HPTDC_OFFSET_BITS                   9
#define HPTDC_OFFSET30_I                    168
#define HPTDC_OFFSET29_I                    177
#define HPTDC_OFFSET28_I                    186
#define HPTDC_OFFSET27_I                    195
#define HPTDC_OFFSET26_I                    204
#define HPTDC_OFFSET25_I                    213
#define HPTDC_OFFSET24_I                    222
#define HPTDC_OFFSET23_I                    231
#define HPTDC_OFFSET22_I                    240
#define HPTDC_OFFSET21_I                    249
#define HPTDC_OFFSET20_I                    258
#define HPTDC_OFFSET19_I                    267
#define HPTDC_OFFSET18_I                    276
#define HPTDC_OFFSET17_I                    285
#define HPTDC_OFFSET16_I                    294
#define HPTDC_OFFSET15_I                    303
#define HPTDC_OFFSET14_I                    312
#define HPTDC_OFFSET13_I                    321
#define HPTDC_OFFSET12_I                    330
#define HPTDC_OFFSET11_I                    339
#define HPTDC_OFFSET10_I                    348
#define HPTDC_OFFSET9_I                     357
#define HPTDC_OFFSET8_I                     366
#define HPTDC_OFFSET7_I                     375
#define HPTDC_OFFSET6_I                     384
#define HPTDC_OFFSET5_I                     393
#define HPTDC_OFFSET4_I                     402
#define HPTDC_OFFSET3_I                     411
#define HPTDC_OFFSET2_I                     420
#define HPTDC_OFFSET1_I                     429
#define HPTDC_OFFSET0_I                     438
#define HPTDC_COARSE_COUNT_OFFSET_I         447
#define HPTDC_COARSE_COUNT_OFFSET_BITS      12
#define HPTDC_DLL_TAP_ADJUST31_I            459
#define HPTDC_DLL_TAP_ADJUST_BITS           3
#define HPTDC_DLL_TAP_ADJUST30_I            462
#define HPTDC_DLL_TAP_ADJUST29_I            465
#define HPTDC_DLL_TAP_ADJUST28_I            468
#define HPTDC_DLL_TAP_ADJUST27_I            471
#define HPTDC_DLL_TAP_ADJUST26_I            474
#define HPTDC_DLL_TAP_ADJUST25_I            477
#define HPTDC_DLL_TAP_ADJUST24_I            480
#define HPTDC_DLL_TAP_ADJUST23_I            483
#define HPTDC_DLL_TAP_ADJUST22_I            486
#define HPTDC_DLL_TAP_ADJUST21_I            489
#define HPTDC_DLL_TAP_ADJUST20_I            492
#define HPTDC_DLL_TAP_ADJUST19_I            495
#define HPTDC_DLL_TAP_ADJUST18_I            498
#define HPTDC_DLL_TAP_ADJUST17_I            501
#define HPTDC_DLL_TAP_ADJUST16_I            504
#define HPTDC_DLL_TAP_ADJUST15_I            507
#define HPTDC_DLL_TAP_ADJUST14_I            510
#define HPTDC_DLL_TAP_ADJUST13_I            513
#define HPTDC_DLL_TAP_ADJUST12_I            516
#define HPTDC_DLL_TAP_ADJUST11_I            519
#define HPTDC_DLL_TAP_ADJUST10_I            522
#define HPTDC_DLL_TAP_ADJUST9_I             525
#define HPTDC_DLL_TAP_ADJUST8_I             528
#define HPTDC_DLL_TAP_ADJUST7_I             531
#define HPTDC_DLL_TAP_ADJUST6_I             534
#define HPTDC_DLL_TAP_ADJUST5_I             537
#define HPTDC_DLL_TAP_ADJUST4_I             540
#define HPTDC_DLL_TAP_ADJUST3_I             543
#define HPTDC_DLL_TAP_ADJUST2_I             546
#define HPTDC_DLL_TAP_ADJUST1_I             549
#define HPTDC_DLL_TAP_ADJUST0_I             552
#define HPTDC_RC_ADJUST_I                   555
#define HPTDC_RC_ADJUST_BITS                12
#define HPTDC_NOT_USED_I                    567
#define HPTDC_NOT_USED_BITS                 3
#define HPTDC_LOW_POWER_MODE                570
#define HPTDC_WIDTH_SELECT_I                571
#define HPTDC_WIDTH_SELECT_BITS             4
#define HPTDC_VERNIER_OFFSET_I              575
#define HPTDC_VERNIER_OFFSET_BITS           5
#define HPTDC_DLL_CONTROL_I                 580
#define HPTDC_DLL_CONTROL_BITS              4
#define HPTDC_DEAD_TIME_I                   584
#define HPTDC_DEAD_TIME_BITS                2
#define HPTDC_TEST_INVERT                   586
#define HPTDC_TEST_MODE                     587
#define HPTDC_ENABLE_TRAILING               588
#define HPTDC_ENABLE_LEADING                589
#define HPTDC_MODE_RC_COMPRESSION           590
#define HPTDC_MODE_RC                       591
#define HPTDC_DLL_MODE_I                    592
#define HPTDC_DLL_MODE_BITS                 2
#define HPTDC_PLL_CONTROL_I                 594
#define HPTDC_PLL_CONTROL_BITS              8
#define HPTDC_SERIAL_CLOCK_DELAY_I          602
#define HPTDC_SERIAL_CLOCK_DELAY_BITS       4
#define HPTDC_IO_CLOCK_DELAY_I              606
#define HPTDC_IO_CLOCK_DELAY_BITS           4
#define HPTDC_CORE_CLOCK_DELAY_I            610
#define HPTDC_CORE_CLOCK_DELAY_BITS         4
#define HPTDC_DLL_CLOCK_DELAY_I             614
#define HPTDC_DLL_CLOCK_DELAY_BITS          4
#define HPTDC_SERIAL_CLOCK_SOURCE_I         618
#define HPTDC_SERIAL_CLOCK_SOURCE_BITS      2
#define HPTDC_IO_CLOCK_SOURCE_I             620
#define HPTDC_IO_CLOCK_SOURCE_BITS          2
#define HPTDC_CORE_CLOCK_SOURCE_I           622
#define HPTDC_CORE_CLOCK_SOURCE_BITS        2
#define HPTDC_DLL_CLOCK_SOURCE_I            624
#define HPTDC_DLL_CLOCK_SOURCE_BITS         3
#define HPTDC_ROLL_OVER_I                   627
#define HPTDC_ROLL_OVER_BITS                12
#define HPTDC_ENABLE_MATCHING               639
#define HPTDC_ENABLE_PAIR                   640
#define HPTDC_ENABLE_TTL_SERIAL             641
#define HPTDC_ENABLE_TTL_CONTROL            642
#define HPTDC_ENABLE_TTL_RESET              643
#define HPTDC_ENABLE_TTL_CLOCK              644
#define HPTDC_ENABLE_TTL_HIT                645
#define HPTDC_SETUP_PARITY                  646

// CONTROL string
#define HPTDC_ENABLE_PATTERN_I              0
#define HPTDC_ENABLE_PATTERN_BITS           4
#define HPTDC_GLOBAL_RESET                  4
#define HPTDC_ENABLE_CHANNEL_I              5
#define HPTDC_ENABLE_CHANNEL_BITS           32
#define HPTDC_DLL_RESET                     37
#define HPTDC_PLL_RESET                     38
#define HPTDC_CONTROL_PARITY                39

// STATUS string
#define HPTDC_ERROR_I                       0
#define HPTDC_ERROR_BITS                    11
#define HPTDC_HAVE_TOKEN                    11
#define HPTDC_ROFIFO_OCCUPANCY_I            12
#define HPTDC_ROFIFO_OCCUPANCY_BITS         8
#define HPTDC_ROFIFO_FULL                   20
#define HPTDC_ROFIFO_EMPTY                  21
#define HPTDC_G3_L1_OCCUPANCY_I             22
#define HPTDC_G_L1_OCCUPANCY_BITS           8
#define HPTDC_G2_L1_OCCUPANCY_I             30
#define HPTDC_G1_L1_OCCUPANCY_I             38
#define HPTDC_G0_L1_OCCUPANCY_I             46
#define HPTDC_TFIFO_OCCUPANCY_I             54
#define HPTDC_TFIFO_OCCUPANCY_BITS          4
#define HPTDC_TFIFO_FULL                    58
#define HPTDC_TFIFO_EMPTY                   59
#define HPTDC_DLL_LOCK                      60
#define HPTDC_NOT_USED                      61

//-----------------------------------------------------------------------------
#endif // _HPTDCSTRING_H_
