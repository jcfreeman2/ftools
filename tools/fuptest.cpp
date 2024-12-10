#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h>

#include "version.h"
#include "FlxConfig.h"
#include "FlxUpload.h"
#include "FlxReceiver.h"
#include "FlxDataChecker.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int   VERSION_ID = 0x21092900; // Use FlxConfig class
//const int VERSION_ID = 0x21011500; // Option -i DMA read index; auto DMA write
//const int VERSION_ID = 0x19012900; // Option -P 4: use constant val per elink
//const int VERSION_ID = 0x19012200; // Option -X: use circular-mode DMA
//const int VERSION_ID = 0x18120300; // Option -y: add EC and IC link
//const int VERSION_ID = 0x18080100;

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int      opt;
  bool     check_elink  = true;
  bool     expect_elink = true;
  bool     receive      = true;
  bool     use_intrpt   = false;
  int      cardnr       = 0;
  int      gbtnr        = -1;
  int      egroupnr     = -1;
  int      ewidth       = 2;
  int      dma_write    = -1; // Autoselect FromHost DMA controller index
  int      dma_read     = 0;
  uint32_t dma_size     = 1;
  bool     dma_circular = false;
  int      speed_factor = 1;
  int      pattern_id   = 0;
  int      repeat       = 30;
  uint64_t datasize     = 32;
  uint64_t buffersize   = 128ULL*1024ULL*1024ULL;
  bool     ec_ic        = false;
  bool     sca_aux      = false;

  // Parse the options
  while( (opt = getopt(argc, argv,
                       "b:cd:ef:G:g:hIi:P:r:Rs:Vw:x:XyY")) != -1 )
    {
      switch( opt )
        {
        case 'b':
          // Buffer size in MBytes
          if( sscanf( optarg, "%lu", &buffersize ) != 1 )
            arg_error( 'b' );
          buffersize *= 1024ULL*1024ULL;
          if( buffersize > 4096ULL*1024ULL*1024ULL )
            {
              buffersize = 4096ULL*1024ULL*1024ULL;
              cout << "(buffer size capped to 4GB)" << endl;
            }
          break;
        case 'c':
          check_elink = false;
          break;
        case 'd':
          if( sscanf( optarg, "%d", &cardnr ) != 1 )
            arg_error( 'd' );
          break;
        case 'e':
          expect_elink = false;
          break;
        case 'f':
          // A factor to increase upload speed with (at your own risk)
          if( sscanf( optarg, "%d", &speed_factor ) != 1 )
            arg_error( 'f' );
          if( speed_factor < 0 || speed_factor > 999 )
            arg_range( 'f', 0, 999 );
          break;
        case 'G':
          // GBT link number to upload to
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'g':
          // Selected E-group number to do the test on
          if( sscanf( optarg, "%d", &egroupnr ) != 1 )
            arg_error( 'g' );
          if( egroupnr < 0 || egroupnr >= FLX_FROMHOST_GROUPS-1 )
            arg_range( 'g', 0, FLX_FROMHOST_GROUPS-1-1 );
          break;
        case 'h':
          usage();
          return 0;
        case 'I':
          use_intrpt = true;
          break;
        case 'i':
          // DMA controller to use for receiving
          if( sscanf( optarg, "%d", &dma_read ) != 1 )
            arg_error( 'i' );
          if( dma_read < 0 || dma_read > 7 )
            arg_range( 'i', 0, 7 );
          break;
        case 'P':
          // Test data pattern identifier
          if( sscanf( optarg, "%d", &pattern_id ) != 1 )
            arg_error( 'P' );
          if( pattern_id < 0 || pattern_id > 4 )
            arg_range( 'P', 0, 4 );
          break;
        case 'r':
          // Test data chunk repeat counter
          if( sscanf( optarg, "%d", &repeat ) != 1 )
            arg_error( 'r' );
          if( repeat < 0 )
            arg_range( 'r', 0, 0x7FFFFFFF );
          break;
        case 'R':
          receive = false;
          break;
        case 's':
          // Number of test data bytes to upload
          if( sscanf( optarg, "%lu", &datasize ) != 1 )
            arg_error( 's' );
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          return 0;
        case 'w':
          // E-link width in bits
          if( sscanf( optarg, "%d", &ewidth ) != 1 )
            arg_error( 'w' );
          if( !(ewidth == 2 || ewidth == 4 || ewidth == 8 || ewidth == 16) )
            {
              cout << "### -w: argument not one of [2,4,8,16]" << endl;
              return 0;
            }
          break;
        case 'x':
          // DMA size in KByte
          if( sscanf( optarg, "%u", &dma_size ) != 1 )
            arg_error( 'x' );
          if( dma_size < 1 || dma_size > buffersize/1024 )
            arg_range( 'x', 1, buffersize/1024 );
          break;
        case 'X':
          dma_circular = true;
          break;
        case 'y':
          ec_ic = true;
          break;
        case 'Y':
          ec_ic = true;
          sca_aux = true;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Need space for headers in the data structure to be uploaded
  // for simplicity allow upload of up to half the buffer size
  if( datasize <= 0 || datasize * (uint64_t) repeat > buffersize/2 )
    {
      cout << "### -s: size argument missing or not in range (1.."
           << buffersize << ")" << endl;
      return 1;
    }

  FlxUpload fup( cardnr, buffersize, dma_write );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Sender  : Opened FLX-device " << cardnr
       << ", firmw " << fup.firmwareVersion()
       << ", " << fup.numberOfChans()
       <<  " chans (cmem bufsize="
       << fup.bufferSize()/(1024*1024) << "MB, ";
  if( dma_circular ) cout << "CIRCULAR ";
  cout << "DMA #" << fup.dmaIndex() << ")" << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  fup.setDmaSize( dma_size*1024 );

  fup.setDmaCircular( dma_circular );

  FlxConfig fconfig( cardnr );
  if( fconfig.hasError() )
    {
      cout << "### " << fconfig.errorString() << endl;
      return 1;
    }

  FlxReceiver *frecv = 0;
  if( receive )
    {
      frecv = new FlxReceiver( cardnr, buffersize, dma_read );
      if( frecv->hasError() )
        {
          cout << "### " << frecv->errorString() << endl;
          frecv->stop();
          return 1;
        }
      cout << "Receiver: Opened FLX-device " << cardnr
           << ", firmw " << frecv->firmwareVersion()
           << ", " << frecv->numberOfChans()
           <<  " chans (cmem bufsize="
           << frecv->bufferSize()/(1024*1024) << "MB, "
           << "DMA #" << dma_read;
      if( use_intrpt )
        cout << " interrupt-driven)" << endl;
      else
        cout << " polling)" << endl;

      frecv->setUseInterrupt( use_intrpt );
    }

  if( !check_elink )
    {
      cout <<  "**NB**: E-link validity not checked!" << endl;
    }

  // GBT link or links to scan
  int gbt_start, gbt_end;
  if( gbtnr == -1 )
    {
      gbt_start = 0;
      gbt_end   = fup.numberOfChans();
    }
  else
    {
      gbt_start = gbtnr;
      gbt_end   = gbtnr + 1;
    }

  // E-group or E-groups to scan
  int egroup_start, egroup_end;
  if( egroupnr == -1 )
    {
      egroup_start = 0;
      egroup_end   = FLX_FROMHOST_GROUPS-1;
    }
  else
    {
      egroup_start = egroupnr;
      egroup_end   = egroupnr + 1;
    }

  // E-paths to scan, which depends on the selected E-link width
  int epath_incr;
  int epath_start = 0;
  if( ewidth == 2 )
    {
      epath_incr = 1;
    }
  else if( ewidth == 4 )
    {
      epath_incr = 2;
    }
  else if( ewidth == 8 )
    {
      epath_incr = 4;
#if (REGMAP_VERSION < 0x500)
      epath_start = 1;
#endif
    }
  else
    {
#if (REGMAP_VERSION < 0x500)
      epath_incr = 5;
      epath_start = 3;
#else
      epath_incr = 8;
#endif
    }

  cout << setfill( '0' );

  // Compile a list of egroup/epath numbers to be included in the test,
  // and include the (virtual) EC and IC elinks on request
  typedef struct lnk { int g, p; } lnk_t;
  vector<lnk_t> elinklist;
  lnk_t link;
  int epathnr;
  for( egroupnr = egroup_start; egroupnr<egroup_end; ++egroupnr )
    for( epathnr=epath_start; epathnr<FLX_ELINK_PATHS; epathnr+=epath_incr )
      {
        if( sca_aux ) continue; // Skip in case of LTDB firmware..
        link.g = egroupnr;
        link.p = epathnr;
        elinklist.push_back( link );
      }
  if( ec_ic )
    {
      // Add EC and IC
      link.g = 7;
      link.p = 7;
      elinklist.push_back( link );
      link.g = 7;
      link.p = 6;
      elinklist.push_back( link );
    }
  if( sca_aux )
    {
      // Add SCA_AUX channel (LTDB firmware)
      link.g = 7;
      link.p = 5;
      elinklist.push_back( link );
    }

  bool first = true;
  int uploadsize;
  for( gbtnr = gbt_start; gbtnr<gbt_end; ++gbtnr )
    //for( egroupnr = egroup_start; egroupnr<egroup_end; ++egroupnr )
    //for( epathnr=epath_start; epathnr<FLX_ELINK_PATHS; epathnr+=epath_incr ){
    for( unsigned int i=0; i<elinklist.size(); ++i )
      {
        egroupnr = elinklist[i].g;
        epathnr  = elinklist[i].p;

        if( check_elink )
          {
            // Check whether the selected e-link is configured
            // and thus a valid link on the selected FLX-device
            if( !fconfig.isEnabledElink( gbtnr, egroupnr, epathnr, &ewidth ) )
              {
                cout << "### E-link " << gbtnr << "-" << egroupnr << "-"
                     << epathnr << " not valid or not configured" << endl;
                continue;
              }
          }

        // Upload to E-link
        int elinknr = ((gbtnr    << BLOCK_GBT_SHIFT) |
                       (egroupnr << BLOCK_EGROUP_SHIFT) |
                       (epathnr  << BLOCK_EPATH_SHIFT));
        if( pattern_id != 4 )
          uploadsize = fup.prepareData( elinknr, datasize, pattern_id, repeat );
        else
          // SPECIAL: constant byte value per E-link: E-link number low-byte)
          uploadsize = fup.prepareData( elinknr, datasize,
                                        pattern_id | (elinknr << 8), repeat );
        int cnt = 0;
        if( uploadsize > 0 )
          {
            if( first )
              {
                cout << "Uploading " << repeat << " chunks of "
                     << datasize << " bytes: "
                     << uploadsize << " bytes";
                int dma_size = fup.dmaSize();
                cout << ", using ";
                if( dma_circular ) cout << "CIRCULAR ";
                cout << "DMA #" << fup.dmaIndex()
                     << ", in units of " << dma_size << " bytes -> "
                     << (uploadsize+dma_size-1)/dma_size << " DMAops"
                     << endl;

                first = false;
              }

            cout << "E-link "
                 << setfill('0') << hex << setw(3) << elinknr << " " << dec
                 << gbtnr << "-" << egroupnr << "-" << epathnr << ": ";
            if( frecv == 0 ) cout << endl;

            while( !fup.uploadFinished() )
              {
                usleep(1000);
                if( fup.upload( speed_factor ) )
                  ++cnt;
              }
          }
        else
          {
            cout << "### Upload E-link " << gbtnr << "-" << egroupnr << "-"
                 << epathnr << ": "
                 << "something is wrong, upload size = "
                 << uploadsize << endl;
          }
        cout << "upload()=" << cnt << ", ";

        // Do not receive data ?
        if( frecv == 0 )
          continue;

        int elinknr_check = elinknr; // For pattern_id == 4
        if( !expect_elink )
          elinknr = -1;

        FlxDataChecker flxChecker;
        int      timeout_us = 10000; // 10 ms
        uint8_t *chunkdata = 0;
        uint32_t size;
        int64_t  chunknr = 0;
        flxChecker.setReceiver( frecv );
        while( flxChecker.nextChunkRecvd( &chunkdata, &size,
                                          timeout_us, elinknr ) )
          {
            bool err = false;
            uint32_t i, err_i;

            // Check chunk size
            if( size != datasize )
              // Take into account: odd size chunk is padded by
              // an additional byte to make it an even number of bytes
              if( !(size == datasize+1 && (datasize & 0x1) != 0) )
                {
                  cout << "### Size=" << size
                       << ", expected " << datasize << endl;
                  //err = true;
                }

            // Check chunk data
            if( pattern_id == 0 )
              for( i=0; i<size; ++i )
                {
                  // Incrementing pattern
                  if( chunkdata[i] != (u8) (i & 0xFF) )
                    if( !(i == datasize &&
                          chunkdata[i] == (u8) 0xFF) )
                      {
                        err = true;
                        err_i = i;
                        break;
                      }
                }
            else if( pattern_id == 1 )
              for( i=0; i<size; ++i )
                {
                  // 0x55/0xAA pattern
                  if( ((i & 1) == 1 && chunkdata[i] != (u8) 0xAA) ||
                      ((i & 1) == 0 && chunkdata[i] != (u8) 0x55) )
                    if( !(i == datasize &&
                          chunkdata[i] == (u8) 0xFF) )
                      {
                        err = true;
                        err_i = i;
                        break;
                      }
                }
            else if( pattern_id == 2 )
              for( i=0; i<size; ++i )
                {
                  // 0xFF pattern
                  if( chunkdata[i] != (u8) 0xFF )
                    {
                      err = true;
                      err_i = i;
                      break;
                    }
                }
            else if( pattern_id == 3 )
              for( i=0; i<size; ++i )
                {
                  // Incrementing-per-chunk pattern
                  if( chunkdata[i] != (u8) (chunknr & 0xFF) )
                    {
                      err = true;
                      err_i = i;
                      break;
                    }
                }
            else if( pattern_id == 4 )
              for( i=0; i<size; ++i )
                {
                  // E-link number lower byte
                  if( chunkdata[i] != (u8) (elinknr_check & 0xFF) )
                    {
                      err = true;
                      err_i = i;
                      break;
                    }
                }

            // Error in data? -> display data
            if( err )
              {
                cout << "### Data error, size=" << size
                     << " index=" << err_i <<":" << endl << hex;
                for( i=0; i<size; ++i )
                  {
                    cout << setw(2) << (uint32_t) chunkdata[i] << " ";
                    if( (i & 0x1F) == 0x1F ) cout << endl;
                  }
                if( (i & 0x1F) != 0 ) cout << endl;
                cout << dec;
                // Pause...
                //char ch;
                //cin >> ch;
              }

            ++chunknr;
          }

        cout << "processed " << flxChecker.blocksHandled() << " blocks, "
             << chunknr << " chunks, pattern: ";
        if( pattern_id == 0 )
          cout << "incr";
        else if( pattern_id == 1 )
          cout << "55/AA";
        else if( pattern_id == 2 )
          cout << "FF";
        else if( pattern_id == 3 )
          cout << "incr/chunk";
        else if( pattern_id == 4 )
          cout << "elinknr low-byte";
        else
          cout << "???";

        if( egroupnr == 7 && epathnr == 7 )
          cout << " (EC)";
        else if( egroupnr == 7 && epathnr == 6 )
          cout << " (IC)";
        else if( egroupnr == 7 && epathnr == 5 )
          cout << " (SCA-AUX)";

        cout << endl;

        // Expected number of chunks received?
        if( chunknr != repeat )
          cout << "### Received " << chunknr
               << ", expected " << repeat << " chunks" << endl;
      }

  if( !dma_circular ) // In order to allow check of DMA status afterwards
    fup.stop();

  if( frecv )
    frecv->stop();

  cout << "Exiting.." << endl;
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fuptest version " << hex << VERSION_ID << dec << endl <<
    "Uploads test data to every E-link of the given width "
    "of the given FLX-device in turn;\noptionally select a single GBT (-G).\n"
    "Checks whether the e-link is valid and configured on the FLX-device,\n"
    "unless option -c is given.\n\n"
    "Expects to receive the data in a loopback on the same e-link number,\n"
    "unless option -e is given.\n"
    "The data is checked byte-for-byte for correctness.\n\n"

    "Usage: fuptest [-h|V] [-d <devnr>] [-b <size>] | [-G <gbt>] [-g <group>]"
    " [-I] [-i <dma]\n"
    "               [-s <bytes>] [-r <chunks>] [-P <patt>] [-w <width]\n"
    "               [-c] [-e] [-f <speed>] [-R] [-x <size>] [-X] [-y|Y]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -b <size>  : DMA (cmem_rcc) memory buffer size to use, in MB "
    "(default 32, max 4096).\n"
    "  -c         : Do not check whether e-link is configured on FLX-device.\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e         : Loopback not necessarily on the same e-link number "
    "(default: receive=upload e-link).\n"
    "  -f <speed> : Speed up default upload rate of about 8MB/s by "
    "factor <speed> (default: 1)\n"
    "  -G <gbt>   : GBT-link number (default: all).\n"
    "  -g <group> : Group number (default: all groups).\n"
    "  -I         : use interrupt to receive data (default: polling)\n"
    "  -i <dma>   : FLX-device DMA controller to receive data (default: 0).\n"
    "  -P <patt>  : Test data pattern: 0=incr, 1=0x55/0xAA, 2=0xFF,\n"
    "               3=incr-per-chunk, 4=val-per-elink (default: 0).\n"
    "  -r <chunks>: Test data chunk count: upload <chunks>*<bytes> "
    "bytes of data (default: 30).\n"
    "  -R         : Do not receive data (use e.g. 'fedump' for that).\n"
    "  -s <bytes> : Number of bytes per chunk to upload (default: 32).\n"
    "  -w <width> : E-link width in bits (2, 4, 8 or 16, default: 2).\n"
    "  -x <size>  : Size of single-shot DMA transfers, in KByte "
    "(default: 1).\n"
    "  -X         : Use continuous-mode DMA for upload "
    "(default: single-shot).\n"
    "  -y         : Include EC and IC 'E-links' in the test.\n"
    "  -Y         : Only EC, IC and SCA-AUX 'E-links' in the test "
    "(LTDB firmware test).\n\n";
}

// ----------------------------------------------------------------------------
