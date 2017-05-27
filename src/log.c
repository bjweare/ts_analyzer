#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "log.h"

#define LOG_DIR "./ts_log"

int log_open(const char *filePath, uint32 len, const char *suffix, FILE **fp)
{
	int32 ret = PROG_FAILURE;
	uint32 i = 0;
	uint32 dotPos = 0;
	uint32 slashPos = 0;

	char strFileName[256] = {0};
	char strFilePath[256] = {0};

	for(i = 0; i < len; i++)
	{
		if('.' == filePath[i])
		{
			dotPos = i;
			break;
		}
	}

	if(len == i)
	{
		return PROG_FAILURE;
	}

	for(i = dotPos; i >= 0; i--)
	{
		if('/' == filePath[i])
		{
			slashPos = i;
			break;
		}
	}

	memcpy(strFileName, filePath+slashPos+1, dotPos-slashPos-1);
	strFileName[dotPos-slashPos] = '\0';

	PROG_PRINT(PROG_DEBUG, "%s", strFileName);

	ret = mkdir(LOG_DIR, S_IRWXU|S_IRWXG|(S_IROTH|S_IXOTH));
	if((PROG_SUCCESS != ret) && (EEXIST != errno))
	{
		PROG_PRINT(PROG_ERR, "%s", strerror(errno));
		return PROG_FAILURE;
	}

	sprintf(strFilePath, "%s/%s.%s", LOG_DIR, strFileName, suffix);
	*fp = fopen(strFilePath, "w+");
	if(NULL == *fp)
	{
		PROG_PRINT(PROG_ERR, "open <%s> failed!", strFilePath);
		return PROG_FAILURE;
	}
	
	return PROG_SUCCESS;
}

int log_close(FILE *fp)
{
	fclose(fp);

	return PROG_SUCCESS;
}

int log_write(FILE *fp, const char *fmt, ...)
{
	va_list arg;  
    int done;  
	  
	va_start(arg, fmt);  
	//done = vfprintf (stdout, fmt, arg);  
	
#if 0
	time_t time_log = time(NULL);  
	struct tm* tm_log = localtime(&time_log);  
	fprintf(fp, "%04d-%02d-%02d %02d:%02d:%02d ", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday, tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec);  
#endif

	done = vfprintf(fp, fmt, arg);  
	va_end(arg);  

	fflush(fp);  

	return done;  
}
