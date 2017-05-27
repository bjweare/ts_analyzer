#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "prj_common.h"
#include "log.h"
#include "ts_parse.h"

TS_DEMUXER_S demuxer;

int main(int argc, char *argv[])
{
	int32 ret;
	int8 strFilePath[256];
	FILE *fp = NULL;
	FILE *fpPts = NULL;
	FILE *fpPcr = NULL;
	//FILE *fpTsi = NULL;
	int32 fileLen = -1;

	uint8 tsPacket[188];
	uint8 packetSize;
	uint32 firstSyncByteOffset;
	uint32 totalPacketCnt = 0;

	STREAM_FILE_INFO_S streamInfo;
	uint32 averageBitRate = 0;
	float duration = 0;
	uint32 curPacketNo = 0;

/* for pcr jitter calculation */
/********************start**********************/
	int8 foundFirstPCR = 0;

	uint32 curBitRate = 0;
	uint32 curAverageBitRate = 0;
	int32 deltaBitRate = 0;

	uint32 curPcrNo = 0;
	uint32 errBitRateCnt=0;
	
	uint64 firstPcr = 0;
	uint64 curPcr = 0;
	uint64 lastPcr = 0;
	int32 pcrJitter = 0;
	uint64 curStreamSize = 0;
	uint64 totalStreamSize = 0;

	uint32 firstPcrPacketNo = 0;
	uint32 lastPcrPacketNo = 0;
/********************end**********************/

	memset(&streamInfo, 0x00, sizeof(STREAM_FILE_INFO_S));

	if(2 != argc)
	{
		printf("Usage: %s filepath\n", argv[0]);
		return PROG_FAILURE;
	}

	strcpy(strFilePath, argv[1]);
	log_open(strFilePath, strlen(strFilePath), "pcr", &fpPcr);
	log_open(strFilePath, strlen(strFilePath), "pts", &fpPts);
	//log_open(strFilePath, strlen(strFilePath), "tsi", &fpTsi);

	fp = fopen(strFilePath, "r");
	if(NULL == fp)
	{
		PROG_PRINT(PROG_ERR, "open <%s> failed!", strFilePath); return
			PROG_FAILURE;
	}
	
	TSParse_GetStreamFileInfo(fp, &streamInfo);
	averageBitRate = streamInfo.averageBitRate;
	duration = streamInfo.duration;
	packetSize = streamInfo.packetSize;
	firstSyncByteOffset = streamInfo.firstSyncByteOffset;
	totalPacketCnt = streamInfo.totalPacketCnt;
	fileLen = streamInfo.fileLen;

	PROG_PRINT(PROG_INFO, "file path: %s", strFilePath);
	PROG_PRINT(PROG_INFO, "the average bitRate of ts file: %u bit/s = %.2f Mbit/s", averageBitRate, 1.0*averageBitRate/1000/1000);
	PROG_PRINT(PROG_INFO, "duration: %.2f s = %d min %d s", duration, ((uint32)duration)/60, ((uint32)duration)%60);
	PROG_PRINT(PROG_INFO, "ts packet size: %d", packetSize);
	PROG_PRINT(PROG_INFO, "ts first sync byte offset: %d", firstSyncByteOffset);
	PROG_PRINT(PROG_INFO, "ts file size: %d Bytes = %.2f MBytes, totalTsPacket: %u", fileLen, 1.0*fileLen/1024/1024, totalPacketCnt);

	demuxer.patSection = (PAT_SECTION_S *)malloc(sizeof(PAT_SECTION_S));
	if(NULL == demuxer.patSection)
	{
		PROG_PRINT(PROG_ERR, "failed to malloc memory!");
		return PROG_FAILURE;
	}

	TS_PACKET_HEADER_S tsHeader;
	PAT_SECTION_S *patSection;
	PMT_SECTION_S pmtSection;
	PES_PACKET_HEADER_S pesHeader;

	patSection = demuxer.patSection;

	memset(&tsHeader, 0x00, sizeof(TS_PACKET_HEADER_S));
	memset(patSection, 0x00, sizeof(PAT_SECTION_S));
	memset(&pmtSection, 0x00, sizeof(PMT_SECTION_S));
	memset(&pesHeader, 0x00, sizeof(PES_PACKET_HEADER_S));
	memset(&demuxer, 0x00, sizeof(TS_DEMUXER_S));

	uint32 i = 0;
	uint8 getFirstPcr = PROG_FALSE;
	uint64 firstPcrBase = 0;
	uint64 calCurPcr = 0;
	uint64 calCurPcrBase = 0;
	int64 deltaPcrBase = 0;
	uint32 firstPcrPktNo = 0;

	fseek(fp, firstSyncByteOffset, SEEK_SET);
	log_write(fpPts, "PktNo\tPID\tcalCurPcrBase\tPTS\tdeltaPcrBase\n");
	log_write(fpPcr, "packetNoDelta\tcurPcrNo\tcurPcr\tcurBitRate\tcurAverageBitRate\tdeltaBitRate(Kbit/S)\tpcrJitter\n");
	while(1)
	{
		ret = fread(tsPacket, packetSize, 1, fp);
		if(ret != packetSize)
		{
			if(0 == ret)
			{
				PROG_PRINT(PROG_ERR, "%s", strerror(errno));
				break;
			}
		}

		memset(&tsHeader, 0x00, sizeof(TS_PACKET_HEADER_S));
		ret = TSParse_TSPacketHeader(tsPacket, packetSize, &tsHeader);
		if(PROG_SUCCESS != ret)
		{
			PROG_PRINT(PROG_ERR, "called TSParse_TSPacketHeader failed: %d.", ret);
			return ret;
		}
		//dump_TSPacketHeaderInfo(fpTsi, &tsHeader);

		if((0 == patSection->progNum) && (PAT_PID == tsHeader.PID)
				&& (PROG_TRUE == tsHeader.payload_unit_start_indicator))
		{
			ret = TSParse_PATSection(&tsPacket[tsHeader.headerLength], packetSize, patSection);
			if(PROG_SUCCESS != ret)
			{
				PROG_PRINT(PROG_ERR, "called TSParse_PATSection failed: %d.", ret);
				return ret;
			}
			//dump_PATSectionInfo(fpTsi, patSection);
		}

		// TODO: multiple pmt compatible
		if((0 == pmtSection.esNumber) && (patSection->progNum > 0)
				&& (tsHeader.PID == patSection->pmtInfo[0].program_map_PID))
		{
			ret = TSParse_PMTSection(&tsPacket[tsHeader.headerLength], packetSize, &pmtSection);
			if(PROG_SUCCESS != ret)
			{
				PROG_PRINT(PROG_ERR, "called TSParse_PMTSection failed: %d.", ret);
				return ret;
			}
			//dump_PMTSectionInfo(fpTsi, &pmtSection);
			for(i = 0; i < pmtSection.esNumber; i++)
			{
				printf("esPID: %#x, stream_type: %#.2x\n", pmtSection.esInfo[i].elementary_PID, pmtSection.esInfo[i].stream_type);
				dump_StreamTypeInfo(pmtSection.esInfo[i].stream_type);
			}
		}

		if((pmtSection.esNumber > 0) && (tsHeader.PID == pmtSection.PCR_PID)
				&& (PROG_TRUE == ((tsHeader.adaptation_field_control >> 1) & 0x01)) 
				&& (tsHeader.adpt_field.PCR_flag))
		{
			if(PROG_FALSE == getFirstPcr)
			{
				firstPcrBase = tsHeader.adpt_field.pcr_base;
				firstPcrPktNo = curPacketNo;
				getFirstPcr = PROG_TRUE;
				//printf("pcr_base: %llu\n", tsHeader.adpt_field.pcr_base);
			}
		}

		if((pmtSection.esNumber > 0)
				&& (tsHeader.PID == pmtSection.esInfo[0].elementary_PID || tsHeader.PID == pmtSection.esInfo[1].elementary_PID)
				&& (PROG_TRUE == tsHeader.payload_unit_start_indicator))
		{
			memset(&pesHeader, 0x00, sizeof(PES_PACKET_HEADER_S));
			ret = TSParse_PESPacketHeader(&tsPacket[tsHeader.headerLength], packetSize, &pesHeader);
			if(PROG_SUCCESS != ret)
			{
				PROG_PRINT(PROG_ERR, "called TSParse_PESPacketHeader failed: %d.", ret);
				return ret;
			}
			//dump_PESPacketHeaderInfo(fpTsi, &pesHeader);
			if(PROG_TRUE == getFirstPcr)
			{
				TSParse_CalculateCurrentPcr(averageBitRate, curPacketNo-firstPcrPktNo, &calCurPcr);
				calCurPcrBase = calCurPcr/300 + firstPcrBase;
				deltaPcrBase = pesHeader.PTS - calCurPcrBase;
			}
			if(PROG_TRUE == ((pesHeader.PTS_DTS_flags >> 1) & 0x01))
			{
				//printf("PID: %#x, calCurPcrBase: \t%llu, PTS: \t%llu, deltaPcrBase: %lld\n", tsHeader.PID, calCurPcrBase, pesHeader.PTS, deltaPcrBase);
				//printf("%u\t%#.4x\t%llu\t%llu\t%llu\t%lld\n", curPacketNo, tsHeader.PID, calCurPcrBase, pesHeader.PTS, pesHeader.DTS, deltaPcrBase);
				log_write(fpPts, "%u\t%#.4x\t%llu\t%llu\t%lld\n", curPacketNo, tsHeader.PID, calCurPcrBase, pesHeader.PTS, deltaPcrBase);
			}
#if 0 // DTS value
			if(PROG_TRUE == (pesHeader.PTS_DTS_flags & 0x01))
			{
				printf(", DTS: %llu\n", pesHeader.DTS);
			}
			printf("\n");
#endif
		}

/* for pcr jitter calculation */
/********************start**********************/
		if(PROG_TRUE == ((tsHeader.adaptation_field_control >> 1) & 0x01)
				&& (tsHeader.adpt_field.PCR_flag))
		{
			curPcr = tsHeader.adpt_field.pcr_base*300 + tsHeader.adpt_field.pcr_ext;

			PROG_PRINT(PROG_DEBUG, "found pcr: %llu", curPcr);
			curPcrNo++;
			if(1 == foundFirstPCR)
			{
#define MIN_DELTA_BITRATE 200*1000 // 200 Kbit/s
				PROG_PRINT(PROG_DEBUG, "lastPcrPacketNo: %u, curPacketNo: %u", lastPcrPacketNo, curPacketNo);
				curStreamSize = (uint64)(curPacketNo-lastPcrPacketNo)*packetSize;
				totalStreamSize = (uint64)(curPacketNo-firstPcrPacketNo)*packetSize;

				/* calculate curBitRate */
				TSParse_GetCurrentBitRate(&curPcr, &lastPcr, curStreamSize, &curBitRate);
				/* calculate curAverageBitRate */
				TSParse_GetCurrentBitRate(&curPcr, &firstPcr, totalStreamSize, &curAverageBitRate);

				deltaBitRate = (int)((int)curBitRate - (int)curAverageBitRate);
				TSParse_CalculatePcrJitter(&curPcr, &lastPcr, curStreamSize, curAverageBitRate, &pcrJitter);
				//if( (pcrJitter > 500) || (pcrJitter < -500) )
				{
					log_write(fpPcr, "%u\t%u\t%llu\t%.4f\t%.4f\t%.4f\t%d\n", curPacketNo-lastPcrPacketNo, curPcrNo, curPcr,
							1.0*curBitRate/1000/1000, 1.0*curAverageBitRate/1000/1000,
							1.0*deltaBitRate/1000, pcrJitter);
				}
				lastPcrPacketNo = curPacketNo; 
				lastPcr = curPcr;

				if( ((deltaBitRate > MIN_DELTA_BITRATE) || (deltaBitRate < (-MIN_DELTA_BITRATE)))
					&& ((pcrJitter > 500) || (pcrJitter < -500)) )
				{
					errBitRateCnt++;
					printf("curPcrNo: %u", curPcrNo);
					printf("\tdelta(bitRate) = curBitRate - curAverageBitRate\n");
					printf("\t%.4f - %.4f = %.4f Kbit/s\n", 1.0*curBitRate/1000,
							1.0*curAverageBitRate/1000, 1.0*deltaBitRate/1000);
					/* ts may lost packet, re-calculate curAverageBitRate and pcr jitter */
					//foundFirstPCR = 0;
					lastPcr = curPcr;
					lastPcrPacketNo = curPacketNo; 
					firstPcr = curPcr;
					firstPcrPacketNo = curPacketNo;
					//curBitRate = 0;
					//curAverageBitRate = 0;
				}
			}
			else
			{
				log_write(fpPcr, "%u\t%u\t%llu\t%.4f\t%.4f\t%.4f\t%d\n", curPacketNo, curPcrNo, curPcr,
						0, 0, 0, 0);
				foundFirstPCR = 1;
				lastPcr = curPcr;
				lastPcrPacketNo = curPacketNo; 
				firstPcr = curPcr;
				firstPcrPacketNo = curPacketNo;
			}
		}
/********************end**********************/

		curPacketNo++;
	}

	free(patSection);
	patSection = NULL;
	demuxer.patSection = NULL;

	log_close(fpPcr);
	log_close(fpPts);
	//log_close(fpTsi);
	
	return 0;
}

