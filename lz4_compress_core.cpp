/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "hls_stream.h"
#include <ap_int.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>


#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string>

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

/**
 * @brief Lz4 compression engine.
 *
 * @tparam MIN_OFFSET lowest match distance
 * @tparam MIN_MATCH minimum match length
 * @tparam LZ_MAX_OFFSET_LIMIT maximum offset limit
 * @tparam OFFSET_WINDOW maximum possible distance of the match
 * @tparam BOOSTER_OFFSET_WINDOW maximum distance of match for booster
 * @tparam LZ_DICT_SIZE dictionary size
 * @tparam MAX_MATCH_LEN maximum match length supported
 * @tparam MATCH_LEN match length
 * @tparam MATCH_LEVEL number of levels to check for match
 *
 * @param inStream input hls stream
 * @param lz4Out output hls stream
 * @param lz4Out_eos output end of stream indicator
 * @param lz4OutSize output compressed size
 * @param max_lit_limit maximum literals before match
 * @param input_size input data size
 * @param core_idx engine index
 */
/*
template <int MIN_OFFSET,
          int MIN_MATCH,
          int LZ_MAX_OFFSET_LIMIT,
          int OFFSET_WINDOW,
          int BOOSTER_OFFSET_WINDOW,
          int LZ_DICT_SIZE,
          int MAX_MATCH_LEN,
          int MATCH_LEN,
          int MATCH_LEVEL>
          */

#define MIN_OFFSET  1
#define MIN_MATCH  4
#define LZ_MAX_OFFSET_LIMIT  65536
#define OFFSET_WINDOW  (64*1024)
#define BOOSTER_OFFSET_WINDOW   (16*1024)
#define LZ_HASH_BIT 12
#define LZ_DICT_SIZE  (1<<LZ_HASH_BIT)
#define MAX_MATCH_LEN  255
#define MATCH_LEN  6
#define MATCH_LEVEL  2

void lz4_compress_engine(hls::stream<uintV_t>& inStream,
                         hls::stream<uintV_t>& lz4Out,
                         hls::stream<bool>& lz4Out_eos,
                         hls::stream<uint32_t>& lz4OutSize,
                         uint32_t max_lit_limit[PARALLEL_BLOCK],
                         uint32_t input_size,
                         uint32_t core_idx) {
    uint32_t left_bytes = 64;
    hls::stream<compressd_dt> compressdStream("compressdStream");
    hls::stream<xf::compression::compressd_dt> bestMatchStream("bestMatchStream");
    hls::stream<compressd_dt> boosterStream("boosterStream");
    hls::stream<uint8_t> litOut("litOut");
    hls::stream<lz4_compressd_dt> lenOffsetOut("lenOffsetOut");

#pragma HLS STREAM variable = compressdStream depth = 8
#pragma HLS STREAM variable = bestMatchStream depth = 8
#pragma HLS STREAM variable = boosterStream depth = 8
#pragma HLS STREAM variable = litOut depth = c_lz4MaxLiteralCount
#pragma HLS STREAM variable = lenOffsetOut depth = 32//c_gmemBurstSize
#pragma HLS STREAM variable = lz4Out depth = 1024
#pragma HLS STREAM variable = lz4OutSize depth = 32//c_gmemBurstSize
#pragma HLS STREAM variable = lz4Out_eos depth = 8

#pragma HLS RESOURCE variable = compressdStream core = FIFO_SRL
#pragma HLS RESOURCE variable = boosterStream core = FIFO_SRL
#pragma HLS RESOURCE variable = litOut core = FIFO_SRL
#pragma HLS RESOURCE variable = lenOffsetOut core = FIFO_SRL
#pragma HLS RESOURCE variable = lz4Out core = FIFO_SRL
#pragma HLS RESOURCE variable = lz4OutSize core = FIFO_SRL
#pragma HLS RESOURCE variable = lz4Out_eos core = FIFO_SRL

#pragma HLS dataflow
    xf::compression::lzCompress<MATCH_LEN, MATCH_LEVEL, LZ_DICT_SIZE, BIT, MIN_OFFSET, MIN_MATCH, LZ_MAX_OFFSET_LIMIT>(
        inStream, compressdStream, input_size, left_bytes);
    xf::compression::lzBestMatchFilter<MATCH_LEN, OFFSET_WINDOW>(compressdStream, bestMatchStream, input_size,
                                                                 left_bytes);
    xf::compression::lzBooster<MAX_MATCH_LEN, BOOSTER_OFFSET_WINDOW>(bestMatchStream, boosterStream, input_size,
                                                                     left_bytes);
    xf::compression::lz4Divide<MAX_LIT_COUNT, PARALLEL_BLOCK>(boosterStream, litOut, lenOffsetOut, input_size,
                                                              max_lit_limit, core_idx);
    xf::compression::lz4Compress(litOut, lenOffsetOut, lz4Out, lz4Out_eos, lz4OutSize, input_size);

   // xf::compression::lz4Compress(litOut, lenOffsetOut, input_size);
}




void  lz4top(char* In, char* Out){


#pragma HLS INTERFACE s_axilite port=return
#pragma HLS INTERFACE m_axi depth=65536 port=In offset=slave
#pragma HLS INTERFACE m_axi depth=10000 port=Out offset=slave


	hls::stream<uintV_t> bytestr_in("compressIn");
	hls::stream<uintV_t> bytestr_out("compressOut");




#pragma HLS STREAM variable = bytestr_in depth = 65536
#pragma HLS STREAM variable = bytestr_out depth = 10000

//#pragma HLS RESOURCE variable = bytestr_in core = FIFO_SRL
//#pragma HLS RESOURCE variable = compressOut core = FIFO_SRL


	hls::stream<bool> lz4Out_eos;
	hls::stream<uint32_t> lz4OutSize;
	uint32_t max_lit_limit[PARALLEL_BLOCK];
	uint32_t input_size = 65536;
	uint32_t core_idx;

//#pragma HLS dataflow


	/*
	std::ifstream inputFile;
	std::fstream outputFile;

	// Input file open for input_size
	inputFile.open("pages-1.img", std::ofstream::binary | std::ofstream::in);
	if (!inputFile.is_open()) {
		printf("Cannot open the input file!!\n");
		exit(0);
	}
	inputFile.seekg(0, std::ios::end);
	uint32_t fileSize = inputFile.tellg();
	inputFile.seekg(0, std::ios::beg);
	input_size = fileSize;
//	uint32_t p = fileSize;


*/


	uint32_t p = 65536;



	// Pushing input file into input stream for compression
	int i = 0;
	while (p--) {
		uint8_t x;
		//inputFile.read((char*)&x, 1);
		x = *(In + i); i ++;
		bytestr_in << x;
	}
	//inputFile.close();

	// COMPRESSION CALL
   // lz4CompressEngineRun(bytestr_in, bytestr_out, lz4Out_eos, lz4OutSize, max_lit_limit, input_size, 0);
	lz4_compress_engine(bytestr_in, bytestr_out, lz4Out_eos, lz4OutSize, max_lit_limit, input_size, 0);





	uint32_t outsize;
	outsize = lz4OutSize.read();
	printf("\n------- Compression Ratio: %f-------\n\n", (float)65536 / outsize);



	/*outputFile.open("outputlz4.txt", std::fstream::binary | std::fstream::out);
	if (!outputFile.is_open()) {
		printf("Cannot open the output file!!\n");
		exit(0);
	}

	outputFile << input_size;*/


	bool eos_flag = lz4Out_eos.read();
	int ii = 0;
	while (outsize > 0) {
		while (!eos_flag) {
			uint8_t w = bytestr_out.read();
			eos_flag = lz4Out_eos.read();
			//outputFile.write((char*)&w, 1);
			*(Out + ii)  = w;
			ii ++;
			outsize--;
		}
		if (!eos_flag) outsize = lz4OutSize.read();
	}
	uint8_t w = bytestr_out.read();
	//outputFile.close();




}
