g++ main.cpp server.cpp sql_interface.cpp -o build/server_executable -I/usr/include -lboost_system -lpqxx -lpq
g++ ./Test_Client/client.cpp -o build/client_executable -I/usr/include -lboost_system