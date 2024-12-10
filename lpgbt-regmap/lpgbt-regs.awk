#! /usr/bin/awk -f
#
# Compile C defines for lpGBT registers
# from a python file from the lpGBT project which describes the lpGBT registers
#

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
    print "// *** awk -v VERSION=X -f lpgbt-regs.awk <lpgbt_register_map_vX.py >[this-file]"
    #print "// *** or"
    #print "// *** lpgbt-regs.awk <lpgbt_register_map_vX.py >[this-file]"
    printf( "#ifndef LPGBTv%d_REGS_H\n", VERSION )
    printf( "#define LPGBTv%d_REGS_H\n", VERSION )
    print ""

    name = ""
    prevaddr = -1
}

$1 == "class" {
    name = substr( $2, 1, length($2)-1 ) # Remove trailing ':'
}

$1 == "address" {
    # This item is the full register (not a bitfield), so make an entry
    regaddr = hex2int( $3 )
    if( regaddr != prevaddr + 1 ) {
        printf( "// ### Missing address(es): %X->%X\n", prevaddr+1, regaddr-1 )
    }
    prevaddr = regaddr

    regname = name "_V" VERSION ""
    printf( "#define %-31s  0x%03X\n", regname, regaddr )
}

END {
    print ""
    printf( "#endif // LPGBTv%d_REGS_H\n", VERSION );
}
