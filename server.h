#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "session.h"
#include "sql_interface.h"
class Server {
public:
    Server(boost::asio::io_context &io_context, int port, const std::string &connectionString);
    void async_listen_for_connections();
private:
    boost::asio::io_context &asio_context;
    boost::asio::ip::tcp::acceptor acceptor;
    std::vector<std::shared_ptr<Session>> sessions;
    SQL_Interface sql_interface;
};