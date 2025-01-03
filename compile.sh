g++ main.cpp server.cpp sql_interface.cpp -o build/server_executable -I/usr/include -lboost_system -lpqxx -lpq
res1=$?

g++ ./Test_Client/client.cpp -o build/client_executable -I/usr/include -lboost_system
res2=$?

if [ $res1 -ne 0 ] || [ $res2 -ne 0 ]; then
  exit 1
else
  exit 0
fi