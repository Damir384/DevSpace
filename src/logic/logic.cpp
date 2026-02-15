#include "logic.hpp"

int App::run(std::string title){
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([title](){
        return title;
    });

    app.port(18080).run();
    return 0;
};