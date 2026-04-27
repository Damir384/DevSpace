#include "utils.hpp"

double SystemMonitor::get_cpu_temp() {
    std::ifstream temp_file("/sys/class/thermal/thermal_zone0/temp");
    if (!temp_file.is_open()) return -1.0;
    
    long millidegrees;
    temp_file >> millidegrees;
    temp_file.close();
    
    return millidegrees / 1000.0;
}

SystemMonitor::RamStats SystemMonitor::get_ram_info() {
    struct sysinfo si;
    if (sysinfo(&si) != 0) return {0, 0, 0.0};

    unsigned long total_ram = (si.totalram * si.mem_unit) / (1024 * 1024);
    unsigned long free_ram = (si.freeram * si.mem_unit) / (1024 * 1024);
    unsigned long buffer_ram = (si.bufferram * si.mem_unit) / (1024 * 1024);
    
    unsigned long used_ram = total_ram - free_ram - buffer_ram;
    double percent = (static_cast<double>(used_ram) / total_ram) * 100.0;

    return {total_ram, used_ram, percent};
}