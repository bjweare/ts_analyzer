#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include "prj_common.h"

int log_open(const char *filePath, uint32 len, const char *suffix, FILE **fp);
int log_close(FILE *fp);
int log_write(FILE *fp, const char *fmt, ...);

#endif
