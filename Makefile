CXX = g++
BUILD_DIR = build

UNAME_S := $(shell uname -s)
PLATFORM := $(UNAME_S)

ifeq ($(OS),Windows_NT)
    PLATFORM := windows
endif

ifeq ($(PLATFORM),Linux)
    CXXFLAGS = -std=c++17 -I. -pthread -static-libgcc -static-libstdc++
    LDFLAGS =
    STATIC = -static
    SERVER_OUT = $(BUILD_DIR)/server
    CLIENT_OUT = $(BUILD_DIR)/client
endif

ifeq ($(PLATFORM),Darwin)
    CXXFLAGS = -std=c++17 -I. -pthread
    LDFLAGS =
    STATIC =
    SERVER_OUT = $(BUILD_DIR)/server
    CLIENT_OUT = $(BUILD_DIR)/client
endif

ifeq ($(PLATFORM),windows)
    CXXFLAGS = -std=c++17 -I. -pthread -static-libgcc -static-libstdc++
    LDFLAGS = -lws2_32
    STATIC = -static
    SERVER_OUT = $(BUILD_DIR)/server.exe
    CLIENT_OUT = $(BUILD_DIR)/client.exe
endif

all: server client

server: $(SERVER_OUT)

$(SERVER_OUT): server.cpp SQL.cpp network_compat.h
	@mkdir -p $(BUILD_DIR)
	$(CXX) server.cpp $(CXXFLAGS) $(STATIC) $(LDFLAGS) -o $(SERVER_OUT)

client: $(CLIENT_OUT)

$(CLIENT_OUT): client.cpp network_compat.h
	@mkdir -p $(BUILD_DIR)
	$(CXX) client.cpp $(CXXFLAGS) $(STATIC) $(LDFLAGS) -o $(CLIENT_OUT)

run_server: server
	@$(SERVER_OUT)

run_client: client
	@$(CLIENT_OUT)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all server client run_server run_client clean
