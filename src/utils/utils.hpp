#pragma once
#include <string>
#include <sys/sysinfo.h>
#include <fstream>

class SystemMonitor {
public:
    struct RamStats {
        unsigned long total;
        unsigned long used;
        double percent;
    };

    double get_cpu_temp();
    RamStats get_ram_info();
};