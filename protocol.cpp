#include "protocol.h"

std::ostream& operator<<(std::ostream& os, const StatPacket& p) {
    os << "[CPU Load: " << p.cpu_load << "% | RAM Available: " << p.mem_available << "MB | Active Processes: " << p.processes_active << "]";
    return os;
}