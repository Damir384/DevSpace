#pragma once
#include <vector>
#include <string>

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

class ProjectManager {
public:
    static std::vector<std::string> get_user_projects(const std::string& base_path);
};