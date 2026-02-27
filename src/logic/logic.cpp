#include "logic.hpp"

int App::run(std::string title){
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        auto page = crow::mustache::load("login.html");
        return page.render();
    });

    app.port(80).run();
    return 0;
};