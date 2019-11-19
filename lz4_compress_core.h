#include "hls_stream.h"
#include <ap_int.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define PARALLEL_BLOCK 1
#include "lz4_compress.h"
#include "lz_compress.h"
#include "lz_optional.h"

#define BIT 8
#define MAX_LIT_COUNT 4096
const int c_lz4MaxLiteralCount = MAX_LIT_COUNT;

typedef ap_uint<32> compressd_dt;
typedef ap_uint<64> lz4_compressd_dt;
typedef ap_uint<BIT> uintV_t;

void  lz4top(char* In, char* Out);