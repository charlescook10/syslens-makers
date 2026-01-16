#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <iostream>
#include <cstdint>

struct StatPacket
{
    double cpu_load;            // Percentage of CPU capacity in use    -- 8 bytes 
    uint32_t mem_available;     // Available RAM in Megabytes           -- 4 bytes 
    uint32_t processes_active;  // Number of running tasks              -- 4 bytes 
};


std::ostream& operator<<(std::ostream& os, const StatPacket& p);

#endif