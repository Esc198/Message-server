g++ main.cpp server.cpp sql_interface.cpp -o server_executable -I/usr/include -lboost_system -lpqxx -lpq
g++ ./Client/client.cpp -o client_executable -I/usr/include -lboost_system