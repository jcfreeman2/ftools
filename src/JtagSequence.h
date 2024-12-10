/* -------------------------------------------------------------------------
File   : JtagSequence.h

Descr  : Class containing JTAG instruction and data strings (usually in pairs)
         typically read from a file.
         The file is a text file containing lines with the following content:
         "I", "i', "D" or "d", followed by the number of bits, followed by a series
         of hexadecimal byte values (matching the given number of bits).
         Empty lines or lines starting with "#", containing comment,
         are allowed.

History: 03APR18; Created.
         03JAN20; Renamed from JtagFile to JtagSequence;
                  throws std::runtime_error.
---------------------------------------------------------------------------- */

#ifndef JTAGSEQUENCE_H
#define JTAGSEQUENCE_H

#include <string>
#include <vector>

// ----------------------------------------------------------------------------

class JtagSequence
{
 public:

  JtagSequence();
  ~JtagSequence();

  unsigned int   stringCount  ( ) { return _jtagStrings.size(); }
  bool           isInstruction( int i );
  bool           readTdi      ( int i );
  int            bitCount     ( int i );
  unsigned char *bits         ( int i, bool reversed = false );

  void setFile                ( std::string filename );
  std::string fileName        ( ) { return _fileName; }
  bool valid                  ( ) { return _valid; }

  void displayInfo            ( bool debug = false );

 private:
  bool        _valid;
  std::string _fileName;

  typedef struct jtagstring
  {
    bool instr;
    bool read_tdi;
    bool reversed;
    int  nbits;
    vector<unsigned char> data;
  } jtagstr_t;

  std::vector<jtagstr_t> _jtagStrings;

  unsigned char _bitReversedMap[256];
};

// ----------------------------------------------------------------------------
#endif // JTAGSEQUENCE_H

