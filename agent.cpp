#include <iostream>
#include <unistd.h>
#include <sys/socket.h> // Core socket functions
#include <arpa/inet.h>  // Functions for handling IP addresses
#include "protocol.h"

#ifdef __APPLE__
    #include <mach/mach.h>
    #include <mach/mach_host.h>
    #include <sys/sysctl.h>
    #include <vector>
#elif __linux__
    #include <fstream>
    #include <string>
    #include <limits>
    #include <dirent.h>
#endif

// RAII Wrapper: ensures the socket 'fd' is closed when this struct dies
struct SocketManager {
    int fd;
    SocketManager() { 
        // Create an IPv4 (AF_INET) TCP (SOCK_STREAM) socket
        fd = socket(AF_INET, SOCK_STREAM, 0); 
    }
    ~SocketManager() { 
        if (fd >= 0) {
            close(fd); 
            std::cout << "Client: Socket closed." << std::endl;
        }
    }
};

long get_free_memory_mb() {
#ifdef __APPLE__
    // --- macOS Implementation (Mach Kernel) ---
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    // Request 64-bit host statistics from the Mach kernel
    kern_return_t result = host_statistics64(mach_host_self(), 
                                            HOST_VM_INFO64, 
                                            (host_info64_t)&vm_stats, 
                                            &count);

    if (result == KERN_SUCCESS) {
        //std::cout << "Page Size: " << sysconf(_SC_PAGESIZE) << std::endl;
        // Multiply free pages by system page size, then convert to MB
        return ((vm_stats.free_count * sysconf(_SC_PAGESIZE)) / (1024*1024));
    }
    return -1; 

#elif __linux__
    // --- Linux Implementation (/proc) ---
    std::ifstream file("/proc/meminfo");
    std::string line;

    // Logic: Find "MemAvailable:", parse the integer, return it.
    while (file >> line) {
        if (line == "MemAvailable:") {
            unsigned long memory;
            if (file >> memory) {
                //std::cout << "Memory Available: " << memory << std::endl;
                return memory / 1024;
            } else {
                return -1;
            }
        }
        // Ignore the rest of the line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    return -1;
#endif
}

double get_cpu_load() {
#ifdef __APPLE__
    // --- macOS Implementation (Mach Kernel) ---
    host_cpu_load_info_data_t cpuLoad1, cpuLoad2;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    if (host_statistics(mach_host_self(),
                        HOST_CPU_LOAD_INFO,
                        (host_info_t)&cpuLoad1,
                        &count) != KERN_SUCCESS) {
        return -1.0;
    }

    usleep(100000);

    if (host_statistics(mach_host_self(),
                        HOST_CPU_LOAD_INFO,
                        (host_info_t)&cpuLoad2,
                        &count) != KERN_SUCCESS) {
        return -1.0;
    }

    unsigned long user =
        cpuLoad2.cpu_ticks[CPU_STATE_USER] -
        cpuLoad1.cpu_ticks[CPU_STATE_USER];

    unsigned long system =
        cpuLoad2.cpu_ticks[CPU_STATE_SYSTEM] -
        cpuLoad1.cpu_ticks[CPU_STATE_SYSTEM];

    unsigned long nice =
        cpuLoad2.cpu_ticks[CPU_STATE_NICE] -
        cpuLoad1.cpu_ticks[CPU_STATE_NICE];

    unsigned long idle =
        cpuLoad2.cpu_ticks[CPU_STATE_IDLE] -
        cpuLoad1.cpu_ticks[CPU_STATE_IDLE];

    unsigned long total = user + system + nice + idle;
    if (total == 0) {
        return 0.0;
    }

    double cpu_usage = (1.0 - (double)idle / total) * 100.0;
    return cpu_usage;

#elif __linux__
    // --- Linux Implementation (/proc) ---
    // https://man7.org/linux/man-pages/man5/proc_stat.5.html
    auto read_cpu_times = [](unsigned long& idle, unsigned long& total) {
        std::ifstream file("/proc/stat");
        std::string cpu;
        unsigned long user, nice, system, idle_time, iowait, irq, softirq, steal; // Guest time is already included in user time
        // reading 'token' by 'token'
        file >> cpu
            >> user >> nice >> system >> idle_time
            >> iowait >> irq >> softirq >> steal;

        idle = idle_time + iowait;
        total = user + nice + system + idle_time + iowait + irq + softirq + steal;
    };

    unsigned long idle1, total1;
    unsigned long idle2, total2;

    read_cpu_times(idle1, total1);

    usleep(100000);

    read_cpu_times(idle2, total2);

    long idle_delta = idle2 - idle1;
    long total_delta = total2 - total1;

    if (total_delta == 0) {
        return 0;
    }

    double cpu_usage = (1.0 - (double)idle_delta / total_delta) * 100.0;
    return cpu_usage;
#endif
}

long get_active_processes() {
#ifdef __APPLE__
    // --- macOS Implementation (Mach Kernel) ---
    int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    size_t len = 0;

    // First call to sysctl to get the required buffer size
    if (sysctl(mib, 3, nullptr, &len, nullptr, 0) < 0) return -1;

    std::vector<char> buffer(len);

    // Second call to sysctl to get process list
    if (sysctl(mib, 3, buffer.data(), &len, nullptr, 0) < 0) return -1;

    // Each process is stored in a kinfo_proc structure
    long process_count = len / sizeof(struct kinfo_proc);
    return process_count;

#elif __linux__
    // --- Linux Implementation (/proc) ---
    size_t count = 0;
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return -1;

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        const char* name = entry->d_name;
        // Check if the directory name is numeric
        bool numeric = true;
        for (size_t i = 0; name[i] != '\0'; ++i) {
            if (!isdigit(name[i])) {
                numeric = false;
                break;
            }
        }
        if (numeric) count++;
    }

    closedir(proc_dir);
    return count;
#endif
}

void send_stat_packet(int sock, StatPacket& packet) {
    uint16_t cpu_fixed = htons(static_cast<uint16_t>(packet.cpu_load * 10)); // 1 decimal place
    uint32_t net_mem = htonl(packet.mem_available);
    uint32_t net_proc = htonl(packet.processes_active);

    // Send in the order: cpu_load, mem_available, processes_active
    send(sock, &cpu_fixed, sizeof(uint16_t), 0);
    send(sock, &net_mem, sizeof(uint32_t), 0);
    send(sock, &net_proc, sizeof(uint32_t), 0);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <ip_address> <port>" << std::endl;
        return 1;
    }

    SocketManager s; // Socket is created here

    // 1. Define the destination (Localhost: 127.0.0.1, Port: 8080)
    sockaddr_in addr = {};           
    addr.sin_family = AF_INET;       
    addr.sin_port = htons(std::stoi(argv[2]));     // htons: 'host to network short' (Big Endian)
    inet_pton(AF_INET, argv[1], &addr.sin_addr);

    // 2. Connect to the server
    if (connect(s.fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "Connection Failed. Did you start the server first?" << std::endl;
        return 1;
    }

    // 3. Send StatPacket to server

    StatPacket my_packet;
    my_packet.cpu_load = get_cpu_load();
    my_packet.mem_available = get_free_memory_mb();
    my_packet.processes_active = get_active_processes();

    send_stat_packet(s.fd, my_packet);

    std::cout << "Message sent!" << std::endl;

    return 0; // RAII Destructor runs here, automatically calling close()
}

