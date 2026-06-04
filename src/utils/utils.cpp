#include "utils.hpp"
#include <sys/sysinfo.h>
#include <fstream>
#include <filesystem>
#include <regex>
#include <unistd.h>

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

namespace fs = std::filesystem;

// bool projects_directory_exist(std::string& path){
//     if (fs::exists(path)) {
//         if (fs::is_directory(path)) {
//             return true;
//         } else {
//             return false;
//         }
//     } else {
//         fs::create_directory(path);

//     }
// }

std::vector<std::string> ProjectManager::get_user_projects(const std::string& base_path) {
    std::vector<std::string> projects;
    try {
        if (fs::exists(base_path) && fs::is_directory(base_path)) {
            for (const auto& entry : fs::directory_iterator(base_path)) {
                if (entry.is_directory()) {
                    projects.push_back(entry.path().filename().string());
                }
            }
        }
    } catch (...) {
    }
    return projects;
}

ProjectStatus ProjectManager::create_project(const std::string& base_path, const std::string& proj_name, uid_t uid, gid_t gid) {
    if (proj_name.empty() || proj_name.length() > 255) return ProjectStatus::NameTooLong;
    
    std::regex name_regex("^[a-zA-Z0-9_-]+$"); //TODO Изменить функцию для поддержки других языков
    if (!std::regex_match(proj_name, name_regex)) return ProjectStatus::InvalidName;

    fs::path full_path = fs::path(base_path) / proj_name;

    if (fs::exists(full_path)) return ProjectStatus::AlreadyExists;

    try {
        fs::path base(base_path);
        
        if (!fs::exists(base)) {
            fs::create_directories(base); 
            if (chown(base.c_str(), uid, gid) != 0) {
                return ProjectStatus::NoPermissions;
            }
            fs::permissions(base, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec);
        }

        if (fs::create_directory(full_path)) {
            if (chown(full_path.c_str(), uid, gid) != 0) {
                return ProjectStatus::NoPermissions;
            }
            return ProjectStatus::Success;
        }
    } catch (const fs::filesystem_error& e) {
        return ProjectStatus::FileSystemError;
    }

    return ProjectStatus::UnknownError;
}


