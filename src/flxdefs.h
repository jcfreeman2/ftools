#ifndef FLXDEFS_H
#define FLXDEFS_H

typedef struct chunk_desc
{
  unsigned int index, length, type;
  bool trunc, err, crcerr;
  int  invalid_sz;
} chunk_desc_t;

// Constants for FELIX/FLX-cards
const int FLX_LINKS           = 24;
const int FLX_ELINK_PROCS     = 15;
const int FLX_ELINK_PATHS     = 8;
const int FLX_TOHOST_GROUPS   = (7+1);
const int FLX_FROMHOST_GROUPS = (5+1);
const int FLX_MAXCHUNK_UNIT   = 512;
const int FLX_MAX_ELINK_NR    = 2047;
const int FLX_SUBCHNK_TYPES   = 8;

enum {
  CHUNKTYPE_NULLFILL = 0,
  CHUNKTYPE_FIRST,     // 1->0x20
  CHUNKTYPE_LAST,      // 2->0x40
  CHUNKTYPE_BOTH,      // 3->0x60
  CHUNKTYPE_MIDDLE,    // 4->0x80
  CHUNKTYPE_TIMEOUT,   // 5->0xA0
  CHUNKTYPE_UNDEFINED6,// 6->0xC0
  CHUNKTYPE_OUTOFBAND  // 7->0xE0
};

// Definitions for FLX-card data blocks (from-GBT/to-Host)

#define BLOCKHEADER_BYTES          4

#define BLOCK_ID                   0xABCD
#define BLOCK_ID_32B               0xABCE
#define BLOCK_ID_PHASE2            0xC0CE

#define BLOCK_SEQNR_MASK           0xF800
#define BLOCK_SEQNR_SHIFT          11
#define BLOCK_ELINK_MASK           0x07FF
#define BLOCK_ELINK_SHIFT          0
#define BLOCK_GBT_MASK             0x07C0
#define BLOCK_GBT_SHIFT            6
#define BLOCK_EGROUP_MASK          0x0038
#define BLOCK_EGROUP_SHIFT         3
#define BLOCK_EPATH_MASK           0x0007
#define BLOCK_EPATH_SHIFT          0
#define BLOCK_SEQNR_MAX           (BLOCK_SEQNR_MASK>>BLOCK_SEQNR_SHIFT)
#define BLOCK_ELINK_MAX            BLOCK_ELINK_MASK

// 16-bit chunk trailer
#define TRAILER_BYTES              2
#define TRAILER_LENGTH_MSK         0x03FF
#define TRAILER_CRCERR_MSK         0x0400
#define TRAILER_ERROR_MSK          0x0800
#define TRAILER_TRUNC_MSK          0x1000
#define TRAILER_TYPE_MSK           0xE000
#define TRAILER_CRCERR_SHIFT       10
#define TRAILER_ERROR_SHIFT        11
#define TRAILER_TRUNC_SHIFT        12
#define TRAILER_TYPE_SHIFT         13

// 32-bit chunk trailer
#define TRAILER_BYTES_32B          4
#define TRAILER_LENGTH_MSK_32B     0x0000FFFF
#define TRAILER_BUSY_MSK_32B       0x02000000
#define TRAILER_CRCERR_MSK_32B     0x04000000
#define TRAILER_ERROR_MSK_32B      0x08000000
#define TRAILER_TRUNC_MSK_32B      0x10000000
#define TRAILER_TYPE_MSK_32B       0xE0000000
#define TRAILER_CRCERR_SHIFT_32B   26
#define TRAILER_ERROR_SHIFT_32B    27
#define TRAILER_TRUNC_SHIFT_32B    28
#define TRAILER_TYPE_SHIFT_32B     29

#include "FlxTrailerFormat.h"

// Chunk contents and indices
// (NB: concerns (emulator) test data for test purposes)
#define CHUNKHDR_SIZE              8
#define CHUNK_ID                   0xAA
#define CHUNK_ID_I                 0
#define CHUNK_SZ_HI_I              1
#define CHUNK_SZ_LO_I              2
//#define CHUNK_CNTR_HI_I          3
//#define CHUNK_CNTR_LO_I          4
#define CHUNK_CNTR_I               3
#define CHUNK_EID_I                4
#define CHUNK_BB_I                 5
#define CHUNK_AA_I                 6
#define CHUNK_ELINKBITS_I          7

// Definitions for FLX-card to-GBT/from-Host data
#define TOGBT_BLOCK_BYTES         (16*2)
#define TOGBT_PAYLOAD_BYTES       (15*2)
#define TOGBT_EOM_MASK             0x0001
#define TOGBT_EOM_SHIFT            0
#define TOGBT_LENGTH_MASK          0x001E
#define TOGBT_LENGTH_SHIFT         1
#define TOGBT_EPATH_MASK           0x00E0
#define TOGBT_EPATH_SHIFT          5
#define TOGBT_EGROUP_MASK          0x0700
#define TOGBT_EGROUP_SHIFT         8
#define TOGBT_GBT_MASK             0xF800
#define TOGBT_GBT_SHIFT            11
#define TOGBT_ELINK_MASK           0xFFE0
#define TOGBT_ELINK_SHIFT          5

// Definitions for FLX-card to-GBT/from-Host data (continued)
// RM5: no more EoM bit, instead length=0x1F means 'Not EoM'
#define TOGBT_LENGTH_MASK_5BIT     0x001F

// FLX-card interrupt numbers
#if REGMAP_VERSION < 0x500
#define FLX_INT_FH_WRAPAROUND      0
#define FLX_INT_TH_WRAPAROUND      1
#define FLX_INT_DATA_AVAILABLE     2
#define FLX_INT_FH_FULL            3
#define FLX_INT_TEST               4
#define FLX_INT_CR_XOFF            5
#define FLX_INT_TH_ALMOSTFULL      6
#define FLX_INT_TH_FULL            7
#else
// Interrupts 'data available' match DMA controller numbers
// (so 4 controllers have interrupts 0 to 3 resp. assigned to them)
#define FLX_INT_DATA_AVAILABLE     0
#endif

// GBT-SCA stuff
// -------------
#define HDLC_SOF               0x7E
// HDLC control
#define HDLC_CTRL_SFORMAT      0x01
#define HDLC_CTRL_UFORMAT      0x03
#define HDLC_CTRL_RECVREADY    (0x00|HDLC_CTRL_SFORMAT)
#define HDLC_CTRL_REJECT       (0x08|HDLC_CTRL_SFORMAT)
#define HDLC_CTRL_RECVNOTREADY (0x04|HDLC_CTRL_SFORMAT)
#define HDLC_CTRL_SELREJECT    (0x0C|HDLC_CTRL_SFORMAT)
#define HDLC_CTRL_TYPE_MASK    (0x0C|HDLC_CTRL_SFORMAT)
// HDLC I-frame fields
#define HDLC_CTRL_SEQNR_MASK   0x07
#define HDLC_CTRL_POLLBIT      0x10
#define HDLC_CTRL_NRECVD_SHIFT 5
#define HDLC_CTRL_NSENT_SHIFT  1
// HDLC U-frame control commands
#define HDLC_CTRL_CONNECT      0x2F
#define HDLC_CTRL_RESET        0x8F
#define HDLC_CTRL_TEST         0xE3
#define HDLC_CTRL_POLLBIT      0x10
// GBT-SCA device identifiers (Channel byte)
#define SCA_DEV_CONFIG         0x00
#define SCA_DEV_SPI            0x01
#define SCA_DEV_GPIO           0x02
#define SCA_DEV_I2C            0x03
#define SCA_I2C_DEVICES        16
#define SCA_DEV_JTAG           0x13
#define SCA_DEV_ADC            0x14
#define SCA_DEV_DAC            0x15
#define SCA_DEV_MAX            SCA_DEV_DAC
// GBT-SCA error code byte
#define SCA_ERR_CHAN           0x02
#define SCA_ERR_CMD            0x04
#define SCA_ERR_TRID           0x08
#define SCA_ERR_LEN            0x10
#define SCA_ERR_CHAN_DISA      0x20
#define SCA_ERR_CHAN_BUSY      0x40
#define SCA_ERR_IN_PROGRESS    0x80
// SCA_DEV_CONFIG commands
#define SCA_CONFIG_WR_A        0x00
#define SCA_CONFIG_RD_A        0x01
#define SCA_CONFIG_WR_B        0x02
#define SCA_CONFIG_RD_B        0x03
#define SCA_CONFIG_WR_C        0x04
#define SCA_CONFIG_RD_C        0x05
#define SCA_CONFIG_WR_D        0x06
#define SCA_CONFIG_RD_D        0x07
// SCA_DEV_GPIO commands
#define SCA_GPIO_WR_OUT        0x10
#define SCA_GPIO_RD_OUT        0x11
#define SCA_GPIO_RD_IN         0x01
#define SCA_GPIO_WR_DIR        0x20
#define SCA_GPIO_RD_DIR        0x21
// SCA_DEV_DAC commands
#define SCA_DAC_A_WR           0x10
#define SCA_DAC_B_WR           0x20
#define SCA_DAC_C_WR           0x30
#define SCA_DAC_D_WR           0x40
#define SCA_DAC_A_RD           0x11
#define SCA_DAC_B_RD           0x21
#define SCA_DAC_C_RD           0x31
#define SCA_DAC_D_RD           0x41
// SCA_DEV_ADC commands
#define SCA_ADC_GO             0x02
#define SCA_ADC_WR_MUX         0x50
#define SCA_ADC_RD_MUX         0x51
#define SCA_ADC_WR_CURR        0x60
#define SCA_ADC_RD_CURR        0x61
#define SCA_ADC_WR_GAIN        0x10
#define SCA_ADC_RD_GAIN        0x11
#define SCA_RD_CHIPID          0xD1
#define SCA_RD_CHIPID_V1       0x91
// SCA_DEV_ADC commands (SCA-V1)
#define SCA_ADC_GO_V1          0xB2
#define SCA_ADC_WR_MUX_V1      0x30
#define SCA_ADC_RD_MUX_V1      0x31
#define SCA_ADC_WR_CUREN_V1    0x40
#define SCA_ADC_RD_CUREN_V1    0x41
// SCA_DEV_I2C commands
#define SCA_I2C_WR_CTRL        0x30
#define SCA_I2C_RD_CTRL        0x31
#define SCA_I2C_RD_STAT        0x11
#define SCA_I2C_SINGLE_7B_WR   0x82
#define SCA_I2C_SINGLE_7B_RD   0x86
#define SCA_I2C_MULTI_7B_WR    0xDA
#define SCA_I2C_MULTI_7B_RD    0xDE
#define SCA_I2C_SINGLE_10B_WR  0x8A
#define SCA_I2C_SINGLE_10B_RD  0x8E
#define SCA_I2C_MULTI_10B_WR   0xE2
#define SCA_I2C_MULTI_10B_RD   0xE6
#define SCA_I2C_WR_DATA0       0x40
#define SCA_I2C_RD_DATA0       0x41
#define SCA_I2C_WR_DATA1       0x50
#define SCA_I2C_RD_DATA1       0x51
#define SCA_I2C_WR_DATA2       0x60
#define SCA_I2C_RD_DATA2       0x61
#define SCA_I2C_WR_DATA3       0x70
#define SCA_I2C_RD_DATA3       0x71
// SCA_DEV_JTAG commands
#define SCA_JTAG_WR_CTRL       0x80
#define SCA_JTAG_RD_CTRL       0x81
#define SCA_JTAG_WR_FREQ       0x90
#define SCA_JTAG_RD_FREQ       0x91
#define SCA_JTAG_WR_TDO0       0x00
#define SCA_JTAG_WR_TDO1       0x10
#define SCA_JTAG_WR_TDO2       0x20
#define SCA_JTAG_WR_TDO3       0x30
#define SCA_JTAG_RD_TDI0       0x01
#define SCA_JTAG_RD_TDI1       0x11
#define SCA_JTAG_RD_TDI2       0x21
#define SCA_JTAG_RD_TDI3       0x31
#define SCA_JTAG_WR_TMS0       0x40
#define SCA_JTAG_WR_TMS1       0x50
#define SCA_JTAG_WR_TMS2       0x60
#define SCA_JTAG_WR_TMS3       0x70
#define SCA_JTAG_RD_TMS0       0x41
#define SCA_JTAG_RD_TMS1       0x51
#define SCA_JTAG_RD_TMS2       0x61
#define SCA_JTAG_RD_TMS3       0x71
#define SCA_JTAG_ARESET        0xC0
#define SCA_JTAG_GO            0xA2
#define SCA_JTAG_GO_MANUAL     0xB0

// SCA_DEV_I2C constants
#define SCA_I2C_CTRL_100K      0x00
#define SCA_I2C_CTRL_200K      0x01
#define SCA_I2C_CTRL_400K      0x02
#define SCA_I2C_CTRL_1M        0x03
#define SCA_I2C_STAT_SUCCESS   0x04
#define SCA_I2C_STAT_LEVELERR  0x08
#define SCA_I2C_STAT_INV_CMD   0x20
#define SCA_I2C_STAT_NOACK     0x40

#endif // FLXDEFS_H
