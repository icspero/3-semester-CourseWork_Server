#define HEADER_H

#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include <cstring>
#include <thread>

#include <pqxx/pqxx>
#include <openssl/evp.h>

using namespace std;
using namespace pqxx;

string sha256(const string &password);
string add_first_admin(connection &C);
string add_user(connection &C, const string &login, const string &password, const int &role);
string authenticate_user(connection &C, const string &login, const string &password);
string change_user_role(connection &C, const string &login, const string &new_role);
string add_topic(connection &C, const string &cipher_name, const string &description, const string &theory);
string delete_topic(connection &C, const string &cipher_name);
string get_all_topics(connection &C);
string add_task(connection &C, const string &cipher_name, const string &question, const string &correct_answer);
string get_tasks_for_topic(connection &C, const string &cipher_name);
string delete_task(connection &C, int task_id);
string get_task(connection &C, int task_id);
string before_login();
string communicate_with_client(string& message, int client_socket_fd);