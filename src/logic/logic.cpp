#include "logic.hpp"
#include "secure.hpp"
#include "utils.hpp"
#include "crow/middlewares/cookie_parser.h"
#include "crow/middlewares/session.h"
#include <pwd.h>

using Session = crow::SessionMiddleware<crow::InMemoryStore>;

// Добавить парсер настроек из файла конфигурации

// Проверка авторизации и сборка базового контекса
static bool base_context(crow::mustache::context& ctx, Session::context& session) {
    std::string user = session.get("username", "");
    
    if (user.empty()) {
        return false;
    }

    std::string realname = session.get("display_name", "");
    
    ctx["username"] = user;
    ctx["display_name"] = realname.empty() ? user : realname + " (" + user + ")";
    
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

        if (base_context(ctx, session)) {
            ctx["title"] = "Dashboard";

            return crow::response(crow::mustache::load("index.mustache").render(ctx));
        } else {
            ctx["title"] = "Login";
            return crow::response(crow::mustache::load("login.mustache").render(ctx));
        }
    });

    CROW_ROUTE(app, "/").methods("POST"_method)
    ([&app](const crow::request& req) {
        auto params = crow::query_string("?" + req.body);
        std::string user = params.get("username") ? params.get("username") : "";
        std::string pass = params.get("password") ? params.get("password") : "";

        if (auth_user(user, pass)) {
            // Получаем данные пользователя и записываем их в сессию
            auto& session = app.get_context<Session>(req);
            struct passwd *pw = getpwnam(user.c_str());
            std::string gecos(pw->pw_gecos);
            std::string realName = gecos.substr(0, gecos.find(','));
            
            if (!realName.empty()) session.set("display_name", realName);
            session.set("username", user);
            session.set("uid", std::to_string(pw->pw_uid));
            session.set("gid", std::to_string(pw->pw_gid));
            session.set("home", std::string(pw->pw_dir));
            session.set("shell", std::string(pw->pw_shell));

            // Перенаправляем на главную после логина
            crow::response res;
            res.code = 302;
            res.set_header("Location", "/");
            return res;
        }

        crow::mustache::context ctx;
        ctx["error"] = "Invalid login";
        return crow::response(crow::mustache::load("login.mustache").render(ctx));
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