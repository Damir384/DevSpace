#include "main.hpp"

std::string getExecutableDir() {
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0) return ".";
    path[len] = '\0';

    std::string full(path);
    return full.substr(0, full.find_last_of('/'));
}

int main()
{
    chdir(getExecutableDir().c_str());
    App app;
    // app.loglevel(crow::LogLevel::Warning); 
    app.run("Hello world!");
}
