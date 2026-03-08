#include "logic.hpp"
#include "secure.hpp"

int App::run(std::string title){
    

    crow::SimpleApp app;

    CROW_ROUTE(app, "/").methods("GET"_method)([](){
        crow::mustache::context ctx;
        ctx["title"] = "login";
        auto page = crow::mustache::load("login.mustache");
        return page.render(ctx);
    });
    
    CROW_ROUTE(app, "/").methods("POST"_method)
    ([](const crow::request& req){
        // Парсим тело POST-запроса. Добавляем "?", чтобы парсер Crow 
        // воспринял строку параметров корректно (как в URL)
        auto params = crow::query_string("?" + req.body);

        // Пытаемся достать "username" из параметров формы, если его нет — берем пустую строку
        std::string user = params.get("username") ? params.get("username") : "";
        
        // Аналогично достаем "password"
        std::string pass = params.get("password") ? params.get("password") : "";

        // Вызываем нашу функцию проверки через PAM (из secure.cpp)
        if(auth_user(user, pass))
        {
            // Если успех: создаем контекст для шаблона Mustache
            crow::mustache::context ctx;
            ctx["user"] = user; // Передаем имя пользователя в шаблон

            // Загружаем файл index.mustache (главная страница после входа)
            auto page = crow::mustache::load("index.mustache");
            
            // Рендерим страницу с данными пользователя и возвращаем клиенту
            return crow::response(page.render(ctx));
        }

        // Если авторизация провалилась: готовим контекст с ошибкой
        crow::mustache::context ctx;
        ctx["error"] = "Invalid login"; // Текст ошибки для отображения в html

        // Загружаем обратно страницу входа login.mustache
        auto page = crow::mustache::load("login.mustache");
        
        // Возвращаем страницу входа с текстом ошибки
        return crow::response(page.render(ctx));
    });

    app.port(80).run();
    return 0;
};