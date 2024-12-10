#ifndef ENCODER8B10B_H
#define ENCODER8B10B_H

#include <cstdint>

// Placeholders in the data to be replaced by appropriate control symbols
const uint32_t SOC_CODE   = 0x100;
const uint32_t EOC_CODE   = 0x200;
const uint32_t COMMA_CODE = 0x300;
const uint32_t SOB_CODE   = 0x400;
const uint32_t EOB_CODE   = 0x500;

class Encoder8b10b
{
 public:
  Encoder8b10b();
  ~Encoder8b10b();

  bool encode( uint32_t *data,
               uint32_t  size,
               bool      soc_is_k287  = true,
               bool      fei4b_kchars = false );

  bool decode( uint32_t *data,
               uint32_t size );

  int  errIndex() { return _errIndex; }

  void displayTables();

 private:
  uint32_t encode5b6b( uint32_t data, int *rd );
  uint32_t encode3b4b( uint32_t b3, uint32_t b5, int *rd );
  uint32_t bitsInWord( uint32_t word );

 private:
  int _errIndex;
};

#endif // ENCODER8B10B_H
