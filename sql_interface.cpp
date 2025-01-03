#include "sql_interface.h"

SQL_Interface::SQL_Interface(const std::string &connectionString)
    : conn(connectionString) {
    try {
        if (conn.is_open()) {
            std::cout << "Opened database successfully: " << conn.dbname() << std::endl;
            // Prepare statements
            conn.prepare("auth_user", 
                "SELECT * FROM users WHERE name = $1 AND password = $2");
            conn.prepare("create_user", 
                "INSERT INTO users (name, password) VALUES ($1, $2)");
            conn.prepare("send_message", 
                "INSERT INTO messages (userFrom, userTo, message) VALUES ($1, $2, $3)");
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
        
        // Execute prepared statement based on parameter count
        if (params.size() == 2) {
            R = W.exec_prepared(statement_name, params[0], params[1]);
        } else if (params.size() == 3) {
            R = W.exec_prepared(statement_name, params[0], params[1], params[2]);
        } else {
            throw std::runtime_error("Unsupported number of parameters");
        }
        
        W.commit();
        return R;
    } catch (const std::exception &e) {
        std::cerr << "SQL Error with params: " << e.what() << std::endl;
        return pqxx::result();
    }
}