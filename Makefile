# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall

# Targets
TARGETS = collector agent

# Source files
SRCS_COLLECTOR = collector.cpp protocol.cpp
SRCS_AGENT = agent.cpp protocol.cpp

# Object files
OBJS_COLLECTOR = $(SRCS_COLLECTOR:.cpp=.o)
OBJS_AGENT = $(SRCS_AGENT:.cpp=.o)

# Default target builds both
all: $(TARGETS)

# Build collector
collector: $(OBJS_COLLECTOR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_COLLECTOR)

# Build agent
agent: $(OBJS_AGENT)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_AGENT)

# Generic rule for object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS_COLLECTOR) $(OBJS_AGENT) $(TARGETS)
