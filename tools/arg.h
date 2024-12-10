void usage();

// ----------------------------------------------------------------------------

void arg_error( char opt )
{
  cerr << "### -" << opt << ": error in argument" << endl;
  usage();
  exit( 1 );
}

// ----------------------------------------------------------------------------

void arg_range( char opt, int min, int max )
{
  cerr << "### -" << opt << ": argument not in range ("
       << min << ".." << max << ")" << endl;
  exit( 1 );
}

// ----------------------------------------------------------------------------

void arg_range_hex( char opt, int min, int max )
{
  cerr << "### -" << opt << ": argument not in range ("
       << hex << "0x" << min << "..0x" << max << ")" << dec << endl;
  exit( 1 );
}

// ----------------------------------------------------------------------------

void arg_check_dec( char *str, char opt )
{
  // Check all chars in the string are valid decimal characters
  for( int i=0; str[i]!='\0'; ++i )
    if( str[i] < '0' || str[i] > '9' )
      {
        cerr << "### ";
        if( opt )
          cerr << " -" << opt << ": i";
        else
          cerr << "I";
        cerr << "nvalid char '" << str[i] << "' in decimal value: "
             << str << endl;
        exit( 1 );
      }
}

// ----------------------------------------------------------------------------

void arg_check_hex( char *str, char opt )
{
  // Check all chars in the string are valid hexadecimal characters
  for( int i=0; str[i]!='\0'; ++i )
    {
      char ch = str[i];
      if( !((ch >= '0' && ch <= '9') ||
            (ch >= 'a' && ch <= 'f') ||
            (ch >= 'A' && ch <= 'F')) )
        {
          cerr << "### ";
          if( opt )
            cerr << " -" << opt << ": i";
          else
            cerr << "I";
          cerr << "nvalid char '" << ch << "' in hexadecimal value: 0x"
               << str << endl;
          exit( 1 );
        }
    }
}

// ----------------------------------------------------------------------------
