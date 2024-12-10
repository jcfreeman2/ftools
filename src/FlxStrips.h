#ifndef FLXSTRIPS_H
#define FLXSTRIPS_H

#include <string>
#include <memory>
#include <map>
#include "FlxUpload.h"

using namespace std;

class FlxStrips
{    
    public:

    FlxStrips(int cardnr, int gbtnr, int segment, int dma_write);
    ~FlxStrips();

    // Issue a single trickle trigger
    void trickleSingle();

    // Issue continuous trickle trigger
    void trickleRun();

    // Stop trickle trigger
    void trickleStop();

    // Save the file to trickle memory
    void trickleWrite(const std::string& filename, bool isBinary=false);

    enum Destination {LCB, R3L1, None};

    // address and destination of a strips encoder parameter
    typedef pair<uint8_t, Destination> strips_param_t;

    // find encoder parameter by name and return the address
    // and the destination (LCB or R3L1)
    static strips_param_t findParameter(string name);

    // Set LCB or R3L1 encoder parameter and send out immediately
    void writeParameter(string name, int value, bool display=true);

    // enable all elinks on a GBT channel
    void EnableElinks();

    // disable all elinks on a GBT channel
    void DisableElinks();

    // list all available strips encoder parameters
    static void listParameters();

    // ===================== Private methods =========================

    private:
    unique_ptr<FlxUpload> fupload;
    unique_ptr<FlxCard> flx;
    flxcard_bar2_regs_t* bar2;
    int cardnr, gbtnr, segment, dma_write;

    // print LCB- or R3L1- specific parameters
    static void listParametersHelper(map<string, uint8_t> map);

    protected:
        // List of all valid LCB parameters
    static map<string, uint8_t> strips_lcb_params;

    // List of all valid R3L1 parameters
    static map<string, uint8_t> strips_r3l1_params;

    // Stave channel mapping
    enum Channel {
        LCB_Config = 0,
        LCB_Command = 1,
        LCB_Trickle = 2,
        R3L1_Config = 3,
        R3L1_Command = 4,
        Count = 5
    };

    // how many segments each strips stave has
    static const int SEGMENTS_PER_CHANNEL = 4;

    // maximum theoretical number of FromHost elinks per lpGBT link
    static const int ELINKS_PER_LPGBT = 0x40;

    // length of the LCB_Configure or R3L1_Configure command in bytes
    static const int CONFIG_MSG_BYTES = 4;

    // buffer size for forming commands
    static const int BUFFER_SIZE = 1024;

    // opcode of the LCB_Configure or R3L1_Configure command
    static const uint8_t CONFIG_CMD = 0x10;

    // buffer for forming Configure commands
    unique_ptr<uint8_t[]> buffer;

    // return elink ID based on channel, segment, and lpGBT ID
    int getElink(Channel);

    // return elink id based on Destination (LCB or R3L1 config)
    int getElink(Destination);

    // write configuration command into the buffer
    void writeConfigCommand(uint8_t address, uint16_t data, uint8_t buf[]);

    // Set LCB or R3L1 encoder parameter
    strips_param_t setParameter(string name, int value, int offset=0);

    // enable or disable all elinks on a GBT channel
    void setElinks(bool enable);

    // wait for upload to finish
    void upload(int timeout_sec = 3);
};

#endif // FLXSTRIPS_H
