CXX = g++
CXXFLAGS ?= -std=c++20 -O2 -Wall
CPPFLAGS := $(CPPFLAGS) -I./include

SRCS = src/main.cpp
ISPC_SRCS = src/fractal.ispc

EXEC ?= executable
BUILD_DIR ?= build
RM ?= rm -f

.PHONY = all clean

all:
	mkdir -p $(BUILD_DIR)
	ispc --target=avx2-i32x8 $(ISPC_SRCS) -h include/fractal_ispc.h -o $(BUILD_DIR)/fractal_ispc.o
	@$(CXX) $(CXXFLAGS) -c $(CPPFLAGS) $(SRCS) -o $(BUILD_DIR)/main.o
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(BUILD_DIR)/main.o $(BUILD_DIR)/fractal_ispc.o -o $(EXEC)

# Note that fractal_ispc.h is technically a compiled file
clean:
	@$(RM) *.png
	@$(RM) $(EXEC) 
	@$(RM) include/fractal_ispc.h
	@$(RM) -r $(BUILD_DIR)
