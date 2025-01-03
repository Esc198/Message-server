#pragma once

#include <pqxx/pqxx>
#include <string>

class SQL_Interface {
public:
    SQL_Interface(const std::string &connectionString);
    ~SQL_Interface();

    void executeQuery(const std::string &query);
    pqxx::result executeSelectQuery(const std::string &query);

private:
    pqxx::connection conn;
};

