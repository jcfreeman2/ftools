// Command line tool to configure ITk strips encoder

#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <algorithm>

#include "version.h"
#include "FlxStrips.h"

using namespace std;

#include "arg.h"

// Version identifier: year, month, day, release number
const int VERSION_ID = 0x21072800;  // initial version

namespace Action {
  enum Type {Enable, Disable, List, Set, Trickle, TrickleWrite, TrickleRun,
             TrickleStop, TrickleSingle, None};    
}

const int NUM_SEGMENTS = 4; // Maximum number of segments per stave

int    cardnr      = 0;
int    dma_write   = -1; // Autoselect FromHost DMA controller index
int    dma_read    = 0;
int    gbtnr       = -1; // which gbt ID to use
int    segment     = -1; // which strips segment to use
int    param_value = -1; // set strips parameter to this value
string param_name;       // set this parameter
bool   file_binary = false; // trickle configuration file is in binary

FlxStrips::strips_param_t param; // set strips parameter with this address and destination
Action::Type action = Action::None; // what the tool needs to do
std::string set_argument; // PARAMETER=0xVALUE kind of string
std::string file_name; // file name to upload to trickle memory

void parse_arguments(int argc, char *argv[]);

// ----------------------------------------------------------------------------

// TODO: add the ability to enable / disable encoders more granually?
// eg:
// enable all
// lcb_link, lcb_command, lcb_trickle, r3l1_link, r3l1_command

int main(int argc, char *argv[])
{
  parse_arguments(argc, argv);

  try
    {
      auto strips = make_unique<FlxStrips>(cardnr, gbtnr, segment, dma_write);

      switch (action) {
      case Action::Enable:
        strips->EnableElinks();
        break;

      case Action::Disable:
        strips->DisableElinks();
        break;
                
      case Action::Set:
        strips->writeParameter(param_name, param_value);
        break;

      case Action::TrickleRun:
        strips->trickleRun();
        break;

      case Action::TrickleSingle:
        strips->trickleSingle();
        break;
            
      case Action::TrickleStop:
        strips->trickleStop();
        break;

      case Action::TrickleWrite:
        strips->trickleWrite(file_name, file_binary);
        break;

      default:
        throw runtime_error("Unexpected operation code " + to_string(action));
      }
    }
  catch(const std::exception& e)
    {
      std::cerr << e.what() << endl;
      exit(1);
    }    

  return 0;
}

// ----------------------------------------------------------------------------

void usage()
{
  cout << "fstrips version " << hex << VERSION_ID << dec << endl <<
    "Tool to configure LCB and R3L1 encoder in supported firmware flavor.\n"
    "Usage:\n"
    " fstrips [-h|V] [-d <devnr>] [-G <gbt>] [-s <segment>] [command]\n"
    "  -h           : Show this help text.\n"
    "  -V           : Show version.\n"
    "  -d <devnr>   : FLX-device to use (default: 0).\n"
    "  -G <gbt>     : GBT-link number.\n"
    "  -s <segment> : Stave segment number (0-4)\n"
    "  -I <i2c>   : GBTX/lpGBT I2C address.\n"
    "\n"
    "=> Examples:\n"
    "\n"
    "Enabling and disabling links:\n"
    "Enable Strips encoders on segment 2: "
    "connected to FLX-device GBT link 0:\n"
    "  fstrips -G 0 -s 2 enable\n"
    "Disable the encoders\n"
    "  fstrips -G 0 -s 2 disable\n"
    "\n"
    "Configuring encoder parameters:\n"
    "List all available encoder configuration parameters:\n"
    "  fstrips list\n"
    "Set parameter LCB_FRAME_PHASE to 0x0002 on segment 0"
    " connected to FLX-device GBT link 0:\n"
    "  fstrips -G 0 -s 0 set LCB_FRAME_PHASE=0x0002\n"  
    "\n"
    "Trickle configuration writing and triggering:\n"  
    "Write contents of an ASCII file to trickle configuration memory:\n"
    "  fstrips -G 0 -s 0 trickle write trickle.txt\n"
    "Write contents of a binary file to trickle configuration memory:\n"
    "  fstrips -G 0 -s 0 -B trickle write trickle.dat\n"
    "Issue a single trickle trigger:\n"
    "  fstrips -G 0 -s 0 trickle single\n"
    "Issue continuous trickle trigger:\n"
    "  fstrips -G 0 -s 0 trickle run\n"
    "Stop sending trickle configuration:\n"
    "  fstrips -G 0 -s 0 trickle stop\n";
}

// ----------------------------------------------------------------------------

// parse numbers formatter like 
// 2134 = decimal
// 0x01F = hex
// throws exception if not parsable
int parse_number(std::string str) {
  int base;
  if (str.find("0x") != std::string::npos) {
    base = 16;
  } else if (str.find("0X") != std::string::npos) {
    base = 16;
  } else {
    base = 10;
  }
  return std::stoi(str, 0, base);
}

// ----------------------------------------------------------------------------

// verify that all required parameters are present
void require_gtx_link() {
  if (gbtnr < 0) {
    cerr << "### Provide a GBT link number (-G)" << endl;
    exit(1);
  }
}

// ----------------------------------------------------------------------------

// verify that all required parameters are present
void require_link_ids() {
  if (gbtnr < 0) {
    cerr << "### Provide a GBT link number (-G)" << endl;
    exit(1);
  } 
  if (segment < 0) {
    cerr << "### Provide a segment number (-s)" << endl;
    exit(1);
  }
}

// ----------------------------------------------------------------------------

// Verify that encoder parameter assignment has required data
void require_strips_parameters() {
  if (set_argument.empty()) {
    cerr << "### Parameter to set is not specified. " << endl;
    FlxStrips::listParameters();
    exit(1);
  }
  if (param_value == -1) {
    cerr << "### BUG: Unknown parameter value" << endl;
    exit(1);
  }
}

// ----------------------------------------------------------------------------

void strips_parameter_parse_error(std::string str) {
  cerr << "### Can't parse \"" << str << "\" as a parameter assignment." << endl;
  cerr << "### Example: LCB_FRAME_PHASE=0x02";
  exit(1);
}

// ----------------------------------------------------------------------------

void strips_parameter_not_found(std::string name) {
  cerr << "### \"" << name << "\" is not a valid parameter name." << endl;
  FlxStrips::listParameters();
  exit(1);
}

// ----------------------------------------------------------------------------

void too_many_arguments(std::string argument) {
  cerr << "### Too many positional arguments, extra argument: " << argument << endl;
  exit(1);
}

// ----------------------------------------------------------------------------

void unknown_action(std::string argument) {
  cerr << "### Unknown action: " << argument << endl;
  exit(1);
}

// ----------------------------------------------------------------------------

// Check if the argument is in range, terminate the program if it's not 
// opt - single-character flar of the command line option
// val - value read from user
// min, max - inclusive range
void check_arg_range(char opt, int val, int min, int max)
{
  if(val < min || val > max ) arg_range(opt, min, max);
}

// ----------------------------------------------------------------------------

// parse expression like LCB_PHASE=0x0003
// and populate the corresponding variables
void parse_strips_parameter_assignment(std::string str) {
  std::string delimeter = "=";
  std::string name, value;

  try
    {        
      std::string::size_type pos = str.find(delimeter);
      if (pos == std::string::npos) {
        cerr << "Please specify the value" << endl;
        cerr << "Example: HCC_MASK=0xFFFF" << endl;
        exit(1);
      }
      param_name = str.substr(0, pos);
      value = str.substr(pos + delimeter.length(), str.length());
      param_value = parse_number(value);
    }
  catch (std::invalid_argument& e) { 
    cerr << "\"" << value << "\" is not a valid number" << endl;
    exit(1);
  }
  catch (const std::exception& e)
    {
      strips_parameter_parse_error(str);
      exit(1);
    }
}

// ----------------------------------------------------------------------------

// parse and verify all arguments
void parse_arguments(int argc, char *argv[]) {
  int opt;

  // Parse the arguments with flags
  while( (opt = getopt(argc, argv, "G:s:d:VhB")) != -1 ) {
    try
      {
        switch (opt) {
        case 'G': // GBT link number
          gbtnr = parse_number(optarg);
          // check_arg_range('G', gbtnr, 0, FLX_LINKS-1);
          break;

        case 's':
          segment = parse_number(optarg);
          check_arg_range('s', segment, 0, NUM_SEGMENTS-1);
          break;

        case 'B':
          file_binary = true;
          break;

        case 'd':
          cardnr = parse_number(optarg);
          break;

        case 'V':
          cout << "Version " << hex << VERSION_ID << dec
               << " (tag: " << VERSION << ")" << endl;
          exit(0);

        case 'h':
          usage();
          exit(0);

        default:
          usage();
          exit(1);
        }
      }
    catch (std::invalid_argument& e) { arg_error(opt); }
    catch(const std::exception& e)
      {
        std::cerr << e.what() << '\n';
        exit(1);
      }
  }

  // Parse positional arguments
  int pos = 0;
  std::string arg;
  for (opt = optind; opt < argc; opt++) {
    arg = std::string(argv[opt]);
    std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
    switch (pos) {
    case 0:
      // Determine which action to take
      if (arg == "set") {
        action = Action::Set;
      } else if (arg == "enable") {
        action = Action::Enable;
      } else if (arg == "list") {
        action = Action::List;
      } else if (arg == "disable") {
        action = Action::Disable;
      } else if (arg == "trickle") {
        action = Action::Trickle;
      } else {
        unknown_action(arg);
      }
      break;

    case 1:
      switch (action) {
      case Action::Trickle:
        if (arg == "write") {
          action = Action::TrickleWrite;
        } else if (arg == "run") {
          action = Action::TrickleRun;
        } else if (arg == "single") {
          action = Action::TrickleSingle;
        } else if (arg == "stop") {
          action = Action::TrickleStop;
        } else {
          unknown_action(arg);
        }
        break;

      case Action::Set:
        set_argument = std::string(argv[opt]);
        parse_strips_parameter_assignment(set_argument);
        break;

      default:
        too_many_arguments(arg);
      }
      break;

    case 2:
      if (action == Action::TrickleWrite) {
        file_name = std::string(argv[opt]);
      } else {
        too_many_arguments(arg);
      }
      break;

    default:
      too_many_arguments(arg);
    }
    pos += 1; // position of the positional argument after the flags
  }

  // check if all needed arguments were provided
  // perform actions that don't require strips communication
  switch (action) {
  case Action::List:
    FlxStrips::listParameters();
    exit(0);
    break;
  case Action::Enable:
    require_gtx_link();
    break;
  case Action::Disable:
    require_gtx_link();
    break;        
  case Action::Set:
    require_link_ids();
    require_strips_parameters();
    break;
  case Action::None:
    usage();
    exit(1);
  case Action::TrickleRun:
    require_link_ids();
    break;
  case Action::TrickleStop:
    require_link_ids();
    break;
  case Action::TrickleSingle:
    require_link_ids();
    break;
  case Action::TrickleWrite:
    require_link_ids();
    if (file_name.empty()) {
      cerr << "### File name containing trickle memory data is not provided" << endl;
      exit(1);
    }
    break;
  case Action::Trickle:
    cerr << "### Trickle action is not specified" << endl;
    cerr << "### Example: trickle write" << endl;
    exit(1);
  default:
    cerr << "### Action is not specified completely" << endl;
    exit(1);
  }
}

// ----------------------------------------------------------------------------
