CC = gcc
CFLAGS_BASE = -Wall -Wextra -g
LDFLAGS_BASE = -pthread

# Check if GTK3 is available
GTK_CHECK := $(shell pkg-config --exists gtk+-3.0 && echo 1 || echo 0)

ifeq ($(GTK_CHECK), 1)
    CFLAGS_GTK = $(CFLAGS_BASE) `pkg-config --cflags gtk+-3.0`
    LDFLAGS_GTK = $(LDFLAGS_BASE) `pkg-config --libs gtk+-3.0`
    GTK_AVAILABLE = 1
else
    CFLAGS_GTK = $(CFLAGS_BASE) -DGTK_UNAVAILABLE
    LDFLAGS_GTK = $(LDFLAGS_BASE)
    GTK_AVAILABLE = 0
    $(warning GTK3 not found. Building server only. Install GTK3 to build the GUI client.)
endif

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

# Files
SERVER_SRC = $(SRC_DIR)/server/server.c $(SRC_DIR)/server/llm_interface.c
CLIENT_SRC = $(SRC_DIR)/client/client.c $(SRC_DIR)/client/gui.c
COMMON_SRC = $(SRC_DIR)/common/socket_utils.c $(SRC_DIR)/common/config.c

SERVER_OBJ = $(SERVER_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CLIENT_OBJ = $(CLIENT_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
COMMON_OBJ = $(COMMON_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

SERVER_BIN = $(BIN_DIR)/llm_server
CLIENT_BIN = $(BIN_DIR)/llm_client
CLI_CLIENT_BIN = $(BIN_DIR)/llm_cli_client

# Targets
ifeq ($(GTK_AVAILABLE), 1)
all: directories $(SERVER_BIN) $(CLIENT_BIN) $(CLI_CLIENT_BIN)
else
all: directories $(SERVER_BIN) $(CLI_CLIENT_BIN)
endif

directories:
	@mkdir -p $(BUILD_DIR)/server $(BUILD_DIR)/client $(BUILD_DIR)/common $(BIN_DIR)

$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS_BASE)

$(CLIENT_BIN): $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS_GTK)

$(CLI_CLIENT_BIN): $(BUILD_DIR)/client/cli_client.o $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS_BASE)

$(BUILD_DIR)/server/%.o: $(SRC_DIR)/server/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_BASE) -c $< -o $@

$(BUILD_DIR)/common/%.o: $(SRC_DIR)/common/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_BASE) -c $< -o $@

$(BUILD_DIR)/client/%.o: $(SRC_DIR)/client/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_GTK) -c $< -o $@

$(BUILD_DIR)/client/cli_client.o: $(SRC_DIR)/client/cli_client.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_BASE) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client:
ifeq ($(GTK_AVAILABLE), 1)
	./$(CLIENT_BIN)
else
	@echo "GTK3 not available. Using CLI client instead."
	./$(CLI_CLIENT_BIN)
endif

run-cli-client: $(CLI_CLIENT_BIN)
	./$(CLI_CLIENT_BIN)

.PHONY: all clean directories run-server run-client run-cli-client
