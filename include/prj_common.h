#ifndef __PRJ_COMMON_H__
#define __PRJ_COMMON_H__

#if 0
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
typedef unsigned long long uint64;
typedef signed char	int8;
typedef signed short int16;
typedef signed int int32;
typedef signed long long int64;
#else
#define uint8 unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define uint64 unsigned long long
#define int8 char
#define int16 short
#define int32 int
#define int64 long long
#endif

#define PROG_ERR	3
#define PROG_WARN	2
#define PROG_INFO	1
#define PROG_DEBUG	0
#define DEFAULT_LEVEL PROG_INFO
#define PROG_PRINT(level, fmt, args...) \
	do { \
		if(level >= DEFAULT_LEVEL) \
		{ \
			printf("%s[%d]: " fmt "\n", __func__, __LINE__, ##args); \
		} \
	} while (0)

#define PROG_TRUE 1
#define PROG_FALSE 0

#define PROG_SUCCESS 0
#define PROG_FAILURE -1

#define PROG_ASSERT(condition) do { \
		if(!(condition)) \
		{ \
			PROG_PRINT(PROG_ERR, "assert failed!!"); \
			return PROG_FAILURE; \
		} \
	} while (0)

#endif
