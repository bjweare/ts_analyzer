#ifndef __TS_PARSE_H__
#define __TS_PARSE_H__

#define PAT_PID 0x0000 // Program Association Table
#define CAT_PID 0x0001 // Conditional Access Table
#define TSDT_PID 0x0002 // Transport Stream Description Table

#define PAT_TID 0x00
#define CAT_TID 0x01
#define TS_PMS_TID 0x02 // TS_program_map_section
#define TS_DS_TID 0x03 // TS_description_section

#define TS_PACKET_SIZE_188 188
#define TS_PACKET_SIZE_192 192
#define TS_PACKET_SIZE_204 204
#define TS_PACKET_SIZE_MAX 204
#define TS_SYNC_BYTE 0x47

#define PCR_CLOCK_IN_HZ (27*1000*1000)

#define MAX_PROG_IN_PAT (256)
#define MAX_ES_IN_PROG (16)
#define MAX_ES_IN_TS (MAX_PROG_IN_PAT*MAX_ES_IN_PROG)

/* 2.4.3.5 Semantic definition of fields in adaptation field */
typedef struct adaptation_field_s
{
	uint8 adaptation_field_length; //         8 bit
	uint8 discontinuity_indicator; //         1 bit
	uint8 random_access_indicator; //         1 bit
	uint8 es_priority_indicator; //           1 bit
	uint8 PCR_flag; //                        1 bit
	uint8 OPCR_flag; //                       1 bit
	uint8 splicing_point_flag; //             1 bit
	uint8 transport_private_data_flag; //     1 bit
	uint8 adaptation_field_ext_flag; //       1 bit
	uint64 pcr_base; //                      33 bit
	//six_reserved_bits //                    6 bit
	uint16 pcr_ext; //                        9 bit
}ADAPTATION_FIELD_S;

/* 2.4.3.2 Transport Stream packet layer */
typedef struct ts_packet_header_s
{
	uint8 sync_byte; //                       8 bit
	uint8 transport_error_indicator; //       1 bit
	uint8 payload_unit_start_indicator; //    1 bit
	uint8 transport_priority; //              1 bit
	uint16 PID; //                           13 bit
	uint8 transport_scrambling_control; //    2 bit
	uint8 adaptation_field_control; //        2 bit
	uint8 continuity_counter; //              4 bit
	ADAPTATION_FIELD_S adpt_field;
	uint8 headerLength; //the offset of payload or the length of ts packet header
}TS_PACKET_HEADER_S;

typedef struct pat_prog_info_s
{
	uint16 program_number; //            16 bit
	//three_reserved_bits //              3 bit
	uint16 program_map_PID; //           13 bit
	uint16 network_PID; //               13 bit
}PAT_PROG_INFO_S;

/* 2.4.4.3 Program association Table */
typedef struct pat_section_s
{
	uint8 pointer_field; //               8 bit /* 2.4.4.1 Pointer */
	uint8 table_id; //                    8 bit
	uint8 section_syntax_indicator; //    1 bit
	uint8 bit_0; //                       1 bit
	//two_reserved_bits //                2 bit
	uint16 section_length; //            12 bit
	uint16 transport_stream_id; //       16 bit
	//two_reserved_bits //                2 bit
	uint8 version_number; //              5 bit
	uint8 current_next_indicator; //      1 bit
	uint8 section_number; //              8 bit
	uint8 last_section_number; //         8 bit
	uint8 progNum;
	PAT_PROG_INFO_S pmtInfo[MAX_PROG_IN_PAT];
	uint8 hasNIT;
	PAT_PROG_INFO_S nitInfo;
	uint32 CRC_32; //                    32 bit
}PAT_SECTION_S;

#define STREAM_TYPE_11172_VIDEO         0x01
#define STREAM_TYPE_13818_VIDEO         0x02
#define STREAM_TYPE_11172_AUDIO         0x03
#define STREAM_TYPE_13818_AUDIO         0x04
#define STREAM_TYPE_14496_2_VIDEO       0x10    // MPEG4
#define STREAM_TYPE_14496_10_VIDEO      0x1B    // H264
#define STREAM_TYPE_AVS_VIDEO           0x42    // AVS
#define STREAM_TYPE_13818_7_AUDIO       0x0F    // AAC
#define STREAM_TYPE_14496_3_AUDIO       0x11    // AAC
#define STREAM_TYPE_AC3_AUDIO           0x81    // AC3
#define STREAM_TYPE_SCTE                0x82    // TS packets containing SCTE data
#define STREAM_TYPE_DTS_AUDIO           0x82    // DTS
#define STREAM_TYPE_DOLBY_TRUEHD_AUDIO  0x83    // dolby true HD

#define STREAM_TYPE_PRIVATE             0x06    // PES packets containing private data

typedef struct pmt_es_info_s
{
	uint8 stream_type; //                 8 bit
	//three_reserved_bits //              3 bit
	uint16 elementary_PID; //            13 bit
	//four_reserved_bits //               4 bit
	uint16 ES_info_length; //            12 bit
	int8 *descriptor; //                8*N bit
}PMT_ES_INFO_S; 

typedef struct pmt_section_s
{
	uint8 pointer_field; //               8 bit /* 2.4.4.1 Pointer */
	uint8 table_id; //                    8 bit
	uint8 section_syntax_indicator; //    1 bit
	uint8 bit_0; //                       1 bit
	//two_reserved_bits //                2 bit
	uint16 section_length; //            12 bit
	uint16 program_number; //            16 bit
	//two_reserved_bits //                2 bit
	uint8 version_number; //              5 bit
	uint8 current_next_indicator; //      1 bit
	uint8 section_number; //              8 bit
	uint8 last_section_number; //         8 bit
	//three_reserved_bits //              3 bit
	uint16 PCR_PID; //                   13 bit
	//four_reserved_bits //               4 bit
	uint16 program_info_length; //       12 bit
	int8 *descriptor; //                8*N bit
	uint32 esNumber; // elementary stream number
	PMT_ES_INFO_S esInfo[MAX_ES_IN_PROG]; // elementary stream info array pointer
	uint32 CRC_32; //                    32 bit
}PMT_SECTION_S;

/* Table 2-18 – Stream_id assignments */
typedef enum stream_id_type_e
{
	STREAM_ID_PROG_STREAM_MAP = 0xbc,
	STREAM_ID_PRIV_STREAM_1 = 0xbd,
	STREAM_ID_PADDING_STREAM = 0xbe,
	STREAM_ID_PRIV_STREAM_2 = 0xbf,
	STREAM_ID_AUDIO_STREAM = 0xc0,
	STREAM_ID_VIDEO_STREAM = 0xe0,
	STREAM_ID_ECM_STREAM = 0xf0,
	STREAM_ID_EMM_STREAM = 0xf1,
	STREAM_ID_DSMCC_STREAM = 0xf2,
	STREAM_ID_13522_STREAM = 0xf3,
	STREAM_ID_H2221_TYPE_A_STREAM = 0xf4,
	STREAM_ID_H2221_TYPE_B_STREAM = 0xf5,
	STREAM_ID_H2221_TYPE_C_STREAM = 0xf6,
	STREAM_ID_H2221_TYPE_D_STREAM = 0xf7,
	STREAM_ID_H2221_TYPE_E_STREAM = 0xf8,
	STREAM_ID_ANCILLARY_STREAM = 0xf9,
	STREAM_ID_14496_1_SL_PACKETIZED_STREAM = 0xfa,
	STREAM_ID_14496_1_FLEXMUX_STREAM = 0xfb,
	STREAM_ID_PROG_STREAM_DIRECTORY = 0xff
}STREAM_ID_TYPE_E;

/* 2.4.3.7 Semantic definition of fields in PES packet */
typedef struct pes_packet_header_s
{
	uint32 packet_start_code_prefix; //  24 bit
	uint8 stream_id; //                   8 bit
	uint16 PES_packet_length; //         16 bit
	uint8 bits_10; //                     2 bit
	uint8 PES_scrambling_control; //      2 bit
	uint8 PES_priority; //                1 bit
	uint8 data_alignment_indicator; //    1 bit
	uint8 copyright; //                   1 bit
	uint8 original_or_copy; //            1 bit
	uint8 PTS_DTS_flags; //               2 bit
	uint8 ESCR_flag; //                   1 bit
	uint8 ES_rate_flag; //                1 bit
	uint8 DSM_trick_mode_flag; //         1 bit
	uint8 additional_copy_info_flag; //   1 bit
	uint8 PES_CRC_flag; //                1 bit
	uint8 PES_extension_flag; //          1 bit
	uint8 PES_header_data_length; //      8 bit
	uint64 PTS; //                       33 bit
	uint64 DTS; //                       33 bit
	uint8 headerLength; //the offset of payload or the length of pes section header
}PES_PACKET_HEADER_S;

typedef struct stream_file_info_s
{
	uint32 averageBitRate; // unit - bit/s
	float duration; // unit - s
	uint8 packetSize; // unit - byte
	uint32 firstSyncByteOffset;
	uint32 totalPacketCnt;
	int32 fileLen;
}STREAM_FILE_INFO_S;

typedef struct es_info_s
{
	uint8 streamType;
	uint16 esPID;
	uint16 descStrLen;
	int8 *descStr;
}ES_INFO_S;

typedef struct ts_demuxer_s
{
	PAT_SECTION_S *patSection;
	PMT_SECTION_S *pmtSection;
	uint32 esNumber; // elementary stream number
	ES_INFO_S *esInfo[MAX_ES_IN_TS]; // elementary stream info array pointer
	uint8 curPmtCnt;
	uint8 curEsCnt;
}TS_DEMUXER_S;

int32 TSParse_DetectPacketSize(const uint8 *packetBuf, uint32 size, uint8 *packetSize);
int32 TSParse_GetFirstSyncByteOffset(const uint8 *packetBuf, uint32 bufSize,
		uint8 packetSize, uint32 *offset);
int32 TSParse_GetStreamFileInfo(FILE *fp, STREAM_FILE_INFO_S *streamInfo);
int32 TSParse_GetCurrentBitRate(const uint64 *firstPcr, const uint64 *lastPcr,
		uint64 streamSize, uint32 *bitRate);
int32 TSParse_CalculatePcrJitter(const uint64 *firstPcr, const uint64 *lastPcr,
		uint64 transferedBytes, uint32 bitRate, int32 *pcrJitter);
int32 TSParse_CalculateCurrentPcr(uint32 bitRate, uint64 deltaPacketCnt, uint64 *curPcr);

void dump_TSPacketHeaderInfo(FILE *fp, TS_PACKET_HEADER_S *header);
int32 TSParse_TSPacketHeader(const uint8 *packet, uint8 packetSize, TS_PACKET_HEADER_S *header);

void dump_PATSectionInfo(FILE *fp, PAT_SECTION_S *section);
int32 TSParse_PATSection(const uint8 *secData, uint8 packetSize, PAT_SECTION_S *section);

void dump_PMTSectionInfo(FILE *fp, PMT_SECTION_S *section);
int32 TSParse_PMTSection(const uint8 *secData, uint8 packetSize, PMT_SECTION_S *section);

void dump_PESPacketHeaderInfo(FILE *fp, PES_PACKET_HEADER_S *header);
int32 TSParse_PESPacketHeader(const uint8 *secData, uint8 packetSize, PES_PACKET_HEADER_S *header);

void dump_StreamTypeInfo(uint8 stream_type);
#endif
