#include "../comandos.h"

#include <boost/asio.hpp>
#include <iostream>
#include <string>


void send_int(boost::asio::ip::tcp::socket &socket, int value) {
    boost::asio::write(socket, boost::asio::buffer(&value, sizeof(int)));
}

void send_command(boost::asio::ip::tcp::socket &socket, int command) {
    send_int(socket, command);
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

void create_user(boost::asio::ip::tcp::socket &socket, const std::string &username, const std::string &password) {
    // Send create user command
    send_command(socket, CommandOptions::CREATE_USER);

    // Send username and password for new user
    send_int(socket, static_cast<int>(username.length()));
    send_int(socket, static_cast<int>(password.length()));
    send_message(socket, username);
    send_message(socket, password);
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

void get_messages(boost::asio::ip::tcp::socket &socket, int count, const std::string &destination, const std::string &user_name) {
    std::cout << "Getting " << count << " messages" << std::endl;
    send_int(socket, count);
    
    // Send destination length and destination
    int destination_length = static_cast<int>(destination.length());
    send_int(socket, destination_length);
    send_message(socket, destination); // Send destination

    // Receive message count
    int message_count;
    boost::asio::read(socket, boost::asio::buffer(&message_count, sizeof(int)));
    std::cout << "Message count: " << message_count << std::endl;

    // Receive messages
    for (int i = 0; i < message_count; i++) {
        int userFrom_length;
        int message_length;

        boost::asio::read(socket, boost::asio::buffer(&userFrom_length, sizeof(int)));
        boost::asio::read(socket, boost::asio::buffer(&message_length, sizeof(int)));

        std::vector<char> userFrom_buffer(userFrom_length);
        boost::asio::read(socket, boost::asio::buffer(userFrom_buffer));
        std::string userFrom(userFrom_buffer.begin(), userFrom_buffer.end());

        std::vector<char> message_buffer(message_length);
        boost::asio::read(socket, boost::asio::buffer(message_buffer));
        std::string message(message_buffer.begin(), message_buffer.end());

        std::cout << "Message from: " << userFrom << std::endl;
        std::cout << "Message: " << message << std::endl;
    }
}

bool handle_authentication(boost::asio::ip::tcp::socket &socket, const std::string &username, const std::string &password) {
    authenticate(socket, username, password);

    // Receive authentication result
    int auth_result;
    boost::asio::read(socket, boost::asio::buffer(&auth_result, sizeof(int)));

    if (auth_result != 0) {
        std::cout << "Authentication failed. Creating new user..." << std::endl;
        create_user(socket, username, password);

        // Authenticate again
        authenticate(socket, username, password);

        // Receive authentication result again
        boost::asio::read(socket, boost::asio::buffer(&auth_result, sizeof(int)));

        if (auth_result != 0) {
            std::cerr << "Authentication failed again. Exiting..." << std::endl;
            return false;
        }
    }
    return true;
}

void handle_send_message(boost::asio::ip::tcp::socket &socket, const std::string &destination, const std::string &message) {
    send_command(socket, CommandOptions::SEND_MESSAGE);
    send_message_data(socket, destination, message);
}

void handle_get_messages(boost::asio::ip::tcp::socket &socket, int count, const std::string &destination, const std::string &username) {
    send_command(socket, CommandOptions::GET_MESSAGES);
    get_messages(socket, count, destination, username);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <message>" << std::endl;
        return 1;
    }

    std::string username = argv[1];
    std::string password = argv[2];
    std::string message = argv[3];

    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", "8085");

        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        if (!handle_authentication(socket, username, password)) {
            return 1;
        }

        std::string destination = "user2";
        handle_send_message(socket, destination, message);
        handle_get_messages(socket, 5, destination, username);
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
