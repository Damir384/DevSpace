#include "utils.hpp"
#include <sys/sysinfo.h>
#include <fstream>
#include <filesystem>
#include <regex>
#include <unistd.h>
#include <ctime>

std::string url_decode(const std::string &str)
{
    std::string res;
    res.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            try {
                // Превращаем "20" или "D0" в числовое значение символа
                int hex = std::stoi(str.substr(i + 1, 2), nullptr, 16);
                res += static_cast<char>(hex);
                i += 2;
            } catch (...) {
                // Если % есть, но после него не HEX — оставляем как есть
                res += str[i];
            }
        } else if (str[i] == '+') {
            res += ' ';
        } else {
            res += str[i];
        }
    }
    return res;
}


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

std::string get_permissions(const fs::path& path) {
    fs::perms p = fs::status(path).permissions();
    auto test = [&](fs::perms bit){ return (p & bit) != fs::perms::none ? 'x' : '-'; };
    // для читаемости сделаем отдельно для r, w, x
    auto rtest = [&](fs::perms bit){ return (p & bit) != fs::perms::none ? 'r' : '-'; };
    auto wtest = [&](fs::perms bit){ return (p & bit) != fs::perms::none ? 'w' : '-'; };

    std::string s;
    s += rtest(fs::perms::owner_read);
    s += wtest(fs::perms::owner_write);
    s += test (fs::perms::owner_exec);

    s += rtest(fs::perms::group_read);
    s += wtest(fs::perms::group_write);
    s += test (fs::perms::group_exec);

    s += rtest(fs::perms::others_read);
    s += wtest(fs::perms::others_write);
    s += test (fs::perms::others_exec);

    return s;
}

std::string last_modification_time(const fs::path& p) {
    try {
        auto ftime = fs::last_write_time(p);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now()
            + std::chrono::system_clock::now()
        );
        std::time_t t = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm = *std::localtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%F %T");
        return oss.str();
    } catch (const fs::filesystem_error&) {
        return {};
    }
}

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

bool ProjectManager::exists(const std::string& path, const std::string& project_name) {
    if (project_name.empty()) return false;
    
    fs::path p = path;
    p /= project_name;

    // Проверяем, существует ли путь и является ли он директорией
    return fs::exists(p) && fs::is_directory(p);
}

crow::json::wvalue ProjectManager::list_project_dir(const std::string& base_path, const std::string& project_dir) {
    fs::path base = fs::weakly_canonical(base_path);
    fs::path target = fs::weakly_canonical(base_path+project_dir);

    // Security Check: проверяем, что target все еще находится внутри base
    auto [base_it, target_it] = std::mismatch(
        base.begin(), base.end(), 
        target.begin(), target.end()
    );
    
    // Если итератор базы не дошел до конца — значит, префикс не совпал (попытка побега)
    if (base_it != base.end()) {
        target = base;
    }

    crow::json::wvalue::list file_list;

    try {
        if (fs::exists(target) && fs::is_directory(target)) {
            for (const auto& entry : fs::directory_iterator(target)) {
                crow::json::wvalue item;
                item["entry"] = entry.path().filename().string();
                item["is_directory"] = entry.is_directory();
                item["create_date"] = last_modification_time(entry);
                item["permissions"] = get_permissions(entry);
                
                if (entry.is_regular_file()) {
                    item["size"] = entry.file_size();
                }
                
                file_list.push_back(std::move(item));
            }
        }
    } catch (const fs::filesystem_error& e) {
        // Если что-то пошло не так (права доступа и т.д.), возвращаем пустой список или ошибку
    }

    crow::json::wvalue root;

    auto si = fs::space(target);

    root["files"] = std::move(file_list);
    root["free_space"] = si.free;
    return root;
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
