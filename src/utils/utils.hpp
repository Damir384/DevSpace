#pragma once
#include <vector>
#include <string>

enum class ProjectStatus {
    Success,
    AlreadyExists,
    InvalidName,
    NameTooLong,
    NoPermissions,
    FileSystemError,
    UnknownError
};

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
    //TODO сделать функцию проверки существования директории хранения проектов
    ProjectStatus create_project(const std::string& base_path, const std::string& proj_name, uid_t uid, gid_t gid);
    static std::vector<std::string> get_user_projects(const std::string& base_path);
};

