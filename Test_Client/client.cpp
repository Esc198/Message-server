#include "../comandos.h"

#include <boost/asio.hpp>
#include <iostream>
#include <string>


void send_int(boost::asio::ip::tcp::socket &socket, int value) {
    boost::asio::write(socket, boost::asio::buffer(&value, sizeof(int)));
}

void send_message(boost::asio::ip::tcp::socket &socket, const std::string &message) {
    boost::asio::write(socket, boost::asio::buffer(message));
}

void authenticate(boost::asio::ip::tcp::socket &socket, const std::string &username, const std::string &password) {
    int command = CommandOptions::AUTHENTICATE;
    send_int(socket, command);

    // Send username length and username
    int username_length = static_cast<int>(username.length());
    send_int(socket, username_length);
    
    // Send password length and password
    int password_length = static_cast<int>(password.length());
    send_int(socket, password_length);
    send_message(socket, username);
    send_message(socket, password);

    std::cout << "Authentication data sent: username=" << username << ", password=" << password << std::endl;
}

void send_command(boost::asio::ip::tcp::socket &socket, int command) {
    send_int(socket, command);
}

void send_message_data(boost::asio::ip::tcp::socket &socket, const std::string &destination, const std::string &message) {
    // Send destination length and destination
    int destination_length = static_cast<int>(destination.length());
    send_int(socket, destination_length);
    
    // Send message length and message
    int message_length = static_cast<int>(message.length());
    send_int(socket, message_length);

    send_message(socket, destination);
    send_message(socket, message);
}

void read_response(boost::asio::ip::tcp::socket &socket) {
    char reply[1024];
    size_t reply_length = boost::asio::read(socket, boost::asio::buffer(reply, 1024));
    std::cout << "Reply: " << std::string(reply, reply_length) << std::endl;
}

int main() {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", "8085");

        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        // Authenticate
        std::string username = "user1";
        std::string password = "password123";
        authenticate(socket, username, password);

        // Send a message
        send_command(socket, CommandOptions::SEND_MESSAGE);

        std::string destination = "user2";
        std::string message = "Hello, this is a test message!";
        send_message_data(socket, destination, message);

        // Read server response
        read_response(socket);
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
