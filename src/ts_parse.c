#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "prj_common.h"
#include "ts_parse.h"

struct stream_type_str_pair
{
	uint8 stream_type;
	const int8 *str;
};

static const struct stream_type_str_pair stStrPair[] = {
	{ 0x01, "MPEG2VIDEO" },
	{ 0x02, "MPEG2VIDEO" },
	{ 0x03, "MP3"        },
	{ 0x04, "MP3"        },
	{ 0x05, "Private Section" },
	{ 0x06, "Private Data" },
	{ 0x0f, "AAC"        },
	{ 0x11, "AAC LATM"   },
	{ 0x10, "MPEG4"      },
	{ 0x15, "METADATA"      },
	{ 0x1b, "H264"       },
	{ 0x1c, "AAC"        },
	{ 0x20, "H264"       },
	{ 0x21, "JPEG2000"   },
	{ 0x24, "HEVC"       },
	{ 0x42, "CAVS"       },
	{ 0x81, "AC3"        },
	{ 0x82, "DTS"        },
	{ 0x83, "TRUEHD"     },
	{ 0x87, "EAC3"       },
	{ 0xd1, "DIRAC"      },
	{ 0xea, "VC1"        },
	{ 0 },
};

static uint8 bitmask[9] = 
{
	0x00,
	0x01, 0x03, 0x07, 0x0f,
	0x1f, 0x3f, 0x7f, 0xff
};

/* 
 * val -- original 8-bit data
 * pos -- the pos of the wanted bit, range: 0 ~ 7
 */
#define GET1BIT(val, pos) ((val >> pos) & 0x01)
/* 
 * val   -- original 8-bit data
 * start -- the pos of the lowest bit you want, range: 0 ~ 7
 * n     -- how many bits you want, range: 1 ~ 8
 */
#define GETBITS(val, start, n) ((val >> (start)) & bitmask[n]) // start -- the pos of the lowest bit you want

/* common interface */
int32 TSParse_DetectPacketSize(const uint8 *packetBuf, uint32 size, uint8 *packetSize)
{
	const uint8 *p = packetBuf;
	uint32 i = 0;
	
	if(5*TS_PACKET_SIZE_MAX > size)
	{
		PROG_PRINT(PROG_ERR, "at least %d bytes should be provided", 5*TS_PACKET_SIZE_MAX);
		return PROG_FAILURE;
	}

	for(i = 0; i < size; i++)
	{
		if(TS_SYNC_BYTE != p[i])
		{
			continue;
		}

		if( p[i + 1 * TS_PACKET_SIZE_188] == TS_SYNC_BYTE &&
				p[i + 2 * TS_PACKET_SIZE_188] == TS_SYNC_BYTE &&
				p[i + 3 * TS_PACKET_SIZE_188] == TS_SYNC_BYTE )
		{
			*packetSize = TS_PACKET_SIZE_188;
			return PROG_SUCCESS;
		}
		else if( p[i + 1 * TS_PACKET_SIZE_192] == TS_SYNC_BYTE &&
				p[i + 2 * TS_PACKET_SIZE_192] == TS_SYNC_BYTE &&
				p[i + 3 * TS_PACKET_SIZE_192] == TS_SYNC_BYTE )
		{
			*packetSize = TS_PACKET_SIZE_192;
			return PROG_SUCCESS;
		}
		else if( p[i + 1 * TS_PACKET_SIZE_204] == TS_SYNC_BYTE &&
				p[i + 2 * TS_PACKET_SIZE_204] == TS_SYNC_BYTE &&
				p[i + 3 * TS_PACKET_SIZE_204] == TS_SYNC_BYTE )
		{
			*packetSize = TS_PACKET_SIZE_204;
			return PROG_SUCCESS;
		}	
	}

	return PROG_FAILURE;
}

int32 TSParse_GetFirstSyncByteOffset(const uint8 *packetBuf, uint32 bufSize,
		uint8 packetSize, uint32 *offset)
{
	const uint8 *p = packetBuf;
	uint8 i;
	
	if(3*TS_PACKET_SIZE_MAX > bufSize)
	{
		PROG_PRINT(PROG_ERR, "at least %d bytes should be provided", 5*TS_PACKET_SIZE_MAX);
		return PROG_FAILURE;
	}

	for(i = 0; i < (bufSize-packetSize); i++)
	{
		if(TS_SYNC_BYTE != p[i])
		{
			continue;
		}
		if(p[i + 1 * packetSize] == TS_SYNC_BYTE)
		{
			*offset = i;
			break;
		}
	}

	return PROG_SUCCESS;
}

/* pcr reference interface */
static int32 TSParse_GetPCR(const uint8 *packet, uint8 packetSize, uint64 *pcr)
{
	const uint8 *p = packet;
	uint64 pcr_base = 0;
	uint16 pcr_ext = 0;

	if(TS_PACKET_SIZE_188 > packetSize)
	{
		PROG_PRINT(PROG_ERR, "wrong packetSize: %u\n", packetSize);
		return PROG_FAILURE;
	}

	if(( p[3]&0x20 ) && /* adaptation */
			( p[5]&0x10 ) && /* PCR flag */
			( p[4] >= 7 ) ) /* adaptation length */
	{
		/* PCR_base - 33bit */
		pcr_base = ( (uint64)p[6] << 25 ) |
			( (uint64)p[7] << 17 ) |
			( (uint64)p[8] << 9 ) |
			( (uint64)p[9] << 1 ) |
			( (uint64)p[10] >> 7 );
		pcr_base &= (0x1fffffffff);

		/* PCR_extension - 9bit */
		pcr_ext = ((uint16)(p[10]&0x01) << 8) |
			(uint16)(p[11]);
		PROG_PRINT(PROG_DEBUG, "pcr_base: %llu, pcr_ext: %u", pcr_base, pcr_ext);
		pcr_ext = pcr_ext % (300);

		/* pcr_base = (stc/300)%2^33 */
		*pcr = pcr_base*300 + pcr_ext;

		return PROG_SUCCESS;
	}

	return PROG_FAILURE;
}

int32 TSParse_GetStreamFileInfo(FILE *fp, STREAM_FILE_INFO_S *streamInfo)
{
	int32 ret;
	int32 fileLen = PROG_FAILURE;

	uint8 tsPacket[188];
	uint8 packetSize;
	uint32 firstSyncByteOffset;
	uint32 totalPacketCnt = 0;
	int8 foundFirstPCR = 0;
	uint32 averageBitRate = 0;
	float duration = 0;
	
	uint64 firstPcr = 0;
	uint64 curPcr = 0;

	uint32 curPacketNo = 0;
	uint32 firstPcrPacketNo = 0;
	uint32 lastPcrPacketNo = 0;

	if((NULL == fp) || (NULL == streamInfo))
	{
		PROG_PRINT(PROG_ERR, "Invalid argument!\n");
		return PROG_FAILURE;
	}

	uint8 *p = (uint8 *)malloc(5*TS_PACKET_SIZE_MAX);
	if(NULL == p)
	{
		PROG_PRINT(PROG_ERR, "failed to malloc memory!");
		return PROG_FAILURE;
	}
	
	ret = fread(p, 5*TS_PACKET_SIZE_MAX, 1, fp);
	ret = TSParse_DetectPacketSize(p, 5*TS_PACKET_SIZE_MAX, &packetSize);
	if(0 != ret)
	{
		PROG_PRINT(PROG_ERR, "failed to get ts packet size");
		return PROG_FAILURE;
	}
	
	ret = TSParse_GetFirstSyncByteOffset(p, 5*TS_PACKET_SIZE_MAX, packetSize, &firstSyncByteOffset);
	if(PROG_SUCCESS != ret)
	{
		PROG_PRINT(PROG_ERR, "failed to get first sync byte");
		return PROG_FAILURE;
	}

	free(p);

	PROG_PRINT(PROG_DEBUG, "ts packet size: %d", packetSize);
	PROG_PRINT(PROG_DEBUG, "ts first sync byte offset: %d", firstSyncByteOffset);
	
	fseek(fp, 0, SEEK_END);
	fileLen = ftell(fp);
	totalPacketCnt = (fileLen-firstSyncByteOffset)/packetSize;
	fseek(fp, 0, SEEK_SET);
	
	PROG_PRINT(PROG_DEBUG, "ts file size: %d, totalTsPacket: %u", fileLen, totalPacketCnt);

	fseek(fp, firstSyncByteOffset, SEEK_SET);
	while(1)
	{
		ret = fread(tsPacket, packetSize, 1, fp);
		if(ret != packetSize)
		{
			if(0 == ret)
			{
				PROG_PRINT(PROG_DEBUG, "%s", strerror(errno));
				break;
			}
		}
		
		ret = TSParse_GetPCR(tsPacket, packetSize, &curPcr);
		if(0 == ret)
		{
			PROG_PRINT(PROG_DEBUG, "found pcr: %llu", curPcr);
			if(1 == foundFirstPCR)
			{ // found the last pcr
				PROG_PRINT(PROG_DEBUG, "lastPcrPacketNo: %d, firstPcrPacketNo: %d", lastPcrPacketNo, firstPcrPacketNo);
				duration = 1.0*(curPcr-firstPcr)/(27*1000*1000); /* unit: microsecond */
				TSParse_GetCurrentBitRate(&curPcr, &firstPcr, (uint64)(lastPcrPacketNo-firstPcrPacketNo)*packetSize, &averageBitRate);
				PROG_PRINT(PROG_DEBUG, "averageBitRate: %u bit/s = %f Kbit/s = %f Mbit/s", averageBitRate,
						(float)averageBitRate/1000, (float)averageBitRate/1000/1000);
				break;
			}

			foundFirstPCR = 1;
			lastPcrPacketNo = totalPacketCnt; //ready for finding the last pcr packet
			firstPcrPacketNo = curPacketNo;
			firstPcr = curPcr;
		}
		
		if(1 == foundFirstPCR)
		{
			//lastPcrPacketNo = totalPacketCnt-(curPacketNo-firstPcrPacketNo+1);
			lastPcrPacketNo--;
			fseek(fp, (firstSyncByteOffset+(lastPcrPacketNo)*packetSize), SEEK_SET); //find the last pcr, find from the last packet of the stream 
		}
		
		curPacketNo++;
	}


	streamInfo->averageBitRate = averageBitRate;
	streamInfo->duration = duration;
	streamInfo->packetSize = packetSize;
	streamInfo->firstSyncByteOffset = firstSyncByteOffset;
	streamInfo->totalPacketCnt = totalPacketCnt;
	streamInfo->fileLen = fileLen;
	
	return PROG_SUCCESS;
}

/* 
 * streamSize: the byte num of the stream, unit: byte
 * bitRate: the bit rate of the input stream, unit: bit/s
 */
int32 TSParse_GetCurrentBitRate(const uint64 *firstPcr, const uint64 *lastPcr,
	   uint64 streamSize, uint32 *bitRate)
{
	int32 deltaTime = 0;

	deltaTime = ((*firstPcr-*lastPcr)) / (27); /* unit: microsecond */
	*bitRate = (uint64)(((float)(streamSize))*8) / (((float)(deltaTime))/1000/1000);

	PROG_PRINT(PROG_DEBUG, "deltaTime: %d ns = %d min %d s", deltaTime, (deltaTime/1000/1000)/60, (deltaTime/1000/1000)%60);
	PROG_PRINT(PROG_DEBUG, "bitRate: %lld bit/s = %f Kbit/s = %f Mbit/s", (uint64)*bitRate, (float)*bitRate/1000, (float)*bitRate/1000/1000);

	return PROG_SUCCESS;
}

/* 
 * transferedBytes: the byte num of the stream, unit: byte
 * bitRate: the bit rate of the input stream, unit: bit/s
 */
int32 TSParse_CalculatePcrJitter(const uint64 *firstPcr, const uint64 *lastPcr,
	   uint64 transferedBytes, uint32 bitRate, int32 *pcrJitter)
{
	uint64 deltaTime1 = 0, deltaTime2 = 0;
	int32 pcrAccuracy;

	deltaTime1 = (uint64)((*firstPcr-*lastPcr)*1000) / (27); /* unit: nanosecond */
	deltaTime2 = (uint64)(((float)transferedBytes*8*1000) / ((float)(bitRate)/1000/1000)); /* unit: nanosecond */

	pcrAccuracy = deltaTime1 - deltaTime2;
	*pcrJitter = pcrAccuracy;

	return PROG_SUCCESS;
}

int32 TSParse_CalculateCurrentPcr(uint32 bitRate, uint64 deltaPacketCnt, uint64 *curPcr)
{
	*curPcr = (uint64)((PCR_CLOCK_IN_HZ*((1.0*TS_PACKET_SIZE_188*deltaPacketCnt+11)*8)/(bitRate)));

	return PROG_SUCCESS;
}

/* ts packet header parse */
int32 TSParse_TSPacketHeader(const uint8 *packet, uint8 packetSize, TS_PACKET_HEADER_S *header)
{
	TS_PACKET_HEADER_S *h = header;
	const uint8 *p = packet;
	uint8 idx = 0;

	h->sync_byte = p[idx];
	idx++;
	if(TS_SYNC_BYTE != h->sync_byte)
	{
		PROG_PRINT(PROG_ERR, "wrong sync_byte: %#.2x", h->sync_byte);
		return PROG_FAILURE;
	}
	h->transport_error_indicator = GET1BIT(p[idx] ,7);
	h->payload_unit_start_indicator = GET1BIT(p[idx] ,6);
	h->transport_priority = GET1BIT(p[idx] ,5);
	h->PID = (uint16)((GETBITS(p[idx], 0, 5) << 8) | (p[idx+1]));
	idx += 2;
	h->transport_scrambling_control = GETBITS(p[idx], 6, 2);
	h->adaptation_field_control = GETBITS(p[idx], 4, 2);
	h->continuity_counter = GETBITS(p[idx], 0, 4);
	idx++;

	h->headerLength = 4;
	if(PROG_TRUE == ((h->adaptation_field_control >> 1) & 0x01)) 
	{ // has adaptation_field()
		ADAPTATION_FIELD_S *af = &(h->adpt_field);
		af->adaptation_field_length = p[idx];
		idx++;
		if(af->adaptation_field_length > 0)
		{ // TODO: parse other field (except PCR field) in adaptation_field()
			af->discontinuity_indicator = GET1BIT(p[idx], 7);
			af->random_access_indicator = GET1BIT(p[idx], 6);
			af->es_priority_indicator = GET1BIT(p[idx], 5);
			af->PCR_flag = GET1BIT(p[idx], 4);
			af->OPCR_flag = GET1BIT(p[idx], 3);
			af->splicing_point_flag = GET1BIT(p[idx], 2);
			af->transport_private_data_flag = GET1BIT(p[idx], 1);
			af->adaptation_field_ext_flag = GET1BIT(p[idx], 0);
			idx++;
			if(PROG_TRUE == af->PCR_flag)
			{
				af->pcr_base = ((uint64)p[idx] << 25)
							| ((uint64)p[idx+1] << 17)
							| ((uint64)p[idx+2] << 9)
							| ((uint64)p[idx+3] << 1)
							| ((uint64)GET1BIT(p[idx+4], 7));
				af->pcr_base = af->pcr_base & (0x1fffffffff);
				idx += 4;
				af->pcr_ext = ((uint16)(GET1BIT(p[idx], 0)) << 8)
							| (uint16)(p[idx+1]);
				idx += 2;

				PROG_PRINT(PROG_DEBUG, "pcr_base: %llu, pcr_ext: %u", af->pcr_base, af->pcr_ext);
			}
		}
		h->headerLength += af->adaptation_field_length + 1;
	}

	return PROG_SUCCESS;
}

int32 TSParse_PATSection(const uint8 *secData, uint8 packetSize, PAT_SECTION_S *section)
{
	PAT_SECTION_S *s = section;
	const uint8 *p = secData;
	uint8 idx = 0;
	uint16 leftSectionLen = 0;
	uint32 i = 0;

	s->pointer_field = p[idx];
	idx++;
	s->table_id = p[idx];
	idx++;
	s->section_syntax_indicator = GET1BIT(p[idx], 7);
	s->bit_0 = GET1BIT(p[idx], 6);
	s->section_length = (GETBITS(p[idx], 0, 4) << 8) | (p[idx+1]);
	idx += 2;
	s->transport_stream_id = (p[idx] << 8) | (p[idx+1]);
	idx += 2;
	s->version_number = GETBITS(p[idx], 1, 5);
	s->current_next_indicator = GET1BIT(p[idx], 0);
	idx++;
	s->section_number = p[idx];
	idx++;
	s->last_section_number = p[idx];
	idx++;

	leftSectionLen = s->section_length - 5;

	s->progNum = 0;
	PAT_PROG_INFO_S *pstProgInfo = NULL;
	pstProgInfo = (PAT_PROG_INFO_S *)malloc(sizeof(PAT_PROG_INFO_S));
	if(NULL == pstProgInfo)
	{
		PROG_PRINT(PROG_ERR, "failed to malloc memory.");
		return PROG_FAILURE;
	}
	for(i = 0; i < ((leftSectionLen - 4)/4); i++)
	{
		pstProgInfo->program_number = (p[idx] << 8) | (p[idx+1]);
		idx += 2;
		if(0 == pstProgInfo->program_number)
		{
			pstProgInfo->network_PID = (GETBITS(p[idx], 0, 5) << 8) | (p[idx+1]);
			memcpy(&s->nitInfo, pstProgInfo, sizeof(PAT_PROG_INFO_S));
			s->hasNIT = 1;
		}
		else
		{
			pstProgInfo->program_map_PID = (GETBITS(p[idx], 0, 5) << 8) | (p[idx+1]);
			memcpy(&s->pmtInfo[s->progNum], pstProgInfo, sizeof(PAT_PROG_INFO_S));
			s->progNum++;
		}
		idx += 2;

		if(MAX_PROG_IN_PAT <= s->progNum)
		{
			PROG_PRINT(PROG_WARN, "program info num: %d, max program info in pat: %d",
					(leftSectionLen - 4)/4, MAX_PROG_IN_PAT);
			return PROG_FAILURE;
		}
	}

	s->CRC_32 = ((uint32)p[idx] << 24) | ((uint32)p[idx+1] << 16)
		| ((uint32)p[idx+2] << 8) | (p[idx+3]);
	idx += 4;

	return PROG_SUCCESS;
}

int32 TSParse_PMTSection(const uint8 *secData, uint8 packetSize, PMT_SECTION_S *section)
{
	PMT_SECTION_S *s = section;
	const uint8 *p = secData;
	uint8 idx = 0;
	short leftSectionLen = 0;

	s->pointer_field = p[idx];
	idx++;
	s->table_id = p[idx];
	idx++;
	s->section_syntax_indicator = GET1BIT(p[idx], 7);
	s->bit_0 = GET1BIT(p[idx], 6);
	s->section_length = (GETBITS(p[idx], 0, 4) << 8) | (p[idx+1]);
	idx += 2;
	s->program_number = (p[idx] << 8) | (p[idx+1]);
	idx += 2;
	s->version_number = GETBITS(p[idx], 1, 5);
	s->current_next_indicator = GET1BIT(p[idx], 0);
	idx++;
	s->section_number = p[idx];
	idx++;
	s->last_section_number = p[idx];
	idx++;

	s->PCR_PID = (uint16)((GETBITS(p[idx], 0, 5) << 8) | (p[idx+1]));
	idx += 2;
	s->program_info_length = (uint16)((GETBITS(p[idx], 0, 4) << 8) | (p[idx+1]));
	idx += 2;

	if(0 != s->program_info_length)
	{
		uint16 len = s->program_info_length;
		s->descriptor = (int8 *)malloc(len);
		if(NULL == s->descriptor)
		{
			PROG_PRINT(PROG_ERR, "failed to malloc memory.");
			return PROG_FAILURE;
		}
		memcpy(s->descriptor, &p[idx], len);
		idx += len;
	}

	leftSectionLen = s->section_length - 9 - (s->program_info_length);
	PROG_PRINT(PROG_DEBUG, "leftSectionLen: %d", leftSectionLen);

	s->esNumber = 0;
	while((leftSectionLen-4) > 0)
	{
		uint32 *no = &s->esNumber;
		PMT_ES_INFO_S *pstEsInfo = NULL;

		pstEsInfo = &s->esInfo[*no];
		pstEsInfo->stream_type = p[idx];
		idx++;
		pstEsInfo->elementary_PID = (GETBITS(p[idx], 0, 5) << 8) | (p[idx+1]);
		idx += 2;
		pstEsInfo->ES_info_length = (uint16)((GETBITS(p[idx], 0, 4) << 8) | (p[idx+1]));
		idx += 2;
		if(0 != pstEsInfo->ES_info_length)
		{
			uint16 len = pstEsInfo->ES_info_length;
			pstEsInfo->descriptor = (int8 *)malloc(len);
			if(NULL == pstEsInfo->descriptor)
			{
				PROG_PRINT(PROG_ERR, "failed to malloc memory.");
				return PROG_FAILURE;
			}
			memcpy(pstEsInfo->descriptor, &p[idx], len);
			idx += len;
		}
		leftSectionLen -= (5 + pstEsInfo->ES_info_length);
		(*no)++;
		if(MAX_ES_IN_PROG <= (*no))
		{
			PROG_PRINT(PROG_WARN, "max es in pmt: %d", MAX_ES_IN_PROG);
			return PROG_FAILURE;
		}
		PROG_PRINT(PROG_DEBUG, "leftSectionLen: %d", leftSectionLen);
	}

	s->CRC_32 = ((uint32)p[idx] << 24) | ((uint32)p[idx+1] << 16)
		| ((uint32)p[idx+2] << 8) | (p[idx+3]);
	idx += 4;

	return PROG_SUCCESS;
}

int32 TSParse_PESPacketHeader(const uint8 *secData, uint8 packetSize, PES_PACKET_HEADER_S *header)
{
	PES_PACKET_HEADER_S *h = header;
	const uint8 *p = secData;
	uint8 idx = 0;
	uint8 streamId = 0;

	if( 0 == p[idx] && 0 == p[idx+1] && 0x01 == p[idx+2] )
	{
		h->packet_start_code_prefix = 0x000001;
		idx += 3;
	}
	else
	{
		PROG_PRINT(PROG_ERR, "can't find the start prefix");
		return PROG_FAILURE;
	}

	h->stream_id = p[idx];
	idx++;
	h->PES_packet_length = (p[idx] << 8) | (p[idx+1]);
	idx += 2;
	
	// TODO: stream_id
	streamId = h->stream_id;
	if( streamId != STREAM_ID_PROG_STREAM_MAP
			&& streamId != STREAM_ID_PADDING_STREAM
			&& streamId != STREAM_ID_PRIV_STREAM_2
			&& streamId != STREAM_ID_ECM_STREAM
			&& streamId != STREAM_ID_EMM_STREAM
			&& streamId != STREAM_ID_PROG_STREAM_DIRECTORY
			&& streamId != STREAM_ID_DSMCC_STREAM
			&& streamId != STREAM_ID_H2221_TYPE_E_STREAM )
	{
		h->bits_10 = GETBITS(p[idx], 6, 2);
		h->PES_scrambling_control = GETBITS(p[idx], 4, 2);
		h->PES_priority = GET1BIT(p[idx], 3);
		h->data_alignment_indicator = GET1BIT(p[idx], 2);
		h->copyright = GET1BIT(p[idx], 1);
		h->original_or_copy = GET1BIT(p[idx], 0);
		idx += 1;
		h->PTS_DTS_flags = GETBITS(p[idx], 6, 2);
		h->ESCR_flag = GET1BIT(p[idx], 5);
		h->ES_rate_flag = GET1BIT(p[idx], 4);
		h->DSM_trick_mode_flag = GET1BIT(p[idx], 3);
		h->additional_copy_info_flag = GET1BIT(p[idx], 2);
		h->PES_CRC_flag = GET1BIT(p[idx], 1);
		h->PES_extension_flag = GET1BIT(p[idx], 0);
		idx++;
		h->PES_header_data_length = p[idx];
		idx++;

		if(0x02 != h->PTS_DTS_flags && 0x03 != h->PTS_DTS_flags)
		{
			PROG_PRINT(PROG_ERR, "wrong PTS_DTS_flags: %#.2x", h->PTS_DTS_flags);
			return PROG_FAILURE;
		}

		if(PROG_TRUE == ((h->PTS_DTS_flags >> 1) & 0x01))
		{
			h->PTS = ((uint64)GETBITS(p[idx], 1, 3) << 30)
				| ((uint64)p[idx+1] << 22)
				| ((uint64)GETBITS(p[idx+2], 1, 7) << 15)
				| ((uint64)p[idx+3] << 7)
				| ((uint64)GETBITS(p[idx+1], 1, 7));

			idx += 5;
		}

		if(PROG_TRUE == (h->PTS_DTS_flags & 0x01))
		{
			h->DTS = ((uint64)GETBITS(p[idx], 1, 3) << 30)
				| ((uint64)p[idx+1] << 22)
				| ((uint64)GETBITS(p[idx+2], 1, 7) << 15)
				| ((uint64)p[idx+3] << 7)
				| ((uint64)GETBITS(p[idx+1], 1, 7));

			idx += 5;
		}
	}
	else if( streamId == STREAM_ID_PROG_STREAM_MAP
			|| streamId == STREAM_ID_PRIV_STREAM_2
			|| streamId == STREAM_ID_ECM_STREAM
			|| streamId == STREAM_ID_EMM_STREAM
			|| streamId == STREAM_ID_PROG_STREAM_DIRECTORY
			|| streamId == STREAM_ID_DSMCC_STREAM
			|| streamId == STREAM_ID_H2221_TYPE_E_STREAM )
	{ // PES_packet_data_byte
		PROG_PRINT(PROG_WARN, "to be continued");
	}
	else if( streamId == STREAM_ID_PADDING_STREAM )
	{ // padding_byte
		PROG_PRINT(PROG_WARN, "to be continued");
	}

	return PROG_SUCCESS;
}

void dump_TSPacketHeaderInfo(FILE *fp, TS_PACKET_HEADER_S *header)
{
	TS_PACKET_HEADER_S *h = header;

	fprintf(fp, "#################TS packet header info##################\n");
	fprintf(fp, "sync_byte: %#x\n", h->sync_byte);
	fprintf(fp, "transport_error_indicator: %d\n", h->transport_error_indicator);
	fprintf(fp, "payload_unit_start_indicator: %d\n", h->payload_unit_start_indicator);
	fprintf(fp, "transport_priority: %d\n", h->transport_priority);
	fprintf(fp, "PID: %#x\n", h->PID);
	fprintf(fp, "transport_scrambling_control: %d\n", h->transport_scrambling_control);
	fprintf(fp, "adaptation_field_control: %d\n", h->adaptation_field_control);
	fprintf(fp, "continuity_counter: %d\n", h->continuity_counter);

	if(PROG_TRUE == ((h->adaptation_field_control >> 1) & 0x01)) 
	{
		ADAPTATION_FIELD_S *af = &(h->adpt_field);
		fprintf(fp, "\tadaptation_field_length: %d\n", af->adaptation_field_length);
		if(af->adaptation_field_length > 0)
		{
			fprintf(fp, "\tdiscontinuity_indicator: %d\n", af->discontinuity_indicator);
			fprintf(fp, "\trandom_access_indicator: %d\n", af->random_access_indicator);
			fprintf(fp, "\tes_priority_indicator: %d\n", af->es_priority_indicator);
			fprintf(fp, "\tPCR_flag: %d\n", af->PCR_flag);
			fprintf(fp, "\tOPCR_flag: %d\n", af->OPCR_flag);
			fprintf(fp, "\tsplicing_point_flag: %d\n", af->splicing_point_flag);
			fprintf(fp, "\ttransport_private_data_flag: %d\n", af->transport_private_data_flag);
			fprintf(fp, "\tadaptation_field_ext_flag: %d\n", af->adaptation_field_ext_flag);
			if(PROG_TRUE == af->PCR_flag)
			{
				fprintf(fp, "\tpcr_base: %llu, pcr_ext: %u\n", af->pcr_base, af->pcr_ext);
			}
		}
	}
	fprintf(fp, "headerLength: %d\n", h->headerLength);
	fprintf(fp, "########################################################\n");

	fflush(fp);
}

void dump_PATSectionInfo(FILE *fp, PAT_SECTION_S *section)
{
	PAT_SECTION_S *s = section;
	PAT_PROG_INFO_S *pstProgInfo = NULL;
	uint8 i = 0;

	fprintf(fp, "#################PAT section header info################\n");
	fprintf(fp, "pointer_field: %d\n", s->pointer_field);
	fprintf(fp, "table_id: %#x\n", s->table_id);
	fprintf(fp, "section_syntax_indicator: %d\n", s->section_syntax_indicator);
	fprintf(fp, "bit_0: %d\n", s->bit_0);
	fprintf(fp, "section_length: %d\n", s->section_length);
	fprintf(fp, "transport_stream_id: %#x\n", s->transport_stream_id);
	fprintf(fp, "version_number: %d\n", s->version_number);
	fprintf(fp, "current_next_indicator: %d\n", s->current_next_indicator);
	fprintf(fp, "section_number: %d\n", s->section_number);
	fprintf(fp, "last_section_number: %d\n", s->last_section_number);

	for(i = 0; i < s->progNum; i++)
	{
		pstProgInfo = &s->pmtInfo[i];
		fprintf(fp, "program_number: %d\n", pstProgInfo->program_number);
		if(0 == pstProgInfo->program_number)
		{
			fprintf(fp, "network_PID: %#x\n", pstProgInfo->network_PID);
		}
		else
		{
			fprintf(fp, "program_map_PID: %#x\n", pstProgInfo->program_map_PID);
		}
	}
	fprintf(fp, "CRC_32: %#x\n", s->CRC_32);
	fprintf(fp, "########################################################\n");

	fflush(fp);
}

void dump_PMTSectionInfo(FILE *fp, PMT_SECTION_S *section)
{
	PMT_SECTION_S *s = section;
	uint8 i = 0;

	fprintf(fp, "#################PMT section header info################\n");
	fprintf(fp, "pointer_field: %d\n", s->pointer_field);
	fprintf(fp, "table_id: %#x\n", s->table_id);
	fprintf(fp, "section_syntax_indicator: %d\n", s->section_syntax_indicator);
	fprintf(fp, "bit_0: %d\n", s->bit_0);
	fprintf(fp, "section_length: %d\n", s->section_length);
	fprintf(fp, "program_number: %#x\n", s->program_number);
	fprintf(fp, "version_number: %d\n", s->version_number);
	fprintf(fp, "current_next_indicator: %d\n", s->current_next_indicator);
	fprintf(fp, "section_number: %d\n", s->section_number);
	fprintf(fp, "last_section_number: %d\n", s->last_section_number);

	fprintf(fp, "PCR_PID: %#x\n", s->PCR_PID);
	fprintf(fp, "program_info_length: %#x\n", s->program_info_length);

	fprintf(fp, "esNumber: %d\n", s->esNumber);
	for(i = 0; i < s->esNumber; i++)
	{
		fprintf(fp, "\tstream_type: %#x\n", s->esInfo[i].stream_type);
		fprintf(fp, "\telementary_PID: %#x\n", s->esInfo[i].elementary_PID);
	}
	fprintf(fp, "CRC_32: %#x\n", s->CRC_32);
	fprintf(fp, "########################################################\n");

	fflush(fp);
}

void dump_PESPacketHeaderInfo(FILE *fp, PES_PACKET_HEADER_S *header)
{
	PES_PACKET_HEADER_S *h = header;

	fprintf(fp, "#################PES section header info################\n");
	fprintf(fp, "packet_start_code_prefix: %#x\n", h->packet_start_code_prefix);
	fprintf(fp, "stream_id: %#x\n", h->stream_id);
	fprintf(fp, "PES_packet_length: %u\n", h->PES_packet_length);
	fprintf(fp, "bits_10: %x\n", h->bits_10);
	fprintf(fp, "PES_scrambling_control: %u\n", h->PES_scrambling_control);
	fprintf(fp, "PES_priority: %u\n", h->PES_priority);
	fprintf(fp, "data_alignment_indicator: %u\n", h->data_alignment_indicator);
	fprintf(fp, "copyright: %u\n", h->copyright);
	fprintf(fp, "original_or_copy: %u\n", h->original_or_copy);
	fprintf(fp, "PTS_DTS_flags: %u\n", h->PTS_DTS_flags);
	fprintf(fp, "ESCR_flag: %u\n", h->ESCR_flag);
	fprintf(fp, "ES_rate_flag: %u\n", h->ES_rate_flag);
	fprintf(fp, "DSM_trick_mode_flag: %u\n", h->DSM_trick_mode_flag);
	fprintf(fp, "additional_copy_info_flag: %u\n", h->additional_copy_info_flag);
	fprintf(fp, "PES_CRC_flag: %u\n", h->PES_CRC_flag);
	fprintf(fp, "PES_extension_flag: %u\n", h->PES_extension_flag);
	fprintf(fp, "PES_header_data_length: %u\n", h->PES_header_data_length);

	if(PROG_TRUE == ((h->PTS_DTS_flags >> 1) & 0x01))
	{
		fprintf(fp, "PTS: %llu\n", h->PTS);
	}
	if(PROG_TRUE == (h->PTS_DTS_flags & 0x01))
	{
		fprintf(fp, "DTS: %llu\n", h->DTS);
	}
	fprintf(fp, "########################################################\n");
}

void dump_StreamTypeInfo(uint8 stream_type)
{
	uint8 i = 0;
	uint8 pairNum = sizeof(stStrPair)/sizeof(struct stream_type_str_pair);

	for(i = 0; i < pairNum; i++)
	{
		if(stream_type == stStrPair[i].stream_type)
			break;
	}

	if(i == pairNum)
	{
		printf("unknown stream type: 0x%.2x\n", stream_type);
	}
	else
	{
		printf("stream type: 0x%.2x -- %s\n", stream_type, stStrPair[i].str);
	}
}

