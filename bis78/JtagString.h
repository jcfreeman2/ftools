#ifndef _JTAGSTRING_H_
#define _JTAGSTRING_H_

typedef struct JTAG_VAL_DESCR
{
  const char *name;
  int  index;
  int  no_of_bits;
} JTAG_VAL_DESCR; 

//-----------------------------------------------------------------------------

class JtagString
{
 public:
  JtagString( int nbits, const unsigned char *str );
  JtagString( int nbits );
  virtual ~JtagString();

  void          setBits( int i_dst, int nbits, int value );
  void          setBits( int i_dst, int nbits,
                         int i_src, unsigned char *bytes );
  void          clearBits( unsigned int i_dst, unsigned int nbits );
  int           bits   ( int index, int nbits ) const;
  int           length () const { return _nbits; };

  unsigned char *string() const { return _bitString; };

  bool          operator!=( const JtagString &s ) const;

  virtual void  show( int len = 0 );

 private:
  unsigned char *_bitString;
  int           _nbits;
};

//-----------------------------------------------------------------------------
#endif // _JTAGSTRING_H_
