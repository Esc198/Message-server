#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "sql_interface.h"
#include "./comandos.h"


/*
Formato de la comunicacion:
1. El cliente envia un entero que representa el comando que quiere ejecutar
2. Dependiendo del comando se esperan cosas distintas:
    - SEND_MESSAGE: Se espera un entero con la longitud del nombre del destinatario,
        seguido de un entero con la longitud del mensaje, seguido del nombre del
        destinatario, seguido del mensaje.
    - AUTHENTICATE: Se espera un entero con la longitud del nombre de usuario, seguido
        del nombre de usuario, seguido de un entero con la longitud de la contraseña,
        seguido de la contraseña.
*/

class Session : public std::enable_shared_from_this<Session> {
public:
    Session( boost::asio::ip::tcp::socket socket, SQL_Interface &common_sql_interface) 
        : socket_(std::move(socket)), sql_interface(common_sql_interface), read_data_(1024) {}

    void start() {
        if (!socket_.is_open()) {
            std::cerr << "Socket is not open" << std::endl;
            return;
        }
        do_read();
    }

private:
    boost::asio::ip::tcp::socket socket_;
    std::string user_name;
    SQL_Interface &sql_interface;
    std::vector<char> read_data_;

    void do_read() {
        async_listen_for_command();
    }

    void async_listen_for_command() {
        auto command = std::make_shared<CommandOptions>();
        boost::asio::async_read(socket_, boost::asio::buffer(command.get(), sizeof(CommandOptions)),
            [this, command](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::cout << "Command received: " << *command << std::endl;
                    switch (*command) {
                        case CommandOptions::SEND_MESSAGE:
                            async_listen_for_send_message();
                            break;
                        case CommandOptions::AUTHENTICATE:
                            async_listen_for_authentication();
                            break;
                        default:
                            std::cerr << "Unknown command received." << std::endl;
                            std::cerr << "Command: " << *command << std::endl;
                            do_read();
                            break;
                    }
                } else {
                    std::cerr << "Error from async_listen_for_command: " << ec.message() << std::endl;
                    handle_error(ec);
                }
            });
    }

    void async_listen_for_send_message() {
        auto lengths = std::make_shared<std::array<int, 2>>(); // Lengths for destination and message
        boost::asio::async_read(socket_, boost::asio::buffer(*lengths),
            [this, self = shared_from_this(), lengths](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    int dest_length = (*lengths)[0];
                    int msg_length = (*lengths)[1];
                    std::cout << "Destination length: " << dest_length << std::endl;
                    std::cout << "Message length: " << msg_length << std::endl;

                    auto buffer = std::make_shared<std::vector<char>>(dest_length + msg_length);

                    boost::asio::async_read(socket_, boost::asio::buffer(*buffer),
                        [this, self, buffer, dest_length, msg_length](boost::system::error_code ec, std::size_t) {
                            if (!ec) {
                                std::string destination(buffer->data(), dest_length);
                                std::string message(buffer->data() + dest_length, msg_length);

                                try {
                                    sql_interface.executeQuery("INSERT INTO messages (userFrom, userTo, message) VALUES ('" 
                                        + user_name + "', '" + destination + "', '" + message + "');");
                                } catch (const std::exception &e) {
                                    std::cerr << "Error inserting message: " << e.what() << std::endl;
                                }

                                do_read();
                            } else {
                                std::cerr << "Error from async_listen_for_send_message: " << ec.message() << std::endl;
                                handle_error(ec);
                            }
                        });
                } else {
                    std::cerr << "Error reading lengths for SEND_MESSAGE: " << ec.message() << std::endl;
                    handle_error(ec);
                }
            });
    }

   void async_listen_for_authentication() {
    auto lengths = std::make_shared<std::array<int, 2>>(); // Lengths for username and password
    boost::asio::async_read(socket_, boost::asio::buffer(*lengths),
        [this, self = shared_from_this(), lengths](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                if (bytes_transferred != sizeof(int) * 2) {
                    std::cerr << "Error: Unexpected length of data for lengths. Expected: "
                              << sizeof(int) * 2 << " bytes, received: " << bytes_transferred << " bytes." << std::endl;
                    handle_error(boost::asio::error::operation_aborted);
                    return;
                }

                int user_length = (*lengths)[0];
                int pass_length = (*lengths)[1];

                if (user_length <= 0 || pass_length <= 0) {
                    std::cerr << "Error: Invalid username or password length received." << std::endl;
                    handle_error(boost::asio::error::operation_aborted);
                    return;
                }

                std::cout << "User length: " << user_length << ", Password length: " << pass_length << std::endl;

                auto buffer = std::make_shared<std::vector<char>>(user_length + pass_length);

                boost::asio::async_read(socket_, boost::asio::buffer(*buffer),
                    [this, self, buffer, user_length, pass_length](boost::system::error_code ec, std::size_t bytes_transferred) {
                        if (!ec) {
                            if (bytes_transferred != static_cast<std::size_t>(user_length + pass_length)) {
                                std::cerr << "Error: Unexpected length of data for authentication details. "
                                          << "Expected: " << user_length + pass_length << " bytes, received: " << bytes_transferred << " bytes." << std::endl;
                                handle_error(boost::asio::error::operation_aborted);
                                return;
                            }

                            std::string username(buffer->data(), user_length);
                            std::string password(buffer->data() + user_length, pass_length);

                            user_name = username; // Save the username
                            std::cout << "Authenticating user: " << username << std::endl;
                            std::cout << "Password: " << password << std::endl;

                            // Continue processing
                            do_read();
                        } else {
                            std::cerr << "Error from async_listen_for_authentication (data read): " << ec.message() << std::endl;
                            handle_error(ec);
                        }
                    });
            } else {
                std::cerr << "Error reading lengths for AUTHENTICATE: " << ec.message() << std::endl;
                handle_error(ec);
            }
        });
    }


    void handle_error(const boost::system::error_code& ec) {
        socket_.close();
        std::cerr << "Error: " << ec.message() << std::endl;
    }
};
