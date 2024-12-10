/* -------------------------------------------------------------------------
File   : BitFile.h

Descr  : FPGA bit-file (.bit) class.

History: 03NOV17; Created.
         03JAN20; Throw std::runtime_error.
---------------------------------------------------------------------------- */

#ifndef BITFILE_H
#define BITFILE_H

#include <string>
#include <vector>

// ----------------------------------------------------------------------------

class BitFile
{
 public:

  BitFile();

  ~BitFile();

  unsigned char* data       ( ) { return _data; }
  unsigned int   nbytes     ( ) { return _nbytes; }

  unsigned int   fieldCount ( ) { return _field.size(); }
  int            fieldId    ( unsigned int index );
  int            fieldLength( unsigned int index );
  unsigned char* fieldData  ( unsigned int index );

  void setFile              ( const std::string &filename,
                              bool skip_data = false );
  std::string fileName      ( ) { return _fileName; }
  bool valid                ( ) { return _valid; }

  void reverseBytes         ( );

  void displayInfo          ( bool debug = false );

 private:
  bool        _valid;
  std::string _fileName;

  typedef struct field
  {
    int   id;
    int   length;
    char *data;
  } field_t;

  std::vector<field_t> _field;
  unsigned char       *_data;
  unsigned int         _nbytes;
};

// ----------------------------------------------------------------------------
#endif // BITFILE_H

