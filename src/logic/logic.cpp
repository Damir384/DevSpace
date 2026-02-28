#include "logic.hpp"

int App::run(std::string title){
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        crow::mustache::context ctx;
        ctx["title"] = "login";
        auto page = crow::mustache::load("login.mustache");
        return page.render(ctx);
    });

    app.port(80).run();
    return 0;
};