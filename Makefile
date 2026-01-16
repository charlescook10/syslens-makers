# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall

# Target binary
TARGET = collector

# Source files
SRCS = agent.cpp protocol.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# How to build the binary from object files
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# How to build object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)
