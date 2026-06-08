#include "logic.hpp"
#include "secure.hpp"
#include "utils.hpp"
#include "crow/middlewares/cookie_parser.h"
#include "crow/middlewares/session.h"
#include <pwd.h>
#include <filesystem>
#include <pty.h>
#include <utmp.h>
#include <grp.h>

using Session = crow::SessionMiddleware<crow::InMemoryStore>;

// Добавить парсер настроек из файла конфигурации

std::string pstatus_to_string(ProjectStatus status) {
    switch (status) {
        case ProjectStatus::Success:         return "Успех";
        case ProjectStatus::AlreadyExists:   return "Проект с таким именем уже существует";
        case ProjectStatus::InvalidName:     return "Недопустимое имя проекта";
        case ProjectStatus::NameTooLong:     return "Имя проекта слишком длинное";
        case ProjectStatus::NoPermissions:   return "Ошибка доступа: недостаточно прав в директории проектов";
        case ProjectStatus::FileSystemError: return "Системная ошибка файловой системы";
        case ProjectStatus::UnknownError:    return "Произошла неизвестная ошибка";
        default:                             return "Критическая ошибка";
    }
}

struct UserPtyContext {
    uid_t uid;
    gid_t gid;
    std::string home;
    std::string username;
    std::string shell;
};

// Проверка авторизации и сборка базового контекса
static bool base_context(crow::mustache::context& ctx, Session::context& session) {
    
    auto alerts_raw = session.get("alerts", "[]");
    auto alerts_json = crow::json::load(alerts_raw);
    if (alerts_json && alerts_json.size() > 0) {
        ctx["alerts"] = std::move(alerts_json);
        session.remove("alerts");
    }

    std::string user = session.get("username", "");
    if (user.empty()) return false;

    ctx["username"] = user;
    ctx["display_name"] = session.get("display_name", user);

    return true;
}

int App::run(std::string title) {
    crow::App<crow::CookieParser, Session> app{
        Session{ crow::InMemoryStore{} }
    };

    CROW_ROUTE(app, "/")
    .methods("GET"_method)([&app](const crow::request& req) {
        auto& session = app.get_context<Session>(req);
        crow::mustache::context ctx;
        crow::mustache::context dash_ctx;

        if (!base_context(ctx, session)) {
            ctx["title"] = "Login";
            return crow::response(crow::mustache::load("login.mustache").render(ctx));
        }
        ctx["title"] = "dashboard";
        std::string user_home = "/var/lib/devspace/projects/" + session.get("username", ""); //TODO сделать получение директории хранения проектов из файла конфигурации
        std::vector<std::string> projects = ProjectManager::get_user_projects(user_home);
        std::vector<crow::json::wvalue> proj_list;

        for (const auto& name : projects) {
            proj_list.push_back(crow::json::wvalue({{"project_name", name}}));
        }
        dash_ctx["projects"] = std::move(proj_list);
        ctx["main_content"] = crow::mustache::load("dashboard.mustache").render(dash_ctx).body_;

        return crow::response(crow::mustache::load("index.mustache").render(ctx));
    });

    CROW_ROUTE(app, "/login/").methods("POST"_method)
    ([&app](const crow::request& req) {
        auto& session = app.get_context<Session>(req);
        auto params = crow::query_string("?" + req.body);
        std::string user = params.get("username") ? params.get("username") : "";
        std::string pass = params.get("password") ? params.get("password") : "";

        if (auth_user(user, pass)) {
            // Получаем данные пользователя и записываем их в сессию
            struct passwd *pw = getpwnam(user.c_str());
            std::string gecos(pw->pw_gecos);
            std::string realName = gecos.substr(0, gecos.find(','));
            
            if (!realName.empty()) session.set("display_name", realName);
            session.set("username", user);
            session.set("uid", std::to_string(pw->pw_uid));
            session.set("gid", std::to_string(pw->pw_gid));
            session.set("home", std::string(pw->pw_dir));
            session.set("shell", std::string(pw->pw_shell));

            crow::json::wvalue::list alerts;
            alerts.push_back(crow::json::wvalue({
                {"message", "Добро пожаловать, " + user},
                {"icon_name", "verified_user"}, {"color_class", "w3-deep-purple"}
            }));
            alerts.push_back(crow::json::wvalue({
                {"message", "Внимание: это тестовая сборка DevSpace"},
                {"icon_name", "bug_report"}, {"color_class", "w3-blue"}
            }));

            session.set("alerts", crow::json::wvalue(std::move(alerts)).dump());

            crow::response res;
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }

        crow::json::wvalue::list alerts;
        alerts.push_back(crow::json::wvalue({
            {"message", "Доступ запрещен: неверные учетные данные"},
            {"icon_name", "lock_reset"}, {"color_class", "w3-red"}
        }));
        session.set("alerts", crow::json::wvalue(std::move(alerts)).dump());
        
        crow::response res;
        res.code = 302;
        res.set_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/favicon.ico")
    ([]{
        crow::response res;
        res.set_static_file_info("static/img/favicon.ico");
        return res;
    });

    CROW_ROUTE(app, "/add_project/").methods("POST"_method)
    ([&app](const crow::request& req) {
        auto& session = app.get_context<Session>(req);
        crow::mustache::context ctx;

        if (!base_context(ctx, session)) {
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }

        auto params = crow::query_string("?" + req.body);
        std::string project_name = params.get("project_name") ? params.get("project_name") : "";

        if (project_name.empty()) {
            crow::json::wvalue::list err_alerts;
            err_alerts.push_back(crow::json::wvalue({
                {"message", "Ошибка: имя проекта не может быть пустым"},
                {"icon_name", "error"}, {"color_class", "w3-red"}
            }));
            session.set("alerts", crow::json::wvalue(std::move(err_alerts)).dump());
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }

        uid_t uid = (uid_t)std::stoul(session.get("uid", "32768"));
        gid_t gid = (gid_t)std::stoul(session.get("gid", "32768"));
        std::string user_home = "/var/lib/devspace/projects/" + session.get("username", "");

        ProjectManager pm;
        ProjectStatus status = pm.create_project(user_home, project_name, uid, gid);

        crow::json::wvalue::list alerts;
        if (status == ProjectStatus::Success) {
            alerts.push_back(crow::json::wvalue({
                {"message", "Проект '" + project_name + "' успешно создан."},
                {"icon_name", "done"}, {"color_class", "w3-deep-purple"}
            }));
        } else {
            alerts.push_back(crow::json::wvalue({
                {"message", "Не удалось создать проект. " + pstatus_to_string(status)},
                {"icon_name", "warning"}, {"color_class", "w3-red"}
            }));
        }

        session.set("alerts", crow::json::wvalue(std::move(alerts)).dump());

        crow::response res;
        res.code = 302;
        res.set_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/project/")
    .methods("GET"_method)([&app](const crow::request& req) {
        auto& session = app.get_context<Session>(req);
        crow::mustache::context ctx;
        crow::response res;
        res.code = 302;
        res.set_header("Location", "/");
        return res;
    });

    CROW_ROUTE(app, "/project/<string>")
    .methods("GET"_method)([&app](const crow::request& req, std::string project_name) {
        auto& session = app.get_context<Session>(req);
        crow::mustache::context ctx;
        crow::response res;

        if (!base_context(ctx, session)) {
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }

        res.code = 302;
        res.set_header("Location", "/project/"+project_name+"/");
        return res;
    });

    CROW_ROUTE(app, "/project/<string><path>")
    .methods("GET"_method)([&app](const crow::request& req, std::string project_name, std::string sub_path) {
        auto& session = app.get_context<Session>(req);
        crow::mustache::context ctx;
        crow::mustache::context explorer_ctx;

        if (!base_context(ctx, session)) {
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }

        if (!ProjectManager::exists("/var/lib/devspace/projects/"+session.get("username", ""), project_name)){
            crow::json::wvalue::list alerts;
            alerts.push_back(crow::json::wvalue({
                {"message", "Проекта "+project_name+" несуществует"},
                {"icon_name", "warning"}, {"color_class", "w3-red"}
            }));
            session.set("alerts", crow::json::wvalue(std::move(alerts)).dump());
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }

        sub_path = url_decode(sub_path);

        crow::json::wvalue root = ProjectManager::list_project_dir("/var/lib/devspace/projects/"+session.get("username", "")+"/"+project_name, sub_path);
        
        explorer_ctx = std::move(root);
        explorer_ctx["base_path"] = "/project/"+project_name+"/";
        explorer_ctx["path"] = "/project/"+project_name+sub_path;
        ctx["title"] = project_name;
        ctx["main_content"] = crow::mustache::load("explorer.mustache").render(explorer_ctx).body_;

        return crow::response(crow::mustache::load("index.mustache").render(ctx));
    });

    std::map<crow::websocket::connection*, int> pty_masters;

    CROW_WEBSOCKET_ROUTE(app, "/terminal/ws")
    .onaccept([&](const crow::request& req, void** userdata) {
        auto& session = app.get_context<Session>(req);
        std::string user = session.get("username", "");
        
        if (user.empty()) return false;

        auto* ctx = new UserPtyContext();
        ctx->uid = (uid_t)std::stoul(session.get("uid", "32768"));
        ctx->gid = (gid_t)std::stoul(session.get("gid", "32768"));
        ctx->home = session.get("home", "/tmp");
        ctx->username = user;
        ctx->shell = session.get("shell", "/bin/bash");
        
        *userdata = ctx; 
        return true;
    })
    .onopen([&](crow::websocket::connection& conn) {
        auto* ctx = static_cast<UserPtyContext*>(conn.userdata());
        if (!ctx) { conn.close("Internal Error"); return; }

        int master;
        pid_t pid = forkpty(&master, NULL, NULL, NULL);

        if (pid == 0) {
            if (initgroups(ctx->username.c_str(), ctx->gid) != 0) exit(1);
            if (setgid(ctx->gid) != 0) exit(1);
            if (setuid(ctx->uid) != 0) exit(1);

            chdir(ctx->home.c_str());
            setenv("HOME", ctx->home.c_str(), 1);
            setenv("TERM", "xterm-256color", 1);
            setenv("USER", ctx->username.c_str(), 1);

            execl(ctx->shell.c_str(), ctx->shell.c_str(), "-l", NULL);
            exit(0);
        }

        pty_masters[&conn] = master;

        std::string banner = 
            "\r\n\x1b[1;35m"
            "----------------------------------------------------------\r\n"
            "  Welcome to DevSpace Terminal [ALPHA]\r\n"
            "  Architect: DevDrafts\r\n"
            "  GitHub:    https://github.com/ARDamir384/DevSpace\r\n"
            "  Warning:   DON'T TYPE COMMAND \"YES\"\r\n"
            "----------------------------------------------------------\r\n"
            "\x1b[0m\r\n";

        // Отправляем баннер клиенту
        conn.send_binary(banner);

        std::thread([&conn, master]() {
            char buffer[1024];
            while (true) {
                ssize_t n = read(master, buffer, sizeof(buffer));
                if (n <= 0) break;
                conn.send_binary(std::string(buffer, n));
            }
        }).detach();
    })
    .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
        auto it = pty_masters.find(&conn);
        if (it != pty_masters.end()) {
            int master_fd = it->second;

            if (!is_binary && data.find("resize") != std::string::npos) {
                auto j = crow::json::load(data);
                if (j && j.has("cols") && j.has("rows")) {
                    struct winsize ws;
                    ws.ws_col = (unsigned short)j["cols"].u();
                    ws.ws_row = (unsigned short)j["rows"].u();
                    ioctl(master_fd, TIOCSWINSZ, &ws);
                }
                return; 
            }
            ssize_t written = write(master_fd, data.c_str(), data.size());
            
            if (written == -1) {
                CROW_LOG_ERROR << "Failed to write to PTY master: " << errno;
            }
        }
    })
    .onclose([&](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
        if (pty_masters.count(&conn)) {
            close(pty_masters[&conn]);
            pty_masters.erase(&conn);
        }
        auto* ctx = static_cast<UserPtyContext*>(conn.userdata());
        if (ctx) delete ctx;
    });

    CROW_ROUTE(app, "/terminal/")
    .methods("GET"_method)([&app](const crow::request& req) {
        auto& session = app.get_context<Session>(req);
        crow::mustache::context ctx;

        if (!base_context(ctx, session)) {
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }
        ctx["main_content"] = crow::mustache::load("terminal.mustache").render().body_;
        
        return crow::response(crow::mustache::load("index.mustache").render(ctx));
    });

    app.port(80).multithreaded().run();
    return 0;
}