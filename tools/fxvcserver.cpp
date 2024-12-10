#include <iostream>
#include <iomanip>
using namespace std;
#include <unistd.h> // For getopt()
#include <ctype.h>  // For toupper()
#include <signal.h> // For sigaction()
#include <string.h> // For strerror()

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "JtagPort.h"
#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x20082600;

#define XVC_DEFAULT_PORT 2542
void xvc_server ( JtagPort &jtagport, int ip_portnr = XVC_DEFAULT_PORT,
                  bool verbose = false );
void xvc_handler( int fd, JtagPort &jtagport, bool verbose = false );
int  sread      ( int fd, void *dest, int len );

void usage();

// ----------------------------------------------------------------------------

void ctrlc_handler( int arg )
{
  throw std::runtime_error( "CTRL-C pressed" );
}

// ----------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  int  opt;
  int  devnr       = 0;
  int  dma_write   = -1; // Autoselect FromHost DMA controller index
  int  dma_read    = 0;
  int  gbtnr       = -1;
  int  egroupnr    = 7; // EC-link
  int  epathnr     = 7; // EC-link
  int  elinknr     = -1;
  int  jtag_rate   = 10; // In MHz
  int  ip_portnr   = XVC_DEFAULT_PORT;
  bool verbose     = false;

  // Install a Ctrl-C signal handler
  struct sigaction sa;
  sa.sa_handler = ctrlc_handler;
  sa.sa_flags   = 0;
  sigemptyset( &sa.sa_mask );
  sigaction( SIGINT, &sa, NULL );

  // Parse the options
  unsigned int x;
  while( (opt = getopt(argc, argv, "d:e:G:g:hp:P:R:Vv")) != -1 )
    {
      switch( opt )
        {
        case 'h':
          usage();
          return 0;
        case 'd':
          if( sscanf( optarg, "%d", &devnr ) != 1 )
            arg_error( 'd' );
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
          // GBT link number
          if( sscanf( optarg, "%d", &gbtnr ) != 1 )
            arg_error( 'G' );
          if( gbtnr < 0 || gbtnr > FLX_LINKS-1 )
            arg_range( 'G', 0, FLX_LINKS-1 );
          break;
        case 'g':
          // E-group number
          // (NB: Egroup 7 (or 5) + Epath 7 is EC link)
          if( sscanf( optarg, "%d", &egroupnr ) != 1 )
            arg_error( 'g' );
          if( egroupnr < 0 || (egroupnr > FLX_FROMHOST_GROUPS-1 &&
                               egroupnr != 5 && egroupnr != 7) )
            arg_range( 'g', 0, FLX_FROMHOST_GROUPS-1 );
          break;
        case 'p':
          // E-path number
          if( sscanf( optarg, "%d", &epathnr ) != 1 )
            arg_error( 'p' );
          if( epathnr < 0 || epathnr > FLX_ELINK_PATHS-1 )
            arg_range( 'p', 0, FLX_ELINK_PATHS-1 );
          break;
        case 'P':
          // IP port number to listen on
          if( sscanf( optarg, "%d", &ip_portnr ) != 1 )
            arg_error( 'P' );
          if( ip_portnr < 0 || ip_portnr > 65535 )
            arg_range( 'P', 0, 65535 );
          break;
        case 'R':
          // JTAG clock rate (in MHz)
          if( sscanf( optarg, "%d", &jtag_rate ) != 1 )
            {
              cout << "### -R: error in argument" << endl;
              usage();
              return 1;
            }
          if( !(jtag_rate == 20 || jtag_rate == 10 ||
                jtag_rate == 5 || jtag_rate == 4 ||
                jtag_rate == 2 || jtag_rate == 1) )
            {
              cout << "### -R: argument not one of "
                   << "[1,2,4,5,10,20] MHz" << endl;
              return 1;
            }
          break;
        case 'V':
          cout << "Version " << hex << VERSION_ID << dec << endl;
          return 0;
        case 'v':
          verbose = true;
          break;
        default: // '?'
          usage();
          return 1;
        }
    }

  // Check for either a valid -e or valid -G/g/p options:
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
  // FLX-device sender and receiver

  FlxUpload fup( devnr, 0, dma_write );
  if( fup.hasError() )
    {
      cout << "### " << fup.errorString() << endl;
      return 1;
    }
  cout << "Opened FLX-device " << devnr
       << ", firmw " << fup.firmwareVersion() << endl;

  if( fup.fanOutLocked() )
    cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
  fup.setFanOutForDaq();

  //fup.setDmaSize( 32*1024 );
  fup.setDmaCircular( true );

  FlxReceiver *frecv = new FlxReceiver( devnr, 512*1024*1024, dma_read );
  if( frecv->hasError() )
    {
      cout << "### " << frecv->errorString() << endl;
      frecv->stop();
      return 1;
    }
  frecv->setUseInterrupt( false );

  // -------------------------------------------------------------------------

  try {
    JtagPort jtagport( fup, frecv, elinknr );
    jtagport.configure( jtag_rate );
    jtagport.setVerbose( verbose );

    xvc_server( jtagport, ip_portnr, verbose );
  }
  catch( std::runtime_error &e ) {
    cout << e.what() << endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------

void xvc_server( JtagPort &jtagport, int ip_portnr, bool verbose )
{
  struct sockaddr_in address;

  int s = socket( AF_INET, SOCK_STREAM, 0 );
  if( s < 0 )
    throw std::runtime_error( "Failed on socket()" );

  int i = 1;
  setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &i , sizeof(i) );

  // This is an Internet address
  address.sin_family = AF_INET;

  // Let the system figure out our IP address
  address.sin_addr.s_addr = INADDR_ANY;

  // This is the port we will listen on
  address.sin_port = htons( (unsigned short) ip_portnr );

  // Bind: associate the parent socket with a port
  if( bind( s, (struct sockaddr *) &address, sizeof(address) ) < 0 )
    throw std::runtime_error( "Failed on bind()" );

  // Listen: make this socket ready to accept connection requests
  if( listen( s, 0 ) < 0 ) // Allow 1 requests to queue up
    throw std::runtime_error( "Failed on listen()" );

  while( true )
    {
      socklen_t nsize = sizeof( address );
      int fd = accept( s, (struct sockaddr *) &address, &nsize );
      if( fd >= 0 )
        {
          uint32_t ipaddr = address.sin_addr.s_addr;
          cout << "Connection accepted: fd=" << fd
               << ", addr="
               << ((ipaddr>>0) & 0xFF) << "."
               << ((ipaddr>>8) & 0xFF) << "."
               << ((ipaddr>>16) & 0xFF) << "."
               << ((ipaddr>>24) & 0xFF) << endl;
        }
      else
        {
          std::string s( strerror(errno) );
          throw std::runtime_error( std::string("accept():") + s );
        }
      // Connected !
      xvc_handler( fd, jtagport, verbose );
      close( fd );
      cout << "Connection closed: fd=" << fd << endl;
    }
  /*
  fd_set conn;
  int maxfd = 0;

  FD_ZERO( &conn );
  FD_SET( s, &conn );

  maxfd = s;

  while( 1 )
    {
      fd_set read = conn, except = conn;
      if( select( maxfd + 1, &read, 0, &except, 0 ) < 0 )
        {
          std::string s( strerror(errno) );
          throw std::runtime_error( std::string("select():") + s );
        }

      int fd, newfd;
      for( fd=0; fd<=maxfd; ++fd )
        {
          if( FD_ISSET(fd, &read) )
            {
              if( fd == s )
                {
                  socklen_t nsize = sizeof( address );
                  newfd = accept( s, (struct sockaddr*) &address, &nsize );

                  if( newfd >= 0 )
                    {
                      uint32_t ipaddr = address.sin_addr.s_addr;
                      cout << "Connection accepted: fd=" << newfd
                           << ", addr="
                           << ((ipaddr>>0) & 0xFF) << "."
                           << ((ipaddr>>8) & 0xFF) << "."
                           << ((ipaddr>>16) & 0xFF) << "."
                           << ((ipaddr>>24) & 0xFF) << endl;

                      if( newfd > maxfd )
                        maxfd = newfd;
                      FD_SET( newfd, &conn );
                    }
                  else
                    {
                      std::string s( strerror(errno) );
                      throw std::runtime_error( std::string("accept():") + s );
                    }
                }
              else
                {
                  xvc_handler( fd, jtagport );

                  cout << "Connection closed: fd=" << fd << endl;
                  close( fd );
                  FD_CLR( fd, &conn );
                }
            }
          else if( FD_ISSET(fd, &except) )
            {
              cout << "Connection interrupted: fd=" << fd << endl;
              close( fd );
              FD_CLR( fd, &conn );
              if( fd == s )
                break;
            }
        }
    }
*/
}

// ----------------------------------------------------------------------------

void xvc_handler( int fd, JtagPort &jtagport, bool verbose )
{
  /*
    The XVC 1.0 communication protocol consists of the following three messages:
    getinfo:
    settck:<period in ns>
    shift:<num bits><tms vector><tdi vector>
    with "getinfo:", "settck:" and "shift:" literal ASCII strings,
    <period in ns> and <num bits> 4-byte integers
    and <tms vector> and <tdi vector> byte arrays, each with a size of
    (<num bits> + 7)/8, so rounded to the next byte.
    For more info see:
    https://www.xilinx.com/support/documentation/application_notes/
            xapp1251-xvc-zynq-petalinux.pdf
  */
  const char xvcInfo[] = "xvcServer_v1.0:2048\n"; // Exact name required!
  int     nbits, nbytes;
  uint8_t data[4096];
  uint8_t reply[4096];

  while( true )
    {
      if( sread( fd, data, 2 ) < 0 )
        return;

      if( memcmp( data, "sh", 2 ) == 0 )
        {
          // Shift command: "shift:"
          if( sread( fd, data, 4 ) < 0 ) // Read "ift:"
            return;
          if( sread( fd, data, 4 ) < 0 )
            return;

          memcpy( &nbits, data, 4 );
          nbytes = (nbits + 7) / 8;

          if( verbose )
            cout << "XVC shift: nbits=" << nbits << endl;

          // Read 'nbytes' bytes with TMS bits,
          // followed by 'nbytes' bytes with TDO bits
          if( sread( fd, data, nbytes*2 ) < 0 )
            return;

          memset( reply, 0, nbytes ); // Init reply bytes to 0

          //cout << "XVC shift: call JtagPort::shift()" << endl;

          uint8_t *tms = &data[0];
          uint8_t *tdo = &data[nbytes];
          uint8_t *tdi = &reply[0];
          jtagport.shift( nbits, tms, tdo, tdi );

          write( fd, reply, nbytes );

          /*
          // DEBUG: display TMS, TDO, TDI bytes
          cout << "TMS= " << hex;
          int j;
          for( j=0; j<nbytes; ++j )
            {
              cout << setw(2) << (unsigned int) tms[j] << " ";
              if( (j & 0x1F) == 0x1F )
                cout << endl << "     ";
            }
          cout << endl;

          cout << "TDO= " << hex;
          for( j=0; j<nbytes; ++j )
            {
              cout << setw(2) << (unsigned int) tdo[j] << " ";
              if( (j & 0x1F) == 0x1F )
                cout << endl << "     ";
            }
          cout << endl;

          cout << "TDI= " << hex;
          for( j=0; j<nbytes; ++j )
            {
              cout << setw(2) << (unsigned int) tdi[j] << " ";
              if( (j & 0x1F) == 0x1F )
                cout << endl << "     ";
            }
          cout << endl;
          */
        }
      else if( memcmp(data, "ge", 2) == 0)
        {
          // Getinfo command: "getinfo:"
          cout << "XVC getinfo: reply=" << xvcInfo << endl;

          if( sread(fd, data, 6 ) < 0 ) // Read "tinfo:"
            return;

          write( fd, xvcInfo, sizeof(xvcInfo)-1 );
        }
      else if( memcmp( data, "se", 2 ) == 0 )
        {
          // Settck command: "settck:"
          if( sread(fd, data, 5) < 0 ) // Read "ttck:"
            return;
          if( sread(fd, data, 4) < 0 )
            return;

          cout << "XVC settck: " << *((uint32_t *) data) << endl;

          write( fd, data, 4 );
        }
    }
}

// ----------------------------------------------------------------------------

int sread( int fd, void *dest, int len )
{
  unsigned char *ptr = (unsigned char*) dest;
  int r;
  while( len > 0 )
    {
      r = read( fd, ptr, len );
      if( r <= 0 )
        return r;
      ptr += r;
      len -= r;
    }
  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fxvcserver v" << hex << VERSION_ID << dec << endl <<
    "Relays Xilinx XVC protocol JTAG bit streams to and from the JTAG port "
    "of a GBT-SCA,\n"
    "through its connection to a FELIX system.\n"
    "Usage:\n"
    " fxvcserver [-h|V] [-v] [-d <devnr>] [[-e <elink>] |"
    " [-G <gbt> [-g <group> -p <path>]]\n"
    "            [-P <portnr>] [-R <rate>]\n"
    "  -h         : Show this help text.\n"
    "  -V         : Show version.\n"
    "  -v         : Be verbose (for debugging only).\n"
    "  -d <devnr> : FLX-device to use (default: 0).\n"
    "  -e <elink> : E-link number (hex) or use -G/g/p options.\n"
    "  -G <gbt>   : GBT-link number.\n"
    "  -g <group> : Group number (default: 7=EC).\n"
    "  -p <path>  : E-path number (default: 7=EC).\n"
    "  -P <portnr>: IP port number to listen on (default: 2542).\n"
    "  -R <rate>  : JTAG clock rate, in MHz, 1,2,4,5,10 or 20 (default: 10).\n"
    "\n"
    "In Vivado's Hardware Manager in the TCL Console type \"connect_hw_server\"\n"
    "(if necessary), followed by \"open_hw_target -xvc_url <address>:<portnr>\"\n"
    "to connect to a Xilinx FPGA connected to the GBT-SCA JTAG port,\n"
    "with <address> and <portnr> the IP address and port number\n"
    "of the FELIX host running this fxvcserver.\n";
}

// ----------------------------------------------------------------------------
