#include "server.h"

Server::Server(boost::asio::io_context& io_context, int port, const std::string &connectionString) 
: asio_context(io_context), acceptor(asio_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)), sql_interface(connectionString) {
    acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    async_listen_for_connections();
}

void Server::async_listen_for_connections() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(asio_context);
    acceptor.async_accept(*socket,
        [this, socket](const boost::system::error_code &error) {
            if (!error) {
                auto new_session = std::make_shared<Session>(std::move(*socket), sql_interface);
                sessions.push_back(new_session);
                new_session->start();
                std::cout << "New connection" << std::endl;
            } else {
                std::cerr << "Accept error: " << error.message() << std::endl;
            }
            async_listen_for_connections();
        });
}