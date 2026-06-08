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

    app.port(80).multithreaded().run();
    return 0;
}