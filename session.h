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
    Session(boost::asio::ip::tcp::socket socket, SQL_Interface &common_sql_interface, std::function<void(std::shared_ptr<Session>)> on_disconnect) 
        : socket_(std::move(socket)), sql_interface(common_sql_interface), read_data_(1024), on_disconnect_(on_disconnect) {}

    void start() {
        if (!socket_.is_open()) {
            std::cerr << "Socket is not open" << std::endl;
            return;
        }
        do_read();
    }

    ~Session() {
        std::cout << "Session ended" << std::endl;
    }

private:
    boost::asio::ip::tcp::socket socket_;
    std::string user_name;
    SQL_Interface &sql_interface;
    std::vector<char> read_data_;
    bool is_authenticated = false;
    std::function<void(std::shared_ptr<Session>)> on_disconnect_;
    
    void do_read() {
        async_listen_for_command();
    }

    void async_listen_for_command() {
        auto command = std::make_shared<CommandOptions>();
        boost::asio::async_read(socket_, boost::asio::buffer(command.get(), sizeof(CommandOptions)),
            [this, command](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    std::cout << "Command received: " << *command << std::endl;
                    if (!is_authenticated && *command != CommandOptions::AUTHENTICATE && *command != CommandOptions::CREATE_USER) {
                        std::cerr << "User is not authenticated. Command: " << *command << std::endl;
                        do_read();
                        return;
                    }
                    switch (*command) {
                        case CommandOptions::SEND_MESSAGE:
                            async_listen_for_send_message();
                            break;
                        case CommandOptions::GET_MESSAGES:
                            async_get_messages();
                            break;
                        case CommandOptions::AUTHENTICATE:
                            async_listen_for_authentication();
                            break;
                        case CommandOptions::CREATE_USER:
                            async_listen_for_create_user();
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

    /*
    1. Recibir cuantos mensajes quieres recibir.
    2. recibir la longitud del nombre del destinatario
    3. recibir el nombre del destinatario
    4. Hacer una consulta a la base de datos para obtener los mensajes
    5. Enviar cuantos mensajes se van a enviar
    6. Por cada mensaje:
        1. Enviar la longitud del nombre del remitente
        2. Enviar la longitud del mensaje
        3. Enviar el nombre del remitente
        4. Enviar el mensaje
    */
    void async_get_messages(){
        std::shared_ptr<std::vector<int>> lengths = std::make_shared<std::vector<int>>(2);
        boost::asio::async_read(socket_, boost::asio::buffer(*lengths),
            [this, lengths](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    int num_messages = (*lengths)[0];
                    int dest_length = (*lengths)[1];
                    std::cout << "Number of messages: " << num_messages << std::endl;
                    std::cout << "Destination length: " << dest_length << std::endl;

                    auto buffer = std::make_shared<std::vector<char>>(dest_length);

                    boost::asio::async_read(socket_, boost::asio::buffer(*buffer),
                        [this, buffer, dest_length, num_messages](boost::system::error_code ec, std::size_t) {
                            if (!ec) {
                                std::string destination(buffer->data(), dest_length);
                                std::cout << "Destination: " << destination << std::endl;

                                try {
                                    pqxx::result messages = sql_interface.executeSelectQuery("SELECT * FROM messages WHERE (userTo = '"
                                     + destination + "' AND userFrom = '" + user_name +"') OR (userFrom = '" + destination + 
                                     "' AND userTo = '" + user_name + "');");
                                    int num_messages_to_send = std::min(num_messages, static_cast<int>(messages.size()));
                                    std::cout << "Number of messages to send: " << num_messages_to_send << std::endl;

                                    // Send the number of messages to send
                                    boost::asio::write(socket_, boost::asio::buffer(&num_messages_to_send, sizeof(int)));

                                    for (int i = 0; i < num_messages_to_send; i++) {
                                        std::string message = messages[i]["message"].as<std::string>();
                                        std::string userFrom = messages[i]["userFrom"].as<std::string>();
                                        std::cout << "Message: " << message << std::endl;
                                        std::cout << "From: " << userFrom << std::endl;

                                        int userFrom_length = userFrom.size();
                                        int message_length = message.size();

                                        // Send the lengths of the userFrom and message
                                        boost::asio::write(socket_, boost::asio::buffer(&userFrom_length, sizeof(int)));
                                        boost::asio::write(socket_, boost::asio::buffer(&message_length, sizeof(int)));

                                        // Send the userFrom and message
                                        boost::asio::write(socket_, boost::asio::buffer(userFrom));
                                        boost::asio::write(socket_, boost::asio::buffer(message));

                                    
                                    }
                                } catch (const std::exception &e) {
                                    std::cerr << "Error getting messages: " << e.what() << std::endl;
                                }

                                do_read();
                            } else {
                                std::cerr << "Error from async_get_messages (data read): " << ec.message() << std::endl;
                                handle_error(ec);
                            }
                        });
                } else {
                    std::cerr << "Error reading lengths for GET_MESSAGES: " << ec.message() << std::endl;
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

                                // Check the database for user authentication
                                int auth_result = 1; // Default to failure
                                try {
                                    pqxx::result result = sql_interface.executeSelectQuery(
                                        "SELECT * FROM users WHERE name = '" + username + "' AND password = '" + password + "';");
                                    if (!result.empty()) {
                                        auth_result = 0; // Success
                                        is_authenticated = true;
                                    }
                                } catch (const std::exception &e) {
                                    std::cerr << "Error during authentication: " << e.what() << std::endl;
                                }

                                // Send the authentication result
                                boost::asio::write(socket_, boost::asio::buffer(&auth_result, sizeof(int)));

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


/*
error code 0: No error
error code 1: unexpected length of data for lengths
*/
    void async_listen_for_create_user() {
        auto lengths = std::make_shared<std::array<int, 2>>(); // Lengths for username and password
        boost::asio::async_read(socket_, boost::asio::buffer(*lengths),
            [this, lengths](boost::system::error_code ec, std::size_t bytes_transferred) {
                int error_code = 0;
                if (!ec) {
                    if (bytes_transferred != sizeof(int) * 2) {
                        std::cerr << "Error: Unexpected length of data for lengths. Expected: "
                                << sizeof(int) * 2 << " bytes, received: " << bytes_transferred << " bytes." << std::endl;
                        handle_error(boost::asio::error::operation_aborted);
                        error_code = 1;
                    } else {
                        int user_length = (*lengths)[0];
                        int pass_length = (*lengths)[1];

                        if (user_length <= 0 || pass_length <= 0) {
                            std::cerr << "Error: Invalid username or password length received." << std::endl;
                            handle_error(boost::asio::error::operation_aborted);
                            error_code = 1;
                        } else {
                            auto buffer = std::make_shared<std::vector<char>>(user_length + pass_length);

                            boost::asio::async_read(socket_, boost::asio::buffer(*buffer),
                                [this, &error_code, buffer, user_length, pass_length](boost::system::error_code ec, std::size_t bytes_transferred) {
                                    if (!ec) {
                                        std::string username(buffer->data(), user_length);
                                        std::string password(buffer->data() + user_length, pass_length);

                                        user_name = username; // Save the username
                                        std::cout << "Creating user: " << username << std::endl;
                                        std::cout << "Password: " << password << std::endl;
                                        
                                        // Insert into the database
                                        try {
                                            sql_interface.executeQuery("INSERT INTO users (name, password) VALUES ('" + username + "', '" + password + "');");
                                        } catch (const std::exception &e) {
                                            std::cerr << "Error creating user: " << e.what() << std::endl;
                                            error_code = 2;
                                        }
                                        // Send the error code
                                        boost::asio::write(socket_, boost::asio::buffer(&error_code, sizeof(int)));
                                        // Continue processing
                                        do_read();
                                    } else {
                                        std::cerr << "Error from async_listen_for_authentication (data read): " << ec.message() << std::endl;
                                        handle_error(ec);
                                    }
                                });
                        }
                    }
                } else {
                    std::cerr << "Error reading lengths for AUTHENTICATE: " << ec.message() << std::endl;
                    handle_error(ec);
                }
            });
        }


    void handle_error(const boost::system::error_code& ec) {
        socket_.close();
        std::cerr << "Error: " << ec.message() << std::endl;
        on_disconnect_(shared_from_this());
    }
};
