#include "sql_interface.h"
#include <iostream>

SQL_Interface::SQL_Interface(const std::string &connectionString):
conn(connectionString) {
    try {
        ;
        if (conn.is_open()) {
            std::cout << "Opened database successfully: " << conn.dbname() << std::endl;
        } else {
            std::cerr << "Can't open database" << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

SQL_Interface::~SQL_Interface() {
    conn.disconnect();
}

//Can throw almost all tipes of exceptions, but all are going to derive from std::exception
void SQL_Interface::executeQuery(const std::string &query) {

    pqxx::work W(conn);
    W.exec(query);
    W.commit();

}

pqxx::result SQL_Interface::executeSelectQuery(const std::string &query) {
    pqxx::result R;
    try {
        pqxx::nontransaction N(conn);
        R = N.exec(query);
    } catch (const std::exception &e) {
        std::cerr << "sql error: " << e.what() << std::endl;
    }
    return R;
}