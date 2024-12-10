#include <string.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <limits>
#include <unistd.h>
#include "FlxStrips.h"

using namespace std;


FlxStrips::FlxStrips(int cardnr, int gbtnr, int segment, int dma_write) :
cardnr(cardnr), gbtnr(gbtnr), segment(segment), dma_write(dma_write)
{
    flx = make_unique<FlxCard>();
    flx->card_open(cardnr, LOCK_NONE);
    u64 bar2_addr = flx->bar2Address();
    bar2 = (flxcard_bar2_regs_t *) bar2_addr;
    if (bar2->REG_MAP_VERSION < 0x500) {
        flx->card_close();
        throw runtime_error("Firmware register map version must be greater than rm-5.0");
    }

    buffer = make_unique<uint8_t[]>(BUFFER_SIZE);
    fupload = make_unique<FlxUpload>(cardnr, 0, dma_write);
    if (fupload->hasError()) throw runtime_error(fupload->errorString());

    cout << "Opened FLX device " << cardnr << endl;
    cout << "Firmware version " << fupload->firmwareVersion() << endl;

    if (fupload->firmwareMode() != FIRMW_STRIP)
        throw runtime_error("fstrips only works with FELIX_STRIPS firmware version");

    if (fupload->numberOfChans() <= gbtnr)
        throw runtime_error("This firmware doesn't have GBT channel " + to_string(gbtnr));

    if( fupload->fanOutLocked() )
        cout <<  "**NB**: FanOut-Select registers are locked!" << endl;
    fupload->setFanOutForDaq();
}


FlxStrips::~FlxStrips() {
    flx->card_close();
}


void FlxStrips::trickleSingle()
{
    cout << "Issue single trickle trigger on lpGBT#" << gbtnr
        << " segment " << + segment << endl;
    writeParameter("TRICKLE_TRIGGER_PULSE", 1, false);
}


void FlxStrips::trickleRun() {
    cout << "Issue continuous trickle trigger on lpGBT#" << gbtnr
        << " segment " << segment << endl;
    writeParameter("TRICKLE_TRIGGER_RUN", 1, false);
}


void FlxStrips::trickleStop() {
    cout << "Stop sending trickle trigger on lpGBT#" << gbtnr
        << " segment " << + segment << endl;
    writeParameter("TRICKLE_TRIGGER_RUN", 0, false);
}


void FlxStrips::trickleWrite(const std::string& filename, bool isBinary) {
    string format = (isBinary) ? "binary" : "ACSII";
    cout << "Upload file " << filename << " (" << format 
        << ") to trickle configuration memory on lpGBT#" << gbtnr 
        << " segment " << segment << endl;

    // move trickle write pointer to 0
    int i = 0;
    setParameter("TRICKLE_WRITE_ADDR", 0, CONFIG_MSG_BYTES * i++);
    setParameter("TRICKLE_SET_WRITE_ADDR_PULSE", 1, CONFIG_MSG_BYTES * i++);
    fupload->prepareData(getElink(Channel::LCB_Config), CONFIG_MSG_BYTES * 2, buffer.get());    
    upload();

    // upload trickle configuration data and retrieve file size in bytes
    int file_size = fupload->prepareData(getElink(Channel::LCB_Trickle), filename, 1, isBinary, false);
    upload();

    // configure trickle data start and stop pointers
    i = 0;
    setParameter("TRICKLE_DATA_START", 0, CONFIG_MSG_BYTES * i++);
    setParameter("TRICKLE_DATA_END", file_size, CONFIG_MSG_BYTES * i++);
    fupload->prepareData(getElink(Channel::LCB_Config), CONFIG_MSG_BYTES * 2, buffer.get());
    upload();
}


void FlxStrips::writeParameter(string name, int value, bool display) {
    if (display) {
        cout << "Set parameter " << name << "=0x";
        cout << std::hex << value;
        cout << " on lpGBT#" << gbtnr << " segment " << segment << endl;
    }    
    auto parameter = setParameter(name, value);
    auto dest = parameter.second;
    fupload->prepareData(getElink(dest), CONFIG_MSG_BYTES, buffer.get());
    upload();
}


FlxStrips::strips_param_t FlxStrips::setParameter(string name, int value, int offset) {
    auto parameter = findParameter(name);
    if (parameter.second == Destination::None)
        throw runtime_error("Strips encoder parameter named \"" + name
            + "\" is not in the list of valid parameters.");

    if (value < numeric_limits<uint16_t>::min() || value > numeric_limits<uint16_t>::max())
        throw runtime_error("Strips parameter value \"" + to_string(value)
            + "\" is out of range for uint16_t type.");
        
    writeConfigCommand(parameter.first, (uint16_t)value, buffer.get() + offset);
    return parameter;
}


void FlxStrips::listParametersHelper(map<string, uint8_t> map) {
    for (auto item = map.begin(); item != map.end(); ++item) {
        std::cout << "    " << item->first << endl;
    }
}


void FlxStrips::listParameters() {
    std::cout << "Valid LCB parameters:" << endl;
    FlxStrips::listParametersHelper(strips_lcb_params);
    std::cout << endl;
    std::cout << "Valid R3L1 parameters:" << endl;
    FlxStrips::listParametersHelper(strips_r3l1_params);
}


pair<uint8_t, FlxStrips::Destination> FlxStrips::findParameter(string name) {
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    auto search_lcb = strips_lcb_params.find(name);
    auto search_r3l1 = strips_r3l1_params.find(name);
    if (search_lcb != strips_lcb_params.end()) {
        return make_pair(strips_lcb_params[name], Destination::LCB);
    } else if (search_r3l1 != strips_r3l1_params.end()) {
        return make_pair(strips_r3l1_params[name], Destination::R3L1);
    } else {
        return make_pair(0, Destination::None);
    }
}


int FlxStrips::getElink(FlxStrips::Channel channel) {
    return channel + segment*Channel::Count + gbtnr * ELINKS_PER_LPGBT;
}


int FlxStrips::getElink(FlxStrips::Destination dest) {
    switch (dest) {
        case LCB:
            return getElink(Channel::LCB_Config);
        case R3L1:
            return getElink(Channel::R3L1_Config);
        default:
            throw runtime_error("Destination is not provided (LCB or R3L1)");
    }
}


void FlxStrips::writeConfigCommand(uint8_t address, uint16_t data, uint8_t buf[]) {
    buf[0] = CONFIG_CMD;
    buf[1] = (uint8_t)((data >> 8) & 0xFF);
    buf[2] = (uint8_t)(data & 0xFF);
    buf[3] = address;
}


void FlxStrips::setElinks(bool enable) {    
    #if REGMAP_VERSION >= 0x500    
        int data = (enable) ? 0xFF : 0x00;
        auto egroups = bar2->ENCODING_EGROUP_CTRL_GEN[gbtnr].ENCODING_EGROUP;
        int imax = sizeof(bar2->ENCODING_EGROUP_CTRL_GEN[gbtnr].ENCODING_EGROUP)
            / sizeof(bar2->ENCODING_EGROUP_CTRL_GEN[gbtnr].ENCODING_EGROUP[0]);
        for (int i = 0; i < imax; i++) {
            egroups[i].ENCODING_EGROUP_CTRL.EPATH_ENA = data;
        }        
    #endif
}

void FlxStrips::EnableElinks() {
    cout << "Enable Strips elinks on lpGBT#" + to_string(gbtnr) << endl;
    setElinks(true);
}

void FlxStrips::DisableElinks() {
    cout << "Disable Strips elinks on lpGBT#" + to_string(gbtnr) << endl;
    setElinks(false);
}

void FlxStrips::upload(int timeout_sec) {    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t sec_max = ts.tv_sec + timeout_sec;    

    fupload->upload();
    while (!fupload->uploadFinished()) {
        fupload->upload();
        clock_gettime(CLOCK_REALTIME, &ts);
        if (ts.tv_sec >= sec_max) {
            cout << "Upload timed out." << endl;
            break;
        }        
    }
}


map<string, uint8_t> FlxStrips::strips_lcb_params = {
    {"LCB_FRAME_PHASE", 0x00},
    {"L0A_FRAME_DELAY", 0x01},
    {"TTC_L0A_ENABLE", 0x02},
    {"TTC_BCR_DELAY", 0x03},
    {"GATING_TTC_ENABLE", 0x04},
    {"GATING_BC_START", 0x05},
    {"GATING_BC_STOP", 0x06},
    {"TRICKLE_TRIGGER_PULSE", 0x07},
    {"TRICKLE_TRIGGER_RUN", 0x08},
    {"TRICKLE_DATA_START", 0x09},
    {"TRICKLE_DATA_END", 0x0A},
    {"TRICKLE_WRITE_ADDR", 0x0B},
    {"TRICKLE_SET_WRITE_ADDR_PULSE", 0x0C},
    {"ENCODING_ENABLE", 0x0D},
    {"HCC_MASK", 0x0E},
    {"ABC_MASK_0", 0x0F},
    {"ABC_MASK_1", 0x10},
    {"ABC_MASK_2", 0x11},
    {"ABC_MASK_3", 0x12},
    {"ABC_MASK_4", 0x13},
    {"ABC_MASK_5", 0x14},
    {"ABC_MASK_6", 0x15},
    {"ABC_MASK_7", 0x16},
    {"ABC_MASK_8", 0x17},
    {"ABC_MASK_9", 0x18},
    {"ABC_MASK_A", 0x19},
    {"ABC_MASK_B", 0x1A},
    {"ABC_MASK_C", 0x1B},
    {"ABC_MASK_D", 0x1C},
    {"ABC_MASK_E", 0x1D},
    {"ABC_MASK_F", 0x1E}
};


map<string, uint8_t> FlxStrips::strips_r3l1_params = {
    {"R3L1_FRAME_PHASE", 0x00},
    {"L1_ENABLE", 0x01},
    {"R3_ENABLE", 0x02}
};
