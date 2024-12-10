## lpGBT register map

Files *lpgbt_register_map_v0.py* and *lpgbt_register_map_v1.py*
contain register and register bit field descriptions of the lpGBT version 0
and version 1 respectively, as python classes.

With _awk_ script *lpgbt-items.awk* a C++ structure list is generated from the python file
containing entries for each register and bit field including name, address, number of bits,
bit position in the register, read/write capability, and a one-line description.
This allows for the development of (C++) software tools to access lpGBT registers
on the basis of names (as a text string) of bit fields or registers
(similar to the names found in the lpGBT manual).
File *lpgbt-items-v0.h* contains the list generated for the lpGBT v0,
file *lpgbt-items-v1.h* for the lpGBT v1,

With _awk_ script *lpgbt-regs.awk* a list of register address C++ definitions is generated
from the python file, aimed at use in development of tools for lpGBT access.
File *lpgbt-regs-v0.h* contains the list generated for the lpGBT v0,
file *lpgbt-regs-v1.h* for the lpGBT v1,

The python files have been obtained, courtesy of Stefan Biereigel from the CERN lpGBT project,
from their lpGBT control library project found here:

https://gitlab.cern.ch/lpgbt/lpgbt_control_lib

Scripts and include files in this directory maintained by Henk Boterenbrood, Nikhef, boterenbrood@nikhef.nl.

### List of Tools

Here's a list of currently available (f)tools for lpGBT (v0 or v1 selectable) device access,
via a FELIX device IC channel, or via an lpGBT's EC channel to a secondary (TX-only) lpGBT:

| Tool | Description |
|:----------------|:-------------|
| fice       | read and write GBTX or lpGBT registers, address-based |
| flpgbtconf | read and write individual lpGBT registers or bit fields, address- or name-based; optionally list all |
| flpgbtio   | read and write lpGBT GPIO pins and GPIO configuration registers |
