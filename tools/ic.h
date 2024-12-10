
bool writeIcRegs   ( FlxUpload *fup, FlxReceiver *frecvr,
                     int linknr, int i2c_addr,
                     int reg_addr, int nbytes, uint8_t *reg_vals,
                     bool use_ec, bool lpgbt_v1,
                     bool debug = false, bool display = true );
bool readIcRegs    ( FlxUpload *fup, FlxReceiver *frecvr,
                     int linknr, int i2c_addr,
                     int reg_addr, int nbytes, uint8_t *reg_vals,
                     bool use_ec, bool lpgbt_v1,
                     bool debug = false, bool display = true );
bool receiveIcReply( FlxReceiver *frecvr, int linknr,
                     int reg_addr, int nbytes, uint8_t *reg_vals,
                     bool use_ec, bool lpgbt_v1,
                     bool debug = false, bool display = true );
