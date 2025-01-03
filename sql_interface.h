#pragma once
#include <pqxx/pqxx>
#include <string>
#include <iostream>
#include <vector>
#include <stdexcept>

class SQL_Interface {
public:
    SQL_Interface(const std::string &connectionString);
    ~SQL_Interface();
    
    pqxx::result executeSelectQuery(const std::string &query);
    pqxx::result executeQueryWithParams(const std::string &statement_name, const std::vector<std::string> &params);
    
private:
    pqxx::connection conn;
};
