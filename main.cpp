#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <thread>
#include "server.h"

int main() {
    try {
        boost::asio::io_context io_context;
        std::string connectionString = "dbname=redsocial user=bases password=bases host=127.0.0.1 port=5432";
        Server server(io_context, 8085, connectionString);
        io_context.run();
    } catch (const boost::system::system_error& e) {
        std::cerr << "Boost system error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}