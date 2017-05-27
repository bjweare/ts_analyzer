include config.mak

SRC_DIR := app
SRC_DIR += src
#vpath %.c $(SRC_DIR)

OUT_DIR := out
OBJ_DIR := $(OUT_DIR)/obj
BIN_DIR := $(OUT_DIR)/bin

SRC := $(wildcard $(foreach DIR,$(SRC_DIR),$(DIR)/*.c))
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRC))
HEADFILES := $(wildcard $(foreach DIR,$(SRC_DIR),$(DIR)/*.c))

CFLAGS := -I./include -I./src
CFLAGS += -Werror # -Wall
#CFLAGS += -g
LDFLAGS := -lpthread

APP := $(BIN_DIR)/ts_analyzer

#############################################
.PHONY: all clean

all: $(APP)
	$(AT)echo -e $(B)Compiled Successfully$(N)

$(APP): $(OBJS)
	$(AT)echo -e $(G)LD $@$(N)
	$(AT)test -d $(dir $@) || mkdir -p $(dir $@)
	$(AT)$(CC) $^ -o $@
	$(AT)cp $@ $(NFSPATH)

$(OBJ_DIR)/%.o:%.c
	$(AT)echo -e $(G)CC $^$(N)
	$(AT)test -d $(dir $@) || mkdir -p $(dir $@)
	$(AT)$(CC) $(CFLAGS) -o $@ -c $^ $(LDFLAGS)

clean:
	$(AT)rm -f $(OBJS)
	$(AT)echo -e $(R)Cleaned Successfully$(N)
