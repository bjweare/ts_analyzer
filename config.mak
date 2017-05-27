SHELL := /bin/sh

CROSS_COMPILE = arm-histbv310-linux-
CROSS_COMPILE :=
CC		= $(CROSS_COMPILE)gcc
AR		= $(CROSS_COMPILE)ar   
LD		= $(CROSS_COMPILE)ld
RUN		= $(CROSS_COMPILE)run  
DB		= $(CROSS_COMPILE)gdb  
LINK    = $(CROSS_COMPILE)gcc  
CPP     = $(CROSS_COMPILE)cpp
CXX     = $(CROSS_COMPILE)g++
RANLIB  = $(CROSS_COMPILE)ranlib
AS      = $(CROSS_COMPILE)as
STRIP   = $(CROSS_COMPILE)strip

R := "\033[31m" # Red
G := "\033[32m" # Green
B := "\033[34m" # Blue
N := "\033[0m" # Normal Mode
AT := @
#$(AT)echo -e $(R)Red $(G)Green $(B)Blue $(N)Normal
export AT R G B N

PRJ_DIR := $(shell pwd)
NFSPATH := /home/chenzw/tmp/
export PRJ_DIR NFSPATH
