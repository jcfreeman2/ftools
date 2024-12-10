#ifndef EMUDATAGENERATOR_H
#define EMUDATAGENERATOR_H

#include <vector>

#include "Encoder8b10b.h"

class EmuDataGenerator
{
 public:
  EmuDataGenerator() : _errorId(0), _chunkCount(0) {}
  ~EmuDataGenerator() {}

  bool generate( uint32_t *emudata,
                 uint32_t  emusize,
                 uint64_t  nbits_word,
                 uint64_t  nbits_index_word,
                 uint32_t  modes_word,
                 uint32_t  maxchunksize_word,
                 uint32_t  req_chunksize,
                 uint32_t  egroup,
                 uint32_t  pattern_id          = 0,
                 uint32_t  idle_chars          = 8,
                 bool      lsb_first           = false,
                 bool      emuram_msb_first    = false,
                 bool      width_proportial_sz = false,
                 bool      random_sz           = false,
                 bool      use_streamid        = false,
                 bool      soc_is_k287         = false,
                 bool      add_busy            = false,
                 bool      omit_one_soc        = false,
                 bool      omit_one_eoc        = false,
                 bool      fei4b_kchars        = false );

  bool generateFm( uint64_t *emudata,
                   uint64_t  emusize,
                   uint32_t  req_chunksize,
                   uint32_t  pattern_id,
                   uint32_t  idle_chars,
                   bool      random_sz,
                   bool      crc_new      = true,
                   bool      use_streamid = false,
                   bool      add_busy     = false,
                   bool      omit_one_soc = false,
                   bool      omit_one_eoc = false,
                   bool      add_crc_err  = false );

  bool generateFmDune( uint64_t *emudata,
                       uint64_t  emusize,
                       uint32_t *req_chunksize,
                       uint32_t  idle_chars );

  int errorId()    { return _errorId; }
  int chunkCount() { return _chunkCount; }

 private:
  uint32_t createChunk( uint32_t *chunk_data,
                        uint32_t  chunk_size,
                        uint32_t  chunk_counter,
                        uint32_t  nbits,
                        uint32_t  pattern_id,
                        uint32_t  e_id,
                        bool      use_streamid = false,
                        bool      omit_soc     = false,
                        bool      omit_eoc     = false );

  uint64_t crc20( uint64_t *data,
                  uint64_t  length,
                  bool      crc_new = true );

 private:
  Encoder8b10b _encoder;

  std::vector<uint32_t> _chunkData;

  int _errorId, _chunkCount;
};

#endif // EMUDATAGENERATOR_H
