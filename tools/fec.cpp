#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const   int VERSION_ID = 0x21032300; // Add option -P (pause)
//const int VERSION_ID = 0x21031200; // Invert -N logic
//const int VERSION_ID = 0x21012300; // Option -N
//const int VERSION_ID = 0x19061100; // Receiver buffer 8->64MB
//const int VERSION_ID = 0x19010900; // Count CRC and CMD errors in checker
//const int VERSION_ID = 0x18103000; // Update ADC and I2C operations
//const int VERSION_ID = 0x18101201; // Add option -A (SCA-V1 ADC)
//const int VERSION_ID = 0x18101200; // Use FlxDataChecker::displayDataRecvd()
//const int VERSION_ID = 0x18022800; // Count reply chunks
//const int VERSION_ID = 0x17111700;

#define sleep_ms(x) { if((x)>0) usleep((x)*1000); }

int display_replies( FlxReceiver    *recvr,
                     FlxDataChecker *checkr,
                     uint64_t        timestamp_sent,
                     int             elinknr,
                     string          title = string(),
                     int             expected_replies = -1 );

// To map GPIO bit number to a byte index in the GPIO SCA command
const int GPIO_BYTE_MAP[] = { 2, 3, 0, 1 };

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int   opt;
  int   cardnr      = 0;
  int   gbtnr       = 0;
  int   egroupnr    = 7; // EC-link
  int   epathnr     = 7; // EC-link
  int   dma_write   = -1;// Autoselect FromHost DMA controller index
  int   dma_read    = 0;
  bool  dma_circular = false;
  bool  dma_reset   = false;
  int   hdlc_seq_nr = 0;
  //int sca_addr    = 0;
  bool  receive     = true;
  bool  use_intrpt  = false;
  bool  sca_connect = false;
  bool  sca_reset   = false;
  bool  sca_test    = false;
  int   repeat_cnt  = 0;
  int   time_ms     = -1;
  int   par         = 0;
  int   i2c_dev     = 0;
  bool  do_gpio_out = false;
  bool  do_gpio_in  = false;
  bool  do_adc      = false;
  bool  do_dac      = false;
  bool  do_i2c      = false;
  bool  adc_v1      = false;
  int   pause       = 0;

  // String of chars defining which operations to perform;
  // currently defined: o=GPIO-out, i=GPIO-in a=ADC, d=DAC, I=I2C
  string operations;
  // Parse the options
  while( (opt = getopt(argc, argv, "ACd:G:g:hi:INp:P:r:RTt:VXx:Y:Z")) != -1 )
    {
      switch( opt )
        {
        case 'A':
          adc_v1 = true;
          break;
        case 'C':
          sca_connect = true;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'G':
          // GBT link number to upload to
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'g':
          // E-group number to upload to
          // (NB: Egroup 7 (or 5) + Epath 7 is EC link)
          if( sscanf( optarg, "%d", &egroupnr ) != 1 )
            arg_error( 'g' );
          if( egroupnr < 0 || (egroupnr > FLX_FROMHOST_GROUPS-1 &&
                               egroupnr != 5 && egroupnr != 7) )
            arg_range( 'g', 0, FLX_FROMHOST_GROUPS-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'i':
          // DMA controller to use for receiving
          if( sscanf( optarg, "%d", &dma_read ) != 1 )
            arg_error( 'i' );
          if( dma_read < 0 || dma_read > 7 )
            arg_range( 'i', 0, 7 );
          break;
        case 'I':
          use_intrpt = true;
          break;
        case 'N':
          dma_reset = true;
          break;
        case 'p':
          // E-path number to upload to
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'P':
          // Take a pause before starting actual (circular DMA) operations
          // (to check DMA remains idle until then)
          if( sscanf( optarg, "%d", &pause ) != 1 )
            arg_error( 'P' );
          break;
        case 'r':
          if( sscanf( optarg, "%d", &repeat_cnt ) != 1 )
            arg_error( 'r' );
          break;
        case 'R':
          sca_reset = true;
          break;
        case 'T':
          sca_test = true;
          break;
        case 't':
          if( sscanf( optarg, "%d", &time_ms ) != 1 )
            arg_error( 't' );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'X':
          dma_circular = true;
          break;
        case 'x':
          if( sscanf( optarg, "%d", &par ) != 1 )
            arg_error( 'x' );
          break;
        case 'Y':
          // Initial HLDC receive sequence number
          if( sscanf( optarg, "%d", &hdlc_seq_nr ) != 1 )
            arg_error( 'Y' );
          if( hdlc_seq_nr < 0 || hdlc_seq_nr > 7 )
            arg_range( 'Y', 0, 7 );
          break;
        case 'Z':
          receive = false;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // String defining the operations to perform?
  if( optind < argc )
    {
      operations = string( argv[optind] );
      for( unsigned int i=0; i<operations.size(); ++i )
        if( operations[i] == 'o' )
          do_gpio_out = true;
        else if( operations[i] == 'i' )
          do_gpio_in = true;
        else if( operations[i] == 'a' )
          do_adc = true;
        else if( operations[i] == 'd' )
          do_dac = true;
        else if( operations[i] == 'I' )
          do_i2c = true;
    }

  // -------------------------------------------------------------------------
  // FLX-card sender and receiver

  FlxUpload fup( cardnr, 0, dma_write, hdlc_seq_nr );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Sender  : Opened FLX-device " << cardnr
       << ", firmw " << fup.firmwareVersion() << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  //fup.setDmaSize( 16 * 1024 );
  fup.setDmaCircular( dma_circular );

  FlxReceiver    *flxReceiver = 0;
  FlxDataChecker *flxChecker = 0;
  if( receive )
    {
      flxReceiver = new FlxReceiver( cardnr, 256*1024*1024,
                                     dma_read, dma_reset );
      if( flxReceiver->hasError() )
        {
          cout << "### " << flxReceiver->errorString() << endl;
          flxReceiver->stop();
          return 1;
        }
      cout << "Receiver: Opened FLX-device " << cardnr
           << ", firmw " << flxReceiver->firmwareVersion()
           //<< ", " << flxReceiver->numberOfChans()
           //<< " chans"
           << endl
           << "          (cmem bufsize="
           << flxReceiver->bufferSize()/(1024*1024) << "MB, receive: "
           << "DMA #" << dma_read;
      if( use_intrpt )
        cout << " interrupt-driven)" << endl;
      else
        cout << " polling)" << endl;

      flxReceiver->setUseInterrupt( use_intrpt );

      flxChecker = new FlxDataChecker;
    }

  if( pause > 0 && dma_circular )
    {
      fup.dmaCircularEnable(); // Enable, no further action
      cout << "Enabled (circular) DMA, pausing " << pause << " secs...";
      cout.flush();
      sleep( pause );
      cout << "done" << endl;
    }

  // -------------------------------------------------------------------------

  cout << "Operations: ";
  if( do_gpio_out ) cout << "OUT ";
  if( do_gpio_in )  cout << "IN ";
  if( do_adc )      cout << "ADC ";
  if( do_dac )      cout << "DAC ";
  if( do_i2c )      cout << "I2C ";
  cout << "(repeat=" << repeat_cnt << ")";
  cout << endl;

  int elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                 (egroupnr << BLOCK_EGROUP_SHIFT) |
                 (epathnr  << BLOCK_EPATH_SHIFT));
  int nrecv = 0, nexp = 0; // Number of received and expected GBT-SCA replies
  int nexp_plus = 0;

  cout << "E-link #" << hex << setfill('0') << setw(3) << elinknr
       << setfill(' ') << dec << endl;

  if( sca_connect )
    {
      if( fup.scaConnect( elinknr ) )
        cout << "GBT-SCA connect" << endl;
      else
        cout << "###GBT-SCA connect: " << fup.errorString() << endl;
      //nrecv += display_replies( flxReceiver, flxChecker,
      //                          fup.timestamp(), elinknr, "connect", 1 );
      nexp += 1;
    }

  if( sca_reset )
    {
      if( fup.scaReset( elinknr ) )
        cout << "GBT-SCA reset" << endl;
      else
        cout << "###GBT-SCA reset: " << fup.errorString() << endl;
      //nrecv += display_replies( flxReceiver, flxChecker,
      //                          fup.timestamp(), elinknr, "reset", 1 );
      nexp += 1;
    }

  if( sca_test )
    {
      if( fup.scaTest( elinknr ) )
        cout << "GBT-SCA test" << endl;
      else
        cout << "###GBT-SCA test: " << fup.errorString() << endl;
      //nrecv += display_replies( flxReceiver, flxChecker,
      //                          fup.timestamp(), elinknr, "test", 1 );
      nexp += 1;
    }
  nexp_plus = nexp;

  if( sca_connect || sca_reset || sca_test )
    nrecv += display_replies( flxReceiver, flxChecker,
                              fup.timestamp(), elinknr,
                              "connect/reset/test", nexp_plus );
  nexp_plus = 0;

  char ch;
  u8   data[4];
  int  byte_i;
  u8   bit_i;
  int  trid = 1;
  int  adc_cmd;
  int  repeat;
  if( repeat_cnt <= 0 )
    repeat_cnt = 1;
  if( time_ms < 0 )
    time_ms = 100;

  const int DAC_WR_CMD[] = {
    SCA_DAC_A_WR, SCA_DAC_B_WR, SCA_DAC_C_WR, SCA_DAC_D_WR
  };
  const int DAC_RD_CMD[] = {
    SCA_DAC_A_RD, SCA_DAC_B_RD, SCA_DAC_C_RD, SCA_DAC_D_RD
  };
  const char *DAC_NAME = "ABCD";
  int dac_cmd;

  /*
  // Default do all operations?
  if( do_gpio_out == false && do_gpio_in == false &&
      do_adc == false && do_dac == false )
    {
      do_gpio_out = true;
      do_gpio_in  = true;
      do_adc      = true;
      do_dac      = true;
      do_i2c      = true;
    }
  */

  // -------------------------------------------------------------------------
  //gpio_operations_out:
  if( !do_gpio_out ) goto gpio_operations_in;
  cout << "GPIO operations (OUT):" << endl;

  // Enable GPIO
  data[0] = 0x00;
  data[1] = 1 << SCA_DEV_GPIO;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_B, data );
  //fup.addScaFrame( &trid, SCA_DEV_CONFIG, 2, SCA_CONFIG_WR_B, data );
  sleep_ms( 1 );

  // Set GPIO direction to all output
  //data[0] = 0xFF; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF; //DEBUG
  // Set only GPIO bit 'par' to output:
  // order is bit 16-23, bit 24-31, bit 0-7, bit 8-15
  data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
  byte_i = GPIO_BYTE_MAP[par/8];
  bit_i  = 1 << (par & 0x7);
  data[byte_i] |= bit_i;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_WR_DIR, data );
  //fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_DIR, data );
  sleep_ms( 1 );

  // Read GPIO direction bits
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 0,
                      SCA_GPIO_RD_DIR, data );
  //fup.addScaFrame( &trid, SCA_DEV_GPIO, 0, SCA_GPIO_RD_DIR, data );
  sleep_ms( 1 );

  //fup.uploadScaFrames( elinknr );
  //fup.resetScaFrames();

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "GPIO init", 3 + nexp_plus );
  nexp += 3;
  nexp_plus = 0;

  data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
  repeat = 0;
  while( repeat < repeat_cnt )
    {
      // Official VLDB LEDs on GPIO #18 and #21
      data[byte_i] |= bit_i;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                          SCA_GPIO_WR_OUT, data );
      //fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, data );
      //nrecv += display_replies( flxReceiver, fup.timestamp(), elinknr,
      //                          "GPIO_OUT 1", 1 );
      sleep_ms( time_ms );
      //fup.addScaDelayUs( time_ms * 1000 );

      //data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00; //DEBUG
      data[byte_i] &= ~bit_i;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                          SCA_GPIO_WR_OUT, data );
      //fup.addScaFrame( &trid, SCA_DEV_GPIO, 4, SCA_GPIO_WR_OUT, data );
      //nrecv += display_replies( flxReceiver, fup.timestamp(), elinknr,
      //                          "GPIO_OUT 0", 1 );
      sleep_ms( time_ms );
      //fup.addScaDelayUs( time_ms * 1000 );

      //if( flxReceiver == 0 )
      if( time_ms > 0 )
        {cout << "."; if((repeat & 0x7F) == 0x7F) cout << endl; cout.flush();}
      ++repeat;
    }
  // Send block of GBT-SCA commands
  //fup.uploadScaFrames( elinknr );
  //fup.resetScaFrames();

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "GPIO_OUT", repeat_cnt*2 );
  nexp += repeat_cnt*2;

  // Disable GBT-SCA devices (channels)
  data[0] = 0x00; data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_B, data );
  sleep_ms( 1 );

  // Check if disabled: (try to) set GPIO direction to all output
  data[0] = 0xFF; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_WR_DIR, data );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "GPIO disable", 2 );
  nexp += 2;

  if( do_gpio_in || do_adc || do_dac )
    {
      cout << "\nType any+<return> to continue..";
      cout.flush();
      cin >> ch;
    }

  // -------------------------------------------------------------------------
 gpio_operations_in:
  if( !do_gpio_in ) goto adc_operations;
  cout << "GPIO operations (IN):" << endl;

  // Enable GPIO
  data[0] = 0x00;
  data[1] = 1 << SCA_DEV_GPIO;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_B, data );
  sleep_ms( 1 );

  // Set GPIO direction to all input
  data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_WR_DIR, data );
  sleep_ms( 1 );

  // Read GPIO direction bits
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 0,
                      SCA_GPIO_RD_DIR, data );
  sleep_ms( 1 );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "GPIO init", 3 + nexp_plus );
  nexp += 3;
  nexp_plus = 0;

  repeat = 0;
  while( repeat < repeat_cnt )
    {
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 0,
                          SCA_GPIO_RD_IN, data );
      sleep_ms( time_ms );

      //if( flxReceiver == 0 )
        { cout << "."; cout.flush(); }
      ++repeat;
    }

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "GPIO_IN", repeat_cnt );
  nexp += repeat_cnt;

  // Disable GBT-SCA devices (channels)
  data[0] = 0x00; data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_B, data );
  sleep_ms( 1 );

  // Check if disabled: (try to) set GPIO direction to all output
  data[0] = 0xFF; data[1] = 0xFF; data[2] = 0xFF; data[3] = 0xFF;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_GPIO, 4,
                      SCA_GPIO_WR_DIR, data );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "GPIO disable", 2 );
  nexp += 2;

  if( do_adc || do_dac )
    {
      cout << "\nType any+<return> to continue..";
      cout.flush();
      cin >> ch;
    }

  // -------------------------------------------------------------------------
 adc_operations:
  if( !do_adc ) goto dac_operations;
  cout << "ADC/DAC operations:" << endl;

  // Enable ADC plus DAC
  data[0] = 0x00;
  data[1] = (1 << (SCA_DEV_ADC & 0x7)) | (1 << (SCA_DEV_DAC & 0x7));
  //data[1] = (1 << (SCA_DEV_ADC & 0x7));
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_D, data );
  sleep_ms( 1 );

  // Read back the config register?
  data[0] = 0x00; data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_RD_D, data );
  sleep_ms( 1 );

  // Set DAC output(s)
  //data[0] = 0; data[1] = 0x7B; data[2] = 0; data[3] = 0;
  //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 4,
  //                    SCA_DAC_A_WR, data );
  //sleep_ms( 1 );
  /*
  fup.uploadScaFrame( elinknr &trid, SCA_DEV_DAC, 4,
                      SCA_DAC_B_WR, data );
  sleep_ms( 1 );
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 4,
                      SCA_DAC_C_WR, data );
  sleep_ms( 1 );
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 4,
                      SCA_DAC_D_WR, data );
  sleep_ms( 1 );
  */
  // Read back DAC
  //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 2,
  //                    SCA_DAC_A_RD, data );
  //sleep_ms( 1 );

  // Select ADC input
  //data[0] = 0x1A; data[1] = 0x1B; data[2] = 0x1C; data[3] = 0x1D;
  data[2] = par;
  data[0] = 0; data[1] = 0; data[3] = 0;
  adc_cmd = SCA_ADC_WR_MUX;
  if( adc_v1 ) adc_cmd = SCA_ADC_WR_MUX_V1;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, adc_cmd, data );
  sleep_ms( 1 );

  // Read back selected ADC input
  adc_cmd = SCA_ADC_RD_MUX;
  if( adc_v1 ) adc_cmd = SCA_ADC_RD_MUX_V1;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 0, adc_cmd, data );
  sleep_ms( 1 );

  // Read ADC gain
  adc_cmd = SCA_ADC_RD_GAIN;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 0, adc_cmd, data );
  sleep_ms( 1 );

  // Enable current source for ADC input T-sensors (NTCs) on NSW MM-L1DDC board
  //data[0] = 0x00; data[1] = 0x00; data[2] = 0x07; data[3] = 0x00;
  //adc_cmd = SCA_ADC_WR_CURR;
  //if( adc_v1 ) adc_cmd = SCA_ADC_WR_CUREN_V1;
  //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, adc_cmd, data );
  //sleep_ms( 1 );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "ADC setup", 5 + nexp_plus );
  nexp += 5;
  nexp_plus = 0;

  data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0;
  repeat = 0;
  while( repeat < repeat_cnt )
    {
      data[2] = par & 0xFF; // Read the same ADC input repeatedly
      adc_cmd = SCA_ADC_WR_MUX;
      if( adc_v1 ) adc_cmd = SCA_ADC_WR_MUX_V1;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, adc_cmd, data );
      sleep_ms( 1 );

      // Both ADC_GO and ADC_GO_V1 seem to work (for ADC V1)...
      // (but ADC_GO_V1 takes much longer to execute(?))
      adc_cmd = SCA_ADC_GO;
      data[2] = 1; // NB: if not 1 ADC_GO hangs up the ADC V2 (permanent BUSY)!
      //if( adc_v1 ) adc_cmd = SCA_ADC_GO_V1;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, adc_cmd, data );

      // This one should result in BUSY:
      //adc_cmd = SCA_ADC_GO;
      //if( adc_v1 ) adc_cmd = SCA_ADC_GO_V1;
      //fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 0, adc_cmd, data );

      sleep_ms( time_ms );

      ++repeat;
      //if( flxReceiver == 0 )
        { cout << "."; cout.flush(); }
    }

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "ADC readout", repeat_cnt*2 );
  nexp += repeat_cnt*2;

  // Disable ADC (keep DAC enabled)
  data[0] = 0x00;
  data[1] = (1 << (SCA_DEV_DAC & 0x7));
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_D, data );
  // Check if ADC disabled...
  adc_cmd = SCA_ADC_WR_MUX;
  if( adc_v1 ) adc_cmd = SCA_ADC_WR_MUX_V1;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, adc_cmd, data );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "ADC disable", 2 );
  nexp += 2;

  if( do_dac )
    {
      cout << "\nType any+<return> to continue..";
      cout.flush();
      cin >> ch;
    }

  // -------------------------------------------------------------------------
 dac_operations:
  if( !do_dac ) goto i2c_operations;
  cout << "DAC ";

  // Enable DAC
  data[0] = 0x00;
  data[1] = (1 << (SCA_DEV_DAC & 0x7));
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_D, data );
  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "DAC enable", 1 + nexp_plus );
  nexp += 1;
  nexp_plus = 0;

  sleep_ms( 1 );

  dac_cmd = DAC_WR_CMD[par & 3];
  cout << "DAC-" << DAC_NAME[par & 3];
  cout << " set:" << endl;

  data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0;
  repeat = 0;
  while( repeat < repeat_cnt )
    {
      cout << "dac=" << repeat << endl;

      data[1] = repeat & 0xFF;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 4, dac_cmd, data );

      sleep_ms( time_ms );

      ++repeat;
    }

  // Read (final) DAC value
  dac_cmd = DAC_RD_CMD[par & 3];
  data[1] = 0;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 1, dac_cmd, data );
  sleep_ms( 1 );

  // Reset DAC value to 0
  dac_cmd = DAC_WR_CMD[par & 3];
  data[1] = 0;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 4, dac_cmd, data );
  sleep_ms( 1 );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "DAC set + single read", 2+repeat_cnt );
  nexp += 2+repeat_cnt;

  // Disable DAC
  data[0] = 0x00; data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      SCA_CONFIG_WR_D, data );
  // Check if DAC disabled...
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_DAC, 2, SCA_DAC_A_RD, data );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "DAC disable", 2 );
  nexp += 2;

  // -------------------------------------------------------------------------
 i2c_operations:
  if( !do_i2c ) goto end_of_operations;
  cout << "I2C ";

  i2c_dev = SCA_DEV_I2C + par;

  // Enable I2C channel
  int cmd;
  if( par >= 0 && par < 5 )
    cmd = SCA_CONFIG_WR_B;
  else if( par < 0xD )
    cmd = SCA_CONFIG_WR_C;
  else if( par < 0x10 )
    cmd = SCA_CONFIG_WR_D;
  else
    {
      par = 0;
      cmd = SCA_CONFIG_WR_B;
    }
  data[0] = 0x00;
  data[1] = 1 << (i2c_dev & 0x7);
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                      cmd, data );
  sleep_ms( 1 );

  // Read I2C channel status
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_RD_STAT, data );

  // Write I2C channel control: 400 KHz and 2 bytes payload, reading
  // (i.e. the 2-byte GBTx register address)
  data[0] = 0;
  data[1] = SCA_I2C_CTRL_400K | (2<<2);
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_WR_CTRL, data );
  //sleep_ms( 1 );

  // Read I2C channel control
  data[0] = 0x00;
  data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_RD_CTRL, data );
  //sleep_ms( 1 );

  // Read I2C channel status
  fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2, SCA_I2C_RD_STAT, data );
  sleep_ms( 1 );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "I2C setup", 5 + nexp_plus );
  nexp += 5;
  nexp_plus = 0;

  // DEMO: read out GBTx registers (using I2C address 0, broadcast?)
  repeat = 0;
  while( repeat < repeat_cnt )
    {
      data[0] = (u8) (((repeat+32) & 0xFF00) >> 8);
      data[1] = (u8) ((repeat+32) & 0xFF);
      data[2] = 0x00;
      data[3] = 0x00;
      fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                          SCA_I2C_WR_DATA0, data );
      sleep_ms( 1 );

      // Write the (2) bytes, i.e. the GBTx register address
      u8 i2c_addr = 0; // Broadcast address?
      data[0] = 0;
      data[1] = i2c_addr;
      fup.uploadScaFrame( elinknr, &trid, i2c_dev, 4,
                          SCA_I2C_MULTI_7B_WR, data );
      sleep_ms( 1 );

      // Read the GBTx register content
      cout << "\nGBTX reg " << ((repeat+32) & 0xFFFF) << " in TRID=" << trid;
      fup.uploadScaFrame( elinknr, &trid, i2c_dev, 2,
                          SCA_I2C_SINGLE_7B_RD, data );

      sleep_ms( time_ms );
      ++repeat;
    }

  // Disable I2C channel
  data[0] = 0x00;
  data[1] = 0x00;
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2, cmd, data );
  //sleep_ms( 1 );

  // Check if disabled: read I2C channel status
  fup.uploadScaFrame( elinknr, &trid, SCA_DEV_I2C+par, 2,
                      SCA_I2C_RD_STAT, data );

  nrecv += display_replies( flxReceiver, flxChecker,
                            fup.timestamp(), elinknr,
                            "I2C read", repeat_cnt*3 + 2 );
  nexp += repeat_cnt*3 + 2;

  // -------------------------------------------------------------------------
 end_of_operations:

  cout << endl;
  if( receive )
    {
      cout << "E-link " << hex << setfill('0') << setw(3) << elinknr
           << setfill(' ') << dec;

      if( nrecv != nexp )
        cout << ": ### Total exp chunks: " << nexp << ", got " << nrecv;
      else
        cout << ": Total chunks received: " << nrecv;

      cout << "  (Errors: CRC=" << flxChecker->scaCrcErrors()
           << " CMD=" << flxChecker->scaCmdErrors() << ")" << endl;
    }

  fup.stop();
  if( flxReceiver )
    flxReceiver->stop();
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fec version " << hex << VERSION_ID << dec << endl <<
    "Demo tool for control and read out of various devices on a GBT-SCA\n"
    "through a GBT link's EC channel or any 2-bit wide, HDLC encoded E-link.\n"
    "Receives (and displays) GBT-SCA replies, unless option -Z is given\n"
    "(in that case use e.g. fedump or fdaq to receive the replies).\n"
    "Usage:\n"
    " fec [-h|V] [-d <devnr>] [-i <dma>] [-I] [-N] [-G <gbt>] [-g <group>] "
    "[-p <path>]\n"
    "     [-t <ms>] [-x <par>] [-A] [-C] [-R] [-T] [-P <secs>] "
    "[-X] [-Y <seq>] [-Z] [<ops>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -i <dma>   : FLX-device DMA controller for receiving (default: 0).\n"
    "  -I         : USE interrupt to receive data (default: polling)\n"
    "  -N         : Receiver resets DMA at start-up (default: no reset).\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -r <repeat>: Number of GPIO/ADC/DAC operations to perform "
    "(default: 1).\n"
    "  -A         : Use SCA-V1 ADC commands (default: SCA-V2 ADC).\n"
    "  -C         : Send GBT-SCA connect (HDLC control).\n"
    "  -R         : Send GBT-SCA reset (HDLC control).\n"
    "  -T         : Send GBT-SCA test (HDLC control).\n"
    "  -t <ms>    : Time between some of the ops, in ms (default: 100).\n"
    "  -P <secs>  : Enable FromHost (circular) DMA then pause for <secs> seconds\n"
    "               (for DMA check/debug; default: no pause)\n"
    "  -X         : Use continuous-mode DMA for upload "
    "(default: single-shot).\n"
    "  -x <par>   : Parameter to use in operations, e.g. GPIO number, "
    "ADC or DAC channel (default: 0).\n"
    "  -Y <seq>   : Use <seq> as first HDLC 'receive sequence number'.\n"
    "               (to keep receiving side happy in consecutive calls)\n"
    "  -Z         : Do NOT receive and display the GBT-SCA replies.\n"
    "  <ops>      : String of chars indicating which operation(s) to perform:\n"
    "               o=GPIO-out, i=GPIO-in, a=ADC, d=DAC, I=I2C "
    "(no-string=default: none).\n"
    "Examples:\n"
    "Blink an LED on a VLDB (here connected to GBT link #3, EC-channel)\n"
    "on GBT-SCA GPIO #18 (the other LED is on GPIO #21) 20 times\n"
    "with a rate of 5Hz (100ms ON, 100ms OFF):\n"
    "  fec -G 3 -t 100 -r 20 -x 18 o\n"
    "Read GPIO inputs (GBT-SCA on GBT-link #3's EC-channel) 20 times\n"
    "with a rate of 10Hz:\n"
    "  fec -G 3 -t 100 -r 20 i\n";
}

// ----------------------------------------------------------------------------

int display_replies( FlxReceiver    *recvr,
                     FlxDataChecker *checkr,
                     uint64_t        timestamp_sent,
                     int             elinknr,
                     string          title,
                     int             expected_replies )
{
  if( recvr == 0 ) return 0;

  // Optionally have a title for the GBT-SCA replies displayed
  cout << endl;
  if( title.length() )
    cout << ">> " << title << " <<" << endl;

  // Go through the chunks of each block (skipping any TIMEOUT/NULL/OOB chunks)
  // displaying the data bytes, interpreting chunks as GBT-SCA replies,
  // counting the number of chunks
  int  timeout_us  = 100000;
  bool display_sca = true;
  int  chunks      = 0;
  chunks += checkr->displayDataRecvd( recvr, timeout_us,
                                      elinknr, display_sca );

  //if( chunks == 1 )
    {
      uint64_t ts = recvr->timestamp();
      if( ts > timestamp_sent )
        cout << "T = " << setw(3) << setfill(' ')
             << ((ts-timestamp_sent)+500)/1000
             //<<"." << setfill('0') << setw(3) << (ts-timestamp_sent)%1000
             //<< " (" << ts << " " << timestamp_sent << ")"
             << " us " << endl;
      else
        //cout << "T = ??? recv-sent = " << ts << " - " << timestamp_sent<<endl;
        cout << "T = ???" << endl;
    }

  if( expected_replies >= 0 && chunks != expected_replies )
    cout << "### Chunks expected: " << expected_replies
         << ", got " << chunks << endl;
  if( recvr->fullOccurred() )
    cout << "### Receive buffer full occurred!" << endl;

  return chunks; // Number of chunks received
}

// ----------------------------------------------------------------------------
