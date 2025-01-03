create table users(
	name TEXT PRIMARY KEY,
	password TEXT
)

CREATE TABLE messages (
    ID SERIAL PRIMARY KEY,
    userFrom TEXT,
    userTo TEXT,
    message TEXT
);

SELECT * FROM users;

SELECT * FROM messages;
