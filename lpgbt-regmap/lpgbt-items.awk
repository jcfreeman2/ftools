#! /usr/bin/awk -f
#
# Compile an array of lpGBT register/bitfield C-structs
# (as defined below under 'BEGIN')
# from a python file from the lpGBT project which describes the lpGBT registers
#

# Remove whitespace and '"' as well
function ltrim_w(s) { sub(/^[ \t\r\n\"]+/, "", s); return s }
function rtrim_w(s) { sub(/[ \t\r\n\"]+$/, "", s); return s }
function trim_w(s) { return rtrim_w(ltrim_w(s)); }

# Remove whitespace and ']', '[' and '"' as well
function ltrim(s) { sub(/^[ \t\r\n\[\"]+/, "", s); return s }
function rtrim(s) { sub(/[ \t\r\n\]\"]+$/, "", s); return s }
function trim(s) { return rtrim(ltrim(s)); }

function hex2int( str )
{
  # Convert a string representing a hexadecimal
  # number into the corresponding integer
  n=0;
  for( j=1; j<=length(str); ++j )
  {
    m=substr(str,j,1);
    if( m=="A" ) m=10;
    if( m=="B" ) m=11;
    if( m=="C" ) m=12;
    if( m=="D" ) m=13;
    if( m=="E" ) m=14;
    if( m=="F" ) m=15;
    n=n*16+m;
  }
  return n;
}

BEGIN {
    print "// *** NB! This is generated code:"
    printf( "// *** awk -v VERSION=%d -f lpgbt-items.awk <lpgbt_register_map_v%d.py >[this-file]\n", VERSION, VERSION )
    #print "// *** or"
    #print "// *** lpgbt-items.awk <lpgbt_register_map_vX.py >[this-file]"
    printf( "#ifndef LPGBTv%d_ITEMS_H\n", VERSION )
    printf( "#define LPGBTv%d_ITEMS_H\n", VERSION )
    print ""
    print "#ifndef lpgbt_item"
    print "#define lpgbt_item"
    print "// Structure describing an lpGBT register or register bitfield"
    print "typedef struct lpgbt_item_s"
    print "{"
    print "  int addr;         // lpGBT register register address"
    print "  const char *name; // Register name, or field name preceeded by register name"
    print "  int bitindex;     // Index of first bit of field"
    print "  int nbits;        // Size of field in bits"
    print "  bool readonly;    // Allowed access: R or R/W"
    print "  const char *desc; // Register or bitfield description"
    print "} lpgbt_item_t;"
    print "#endif"
    print ""
    printf( "const lpgbt_item_t LPGBTv%d_ITEM[] = {\n", VERSION )

    name = ""
    desc = ""
}

$1 == "class" {
    name = substr( $2, 1, length($2)-1 ) # Remove trailing ':'
    desc = ""
}

$1 == "address" {
    # This item is the full register, so make the entry for it
    regaddr  = hex2int( $3 )
    bitindex = 0
    nbits    = 8
    if( regaddr >= 0x140 ) {
        readonly = "true"
    }
    else {
        readonly = "false"
    }
    regname = name
    name = ""

    printf( "  { 0x%03X, \"%s\", %d, %d, %s, \"%s\" },\n",
            regaddr, regname, bitindex, nbits, readonly, desc )
}

substr( $1, 1, 3 ) == "\"\"\"" {
    # Description (only the first line of the .py file descriptions)
    if( desc == "" && $1 != "\"\"\"\"\"\"" )
        desc = trim_w( $0 )
}

$1 == "offset" {
    bitindex = $3
}

$1 == "length" {
    nbits = $3

    # Create an entry for a register bitfield
    printf( "  { 0x%03X, \"%s_%s\", %d, %d, %s, \"%s\" },\n",
            regaddr, regname, name, bitindex, nbits, readonly, desc )
}

END {
    print "  { 0, \"\", 0, 0, false, \"\" }" # The last line of the array
    print "};" # End of the array
    print ""
    printf( "#endif // LPGBTv%d_ITEMS_H\n", VERSION )
}
