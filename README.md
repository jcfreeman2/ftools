## ftools toolset

The _ftools_ repository consists of a set of tools to do dedicated operations on FELIX devices or on connected front-end
electronics (for example a GBT-SCA),
to demonstrate various operations and to help users debug issues with FELIX and/or their front-end electronics and data.

For more detailed information about the various tools see the corresponding chapter in the FELIX user manual:
 https://atlas-project-felix.web.cern.ch/atlas-project-felix/user/felix-user-manual/versions/Latest/6_basic_tools.html#sec:basictools

The manual describes the tools deemed suitable for 'normal' users,
but there are some more as shown in the list below,
which were developed for FELIX developers only or for very specific purposes.

To see possible options and a short description of any tool just run the tool with option -h.

Maintained by Henk Boterenbrood, Nikhef, boterenbrood@nikhef.nl.

### List of Tools

Here's a list of the tools contained in the _ftools_ toolset, in alphabetical order:

| Tool | Description |
|:----------------|:-------------|
| fcap       | display FELIX firmware configuration settings |
| fcheck     | check/dump the content of an fdaq-generated data file |
| fdaq       | write a FELIX data stream to a file |
| fdaqm      | write multiple FELIX data streams to files | 
| fec        | demo using various GBT-SCA I/O channels |
| feconf     | configure the FLX card with a configuration file's contents |
| fedump     | dump received FELIX data directly to screen |
| felink     | convert e-link to GBT/group/path number and viceversa |
| femu       | control the onboard FELIX emulators |
| femuran    | configure FELIG or FMEMU firmware emulator data |
| fereverse  | enable/disable the reversal of the in/out bits for specified e-links |
| feto       | enable/disable the instantaneous timeout for specified e-links |
| fexoff     | enable/disable the FLX device FULL mode firmware GBT XOFF feature |
| fexofftx   | generate XOFF/XON in FLX device FULL mode firmware |
| ffile      | template code to read and process an fdaq file |
| fflash     | program a firmware image in an FLX card flash memory |
| fflashprog | load selected firmware image from flash into the FLX card |
| fgbtxconf  | read and write GBTX registers, via a GBT-SCA I2C port |
| fgpolarity | configure FELIX GBT TX/RX transceiver polarity |
| fice       | read and write GBTX registers, via the GBT IC channel |
| fpepo      | address-based FLX device register read and write (in BAR0, BAR1 or BAR2) |
| fplayback  | upload fdaq file contents via GBT links in loopback |
| frecv      | template code for low-level local receiving and processing FELIX data |
| fscaadc    | operate a GBT-SCA ADC channel |
| fscadac    | operate a GBT-SCA DAC channel |
| fscads24   | read a DS24xx 64-bit ID via GBT-SCA GPIO pin |
| fscai2c    | operate an GBT-SCA I2C channel |
| fscai2c-io | demo tool to operate an I2C interface via GBT-SCA GPIO bitbanging |
| fscaid     | read and display a GBT-SCA identifier |
| fscaio     | operate a GBT-SCA GPIO channel |
| fscajfile  | execute SHIFT-DR and SHIFT-IR operations from file on a GBT-SCA JTAG port |
| fscajtag   | program a bitfile into a (Xilinx) FPGA connected to a GBT-SCA JTAG port |
| fscareply  | parse string of bytes as GBT-SCA reply or request |
| fttcbusy   | show and configure FELIX TTC BUSY related settings |
| fttcemu    | control the FELIX TTC emulator |
| fupload    | uploads test data or data from file to e-links |
| fuptest    | FELIX test with links in loopback |
| fxvcserver | interfaces Vivado to Xilinx FPGA(s) in a GBT-SCA JTAG chain |
