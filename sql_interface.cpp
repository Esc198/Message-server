#include "sql_interface.h"

SQL_Interface::SQL_Interface(const std::string &connectionString)
    : conn(connectionString) {
    try {
        if (conn.is_open()) {
            std::cout << "Opened database successfully: " << conn.dbname() << std::endl;
            // Prepare statements
            conn.prepare("auth_user", 
                "SELECT name FROM users WHERE name = $1 AND password = $2");
            conn.prepare("create_user", 
                "INSERT INTO users (name, password) VALUES ($1, $2)");
            conn.prepare("send_message", 
                "INSERT INTO messages (time, userFrom, userTo, message) VALUES ($1, $2, $3, $4)");
            conn.prepare("get_messages", 
                "SELECT * FROM messages WHERE (userTo = $1 AND userFrom = $2) OR (userFrom = $1 AND userTo = $2)");
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

pqxx::result SQL_Interface::executeQueryWithParams(const std::string &statement_name, const std::vector<std::string> &params) {
    try {
        pqxx::work W(conn);
        pqxx::result R;
        
        // Execute prepared statement with dynamic parameter count
        pqxx::prepare::invocation invoc = W.prepared(statement_name);
        for (const auto &param : params) {
            invoc(param);
        }
        R = invoc.exec();
        
        W.commit();
        return R;
    } catch (const std::exception &e) {
        std::cerr << "SQL Error with params: " << e.what() << std::endl;
        return pqxx::result();
    }
}