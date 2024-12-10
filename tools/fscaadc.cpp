#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>
#include <math.h>

#include "version.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x19031300; // Add circular DMA option -X
//const int VERSION_ID = 0x18111300;

#define sleep_ms(x) usleep((x)*1000)

void display_replies( FlxReceiver *recvr,
                      int          elinknr,
                      string       title = string() );
float ntc_volt_to_temp( float volt, int kohm );

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  cardnr       = 0;
  int  gbtnr        = -1;
  int  egroupnr     = 7; // EC-link
  int  epathnr      = 7; // EC-link
  int  elinknr      = -1;
  int  dma_write    = -1;// Autoselect FromHost DMA controller index
  int  dma_read     = 0;
  bool dma_circular = false;
  bool receive      = true;
  bool sca_connect  = false;
  bool sca_reset    = false;
  bool adc_v1       = false;
  bool adc_enable   = true;
  bool adc_disable  = false;
  int  adc_curr_src = 0;
  int  adc_input    = -1;
  int  adc_interval = 200;
  int  repeat_cnt   = 1;
  int  ntc_kohm     = -1;
  int  exp_cnt      = 0;

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "Ac:Cd:DEe:G:g:hi:n:p:r:Rt:VXZ")) != -1 )
    {
      switch( opt )
        {
        case 'A':
          adc_v1 = true;
          break;
        case 'C':
          sca_connect = true;
          break;
        case 'c':
          // ADC current source bitmask
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'c' );
          adc_curr_src = x;
          if( x < 0x00000001 || x > 0x7FFFFFFF )
            arg_range( 'c', 0x1, 0x7FFFFFFF );
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'D':
          adc_disable = true;
          break;
        case 'E':
          adc_enable = false;
          break;
        case 'e':
          // E-link number
          if( sscanf( optarg, "%x", &x ) != 1 )
            arg_error( 'e' );
          elinknr = x;
          if( elinknr < 0 || elinknr > FLX_MAX_ELINK_NR )
            arg_range( 'e', 0, FLX_MAX_ELINK_NR );
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
          // ADC input index
          if( sscanf( optarg, "%d", &adc_input ) != 1 )
            arg_error( 'i' );
          if( adc_input < 0 || adc_input > 31 )
            arg_range( 'i', 0, 31 );
          break;
        case 'n':
          // NTC reference KOhm values
          if( sscanf( optarg, "%d", &ntc_kohm ) != 1 )
            arg_error( 'n' );
          if( ntc_kohm < 1 || ntc_kohm > 100 )
            arg_range( 'n', 1, 100 );
          break;
        case 'p':
          // E-path number to upload to
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'r':
          if( sscanf( optarg, "%d", &repeat_cnt ) != 1 )
            arg_error( 'r' );
          if( repeat_cnt < 1 || repeat_cnt > 100 )
            arg_range( 'r', 1, 100 );
          break;
        case 'R':
          sca_reset = true;
          break;
        case 't':
          // Time interval (in microseconds) between ADC conversions
          // of the same input
          if( sscanf( optarg, "%d", &adc_interval ) != 1 )
            arg_error( 't' );
          if( adc_interval < 0 || adc_interval > 10000 )
            arg_range( 't', 0, 10000 );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'X':
          dma_circular = true;
          break;
        case 'Z':
          receive = false;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Check for either a valid -e or valid -G/g/p options
  if( (elinknr != -1 && !(gbtnr == -1 || egroupnr == -1 || epathnr == -1)) ||
      (elinknr == -1 && !(gbtnr != -1 && egroupnr != -1 && epathnr != -1)) )
    {
      cout << "### Provide either -e or -G/g/p options" << endl;
      return 1;
    }
  if( elinknr != -1 )
    {
      // Derive GBT, e-group and e-path numbers from the given e-link number
      gbtnr    = (elinknr & BLOCK_GBT_MASK) >> BLOCK_GBT_SHIFT;
      egroupnr = (elinknr & BLOCK_EGROUP_MASK) >> BLOCK_EGROUP_SHIFT;
      epathnr  = (elinknr & BLOCK_EPATH_MASK) >> BLOCK_EPATH_SHIFT;
    }
  else
    {
      // Derive e-link number from the given GBT, group and epath numbers
      elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                 (egroupnr << BLOCK_EGROUP_SHIFT) |
                 (epathnr  << BLOCK_EPATH_SHIFT));
   }

  // -------------------------------------------------------------------------
  // FLX-card sender and receiver

  FlxUpload fup( cardnr, 0, dma_write );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-device " << cardnr
       << ", firmw " << fup.firmwareVersion() << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  //fup.setDmaSize( 8*1024 );

  fup.setDmaCircular( dma_circular );

  FlxReceiver *frecv = 0;
  if( receive )
    {
      frecv = new FlxReceiver( cardnr, 64*1024*1024, dma_read );
      if( frecv->hasError() )
        {
          cout << "### " << frecv->errorString() << endl;
          frecv->stop();
          return 1;
        }
      /*
      cout << "Opened FLX-device " << cardnr
           << ", firmw " << frecv->firmwareVersion()
           << ", " << frecv->numberOfChans()
           <<  " chans (cmem bufsize="
           << frecv->bufferSize() << ", receive: "
           << "DMA #" << dma_read << ")" << endl;
      */
    }

  // -------------------------------------------------------------------------
  // Initialize

  if( sca_connect )
    {
      if( fup.scaConnect( elinknr ) )
        cout << "GBT-SCA connect" << endl;
      else
        cout << "###GBT-SCA connect: " << fup.errorString() << endl;
      ++exp_cnt;
    }

  if( sca_reset )
    {
      if( fup.scaReset( elinknr ) )
        cout << "GBT-SCA reset" << endl;
      else
        cout << "###GBT-SCA reset: " << fup.errorString() << endl;
      ++exp_cnt;
    }

  // -------------------------------------------------------------------------
  // Configure

  uint8_t data[4], dev_mask = 0;
  int     trid = 1;
  int     cmd;

  if( adc_enable || adc_disable )
    {
      // Do it twice... first one possibly results in SREJ reply...
      int recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_D, data );
      recv_trid = 254;
      fup.uploadScaFrame( elinknr, &recv_trid, SCA_DEV_CONFIG, 0,
                          SCA_CONFIG_RD_D, data );
      exp_cnt += 2;

      FlxDataChecker flxChecker;
      uint8_t       *chunkdata = 0;
      uint32_t       size;
      int            cnt = 0;
      flxChecker.setReceiver( frecv );
      while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
        {
          if( size == 0 ) continue;
          if( size <= 12 && chunkdata[2] == 0xFE ) // Transaction ID 254
            dev_mask = chunkdata[7];
          ++cnt;
        }

      //cout << "ENA=" << hex << (uint32_t) dev_mask << dec << endl;

      // Received vs expected number of messages
      if( cnt != exp_cnt )
        cout << "(Expected " << exp_cnt
             << ", recvd " << cnt << " msgs)" << endl;
      exp_cnt = 0;
    }

  if( adc_enable )
    {
      // Enable ADC
      data[0] = 0x00;
      data[1] = dev_mask | (1 << (SCA_DEV_ADC & 0x7));
      //data[1] = (1 << (SCA_DEV_ADC & 0x7)) | (1 << (SCA_DEV_DAC & 0x7));
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_D, data );
      ++exp_cnt;

      if( adc_v1 )
        {
          // ###What to write??
          data[0] = 0x33;
          data[1] = 0x33;
          data[2] = 0x33;
          data[3] = 0x33;
          //data[1] = 0xFF;
          cmd = 0x70; // Write gain? (read = 0x71)
          fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, cmd, data );
          ++exp_cnt;
        }

      cout << "ADC enabled" << endl;
    }

  if( adc_curr_src > 0 )
    {
      // Enable current sources for selected ADC inputs
      data[0] = (uint8_t) ((adc_curr_src & 0x00FF0000) >> 16);
      data[1] = (uint8_t) ((adc_curr_src & 0xFF000000) >> 24);
      data[2] = (uint8_t) ((adc_curr_src & 0x000000FF) >>  0);
      data[3] = (uint8_t) ((adc_curr_src & 0x0000FF00) >>  8);
      cmd = SCA_ADC_WR_CURR;
      if( adc_v1 ) cmd = SCA_ADC_WR_CUREN_V1;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, cmd, data );
      ++exp_cnt;

      cout << "ADC current source setting: 0x" << hex << setfill('0')
           << setw(8) << adc_curr_src << dec << setfill( ' ') << endl;
      sleep_ms( 10 );
    }

  // -------------------------------------------------------------------------
  // Read out

  int repeat, input, recv_trid;
  data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0;
  if( adc_input < 0 )
    {
      for( repeat=0; repeat<repeat_cnt; ++repeat )
        for( input=0; input<32; ++input )
          {
            // Select ADC input
            data[2] = input;
            cmd = SCA_ADC_WR_MUX;
            if( adc_v1 ) cmd = SCA_ADC_WR_MUX_V1;
            fup.addScaFrame( &trid, SCA_DEV_ADC, 4, cmd, data );
            fup.addScaDelayUs( 10 );
            ++exp_cnt;

            // Start a conversion:
            // Both ADC_GO and ADC_GO_V1 seem to work (for ADC V1)...
            // (but ADC_GO_V1 takes much longer to execute(?))
            cmd = SCA_ADC_GO;
            data[2] = 1; // NB: if not 1 ADC_GO hangs up the ADC V2
                         // (permanent state of BUSY)!
            //if( adc_v1 ) cmd = SCA_ADC_GO_V1;
            recv_trid = 254;
            fup.addScaFrame( &recv_trid, SCA_DEV_ADC, 4, cmd, data );
            // ...and add requested delay
            fup.addScaDelayUs( adc_interval );
            //sleep_ms( 1 );
            ++exp_cnt;
          }
      // Send block of GBT-SCA commands
      fup.uploadScaFrames( elinknr );
    }
  else
    {
      // Select ADC input
      data[2] = adc_input;
      cmd = SCA_ADC_WR_MUX;
      if( adc_v1 ) cmd = SCA_ADC_WR_MUX_V1;
      fup.addScaFrame( &trid, SCA_DEV_ADC, 4, cmd, data );
      //fup.addScaDelayUs( adc_interval );
      ++exp_cnt;

      for( repeat=0; repeat<repeat_cnt; ++repeat )
        {
          // Start a conversion:
          cmd = SCA_ADC_GO;
          //if( adc_v1 ) cmd = SCA_ADC_GO_V1;
          data[2] = 1;
          recv_trid = 254;
          fup.addScaFrame( &recv_trid, SCA_DEV_ADC, 4, cmd, data );
          // ...and add requested delay
          fup.addScaDelayUs( adc_interval );
          ++exp_cnt;
        }
      // Send block of GBT-SCA commands
      fup.uploadScaFrames( elinknr );
    }

  // -------------------------------------------------------------------------
  // Display

  // DEBUG
  //display_replies( frecv, elinknr, "GBT-SCA ADC readings" );

  cout << "GBT-SCA ADC readings";
  if( adc_v1 ) cout << " (V1)";
  if( adc_input >= 0 )
    cout << " Input #" << adc_input;
  cout << ":" << endl;

  // Error bit identifiers
  const char *SCA_ERR_STR[] = {
    "Generic", "CHAN", "CMD", "TRID", "LEN", "DISA", "BUSY", "PROG"
  };

  FlxDataChecker flxChecker;
  uint8_t       *chunkdata = 0;
  uint32_t       size;
  int            chan = 0; // Expected channel counter
  int            cnt  = 0; // Message counter
  unsigned int   dataword;
  cout << fixed << setprecision( 3 );
  flxChecker.setReceiver( frecv );
  while( flxChecker.nextChunkRecvd( &chunkdata, &size, 5000, elinknr ) )
    {
      if( size == 0 ) continue;

      // Find chunks with SCA replies containing
      // our dedicated Transaction ID of 254
      if( size == 12 && chunkdata[2] == 0xFE ) // Transaction ID
        {
          dataword = (((uint32_t) chunkdata[8]) << 0) |
            (((uint32_t) chunkdata[9]) << 8) |
            (((uint32_t) chunkdata[6]) << 16) |
            (((uint32_t) chunkdata[7]) << 24);

          cout << setw(2) << chan << ": " << hex << setfill('0') << uppercase
               << setw(3) << dataword << "  =  "
               << dec << setfill(' ') << setw(4) << dataword << "  =  "
               << (float) dataword/4095.0 << " V";

          if( ((chan & 0x1F) == 31 && adc_input == -1) || adc_input == 31 )
            {
              // The GBT-SCA T-sensor
              cout << setprecision( 1 ) << " (T="
                   << (720 - 1000.0*((float) dataword/4095.0))/1.83
                   << "C approx.)" << setprecision( 3 );
            }
          else if( ntc_kohm > 0 &&
                   ((adc_input == -1 &&
                     (adc_curr_src & (1<<(chan & 0x1F))) != 0) ||
                    (adc_input != -1 &&
                     (adc_curr_src & (1<<adc_input)) != 0)) )
            {
              // Current source is enabled on this input:
              // assume there is an NTC connected of the given KOhm value:
              // convert the reading to degrees celcius
              cout << setprecision( 1 ) << " (T="
                   << ntc_volt_to_temp( (float) dataword/4095.0, ntc_kohm )
                   << "C NTC=" << ntc_kohm << "K)" << setprecision( 3 );
            }
          cout << endl;
          // Maintain an expected channel number or counter
          ++chan;
          if( adc_input == -1 && chan == 32 )
            {
              chan = 0;
              if( repeat_cnt > 1 ) cout << endl;
            }
        }
      else if( chunkdata[2] == 0xFE )
        {
          // One of the requested Transaction IDs, but not of the expected size
          if( chunkdata[4] != 0 )
            {
              // Error byte unequal to zero
              cout << setw(2) << chan << ": ";
              cout << "###ERR=";
              for( int i=0; i<8; ++i )
                if( chunkdata[4] & (1<<i) )
                  cout << SCA_ERR_STR[i] << " ";
              cout << endl;
            }
          // Maintain an expected channel number or counter
          ++chan;
          if( adc_input == -1 && chan == 32 )
            {
              chan = 0;
              if( repeat_cnt > 1 ) cout << endl;
            }
        }
      ++cnt; // Update message counter
    }

  // Received vs expected number of messages
  if( cnt != exp_cnt )
    cout << "(Expected " << exp_cnt
         << ", recvd " << cnt << " msgs)" << endl;
  exp_cnt = 0;

  // -------------------------------------------------------------------------
  // Disable

  if( adc_curr_src > 0 )
    {
      // Disable current sources
      data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0;
      cmd = SCA_ADC_WR_CURR;
      if( adc_v1 ) cmd = SCA_ADC_WR_CUREN_V1;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_ADC, 4, cmd, data );
    }

  if( adc_disable )
    {
      data[0] = 0x00;
      data[1] = dev_mask;
      fup.uploadScaFrame( elinknr, &trid, SCA_DEV_CONFIG, 2,
                          SCA_CONFIG_WR_D, data );
      cout << "ADC disabled" << endl;
    }

  // -------------------------------------------------------------------------
  // Finish up

  fup.stop();
  if( frecv )
    frecv->stop();
  cout << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fscaadc version " << hex << VERSION_ID << dec << endl <<
    "Tool to read GBT-SCA ADC input channels and display the readings.\n"
    "Usage:\n"
    " fscaadc [-h|V] [-d <devnr>] [-e <elink>] [-G <gbt>] [-g <group>] "
    "[-p <path>] [-c <msk>] [-A]\n"
    "         [-i <index>] [-n <kohm>] [-r <cnt>] [-t <us>] [-C] [-D] [-E] "
    "[-R] [-X] [-Z]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number (default: 0).\n"
    "  -g <group> : Group number (default matches GBT EC 'group' = 7).\n"
    "  -p <path>  : E-path number (default matches GBT EC 'path' = 7).\n"
    "  -A         : Use SCA-V1 ADC commands (default: SCA-V2 ADC).\n"
    "  -c <mask>  : Enable current sources on the ADC inputs in bitmask "
    "<mask>\n"
    "               (disabled again afterwards).\n"
    "  -i <index> : Conversion of ADC input <index> only "
    "(default: all 32 inputs consecutively).\n"
    "  -n <kohm>  : NTC reference resistance value in KOhm;\n"
    "               for ADC inputs with current source enabled (option -c)\n"
    "               a temperature in Celcius is now calculated assuming "
    "they have such NTCs connected.\n"
    "  -r <cnt>   : Number of times to convert ADC input or inputs "
    "(default: 1).\n"
    "  -t <us>    : Microseconds between ADC conversions (default: 200).\n"
    "  -C         : Send GBT-SCA connect (HDLC control).\n"
    "  -R         : Send GBT-SCA reset (HDLC control).\n"
    "  -D         : Disable GBT-SCA ADC after operation "
    "(default: leave enabled)\n"
    "  -E         : Do *not* enable GBT-SCA ADC at start of operation, "
    "assume it already is.\n"
    "  -X         : Use continuous-mode DMA for upload "
    "(default: single-shot).\n"
    "  -Z         : Do not receive and display the GBT-SCA replies.\n";
}

// ----------------------------------------------------------------------------

void display_replies( FlxReceiver *recvr,
                      int          elinknr,
                      string       title )
{
  if( recvr == 0 ) return;

  // Optionally have a title for the GBT-SCA replies displayed
  cout << endl;
  if( title.length() )
    cout << ">> " << title << " <<" << endl;

  // Go through the chunks of each block (skipping any TIMEOUT/NULL/OOB chunks)
  // displaying the data bytes, interpreting chunks as GBT-SCA replies
  FlxDataChecker flxChecker;
  int  timeout_us  = 5000;
  bool display_sca = true;
  flxChecker.displayDataRecvd( recvr, timeout_us, elinknr, display_sca );
}

// ----------------------------------------------------------------------------

// Constants used in NTC temperature conversion (Ohms to degrees)
// Rt/R25 range 68.600 to 3.274 (T from -50 to 0 degrees):
#define ALL 3.3538646E-03
#define BLL 2.5654090E-04
#define CLL 1.9243889E-06
#define DLL 1.0969244E-07
// Rt/R25 range 3.274 to 0.36036 (T from 0 to 50 degrees):
#define AL  3.3540154E-03
#define BL  2.5627725E-04
#define CL  2.0829210E-06
#define DL  7.3003206E-08
// Rt/R25 range 0.36036 to 0.06831 (T from 50 to 100 degrees):
#define AH  3.3539264E-03
#define BH  2.5609446E-04
#define CH  1.9621987E-06
#define DH  4.6045930E-08
// Rt/R25 range 0.06831 to 0.01872 (T from 100 to 150 degrees):
#define AHH 3.3368620E-03
#define BHH 2.4057263E-04
#define CHH (-2.6687093E-06)
#define DHH (-4.0719355E-07)

float ntc_volt_to_temp( float volt, int kohm )
{
  // Conversion from NTC resistance to degrees celcius:
  //
  // T = (1.0 / (a + b*ln(r) + c*(ln(r))^2 + d*(ln(r))^3)) - 273.15
  //
  // with r = Rt/R25 (with 'kohm' equal to R25,
  // the NTC resistance value at 25C, e.g. 5K)
  // and a, b, c and d according to the values shown above

  float temp, ohms, r, ln_r, ln_r2, ln_r3;

  ohms  = volt / (float) 0.0001; // GBT-SCA current source is 100uA = 0.1mA
  r     = ohms / ((float) kohm * 1000.0);
  ln_r  = log( r );
  ln_r2 = ln_r * ln_r;
  ln_r3 = ln_r2 * ln_r;

  if( r <= (float) 0.36036 )
    {
      if( r <= 0.06831 )
        // 0.06831 >= r > 0.01872 ==> 100C <= Temperature < 150C
        temp = AHH + BHH*ln_r + CHH*ln_r2 + DHH*ln_r3;
      else
        // 0.36036 >= r >= 0.06831 ==> 50C <= Temperature < 100C
        temp = AH + BH*ln_r + CH*ln_r2 + DH*ln_r3;
    }
  else
    {
      if( r > 3.274 )
        // 68.600 >= r > 3.274 ==> -50C <= Temperature < 0C
        temp = ALL + BLL*ln_r + CLL*ln_r2 + DLL*ln_r3;
      else
        // 3.274 >= r > 0.36036 ==> 0C <= Temperature < 50C
        temp = AL + BL*ln_r + CL*ln_r2 + DL*ln_r3;
    }

  // Temperature in centigrade
  temp = ((float) 1.0/temp) - (float) 273.15;

  // Temperature in millidegrees centigrade
  //temp = (((float) 1.0/temp) - (float) 273.15) * (float) 1000.0;

  return temp;
}

// ----------------------------------------------------------------------------
