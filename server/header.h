#define HEADER_H

#include <iostream>
#include <cstring>
#include <set>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include <thread>

#include <pqxx/pqxx>
#include <openssl/evp.h>

using namespace std;
using namespace pqxx;

string communicate_with_client(string& message, int client_socket_fd);
string before_login();

string sha256(const string &password);

string add_first_admin(connection &C);
string add_user(connection &C, const string &login, const string &password, const int &role);
string authenticate_user(connection &C, const string &login, const string &password);

string change_user_role(connection &C, const string &login, const string &new_role);

string get_user_id(connection &C, const string &login);
string get_roles(connection &C);
string get_role(connection &C, const string &user_id);
string get_users(connection &C);
string get_user_results(connection &C, const string &user_id, const string &topic_name = "");
string get_user_results_for_admin(connection &C, const string &user_login, const string &topic_name);
string get_roles_users(connection &C, const string &role_name = "");

string add_topic(connection &C, const string &cipher_name, const string &description, const string &theory);
string delete_topic(connection &C, const string &cipher_name);
string get_all_topics(connection &C);
string get_topic_description(connection &C, const string &cipher_name);
string get_topic_theory(connection &C, const string &cipher_name);

string add_task(connection &C, const string &cipher_name, const string &question, const string &correct_answer);
string delete_task(connection &C, int task_id);
string get_tasks_for_topic(connection &C, const string &cipher_name);
string get_task_correct_answer(connection &C, const string &task_id);

string save_result(connection &C, const string &user_id, const string& task_id, const string &is_correct, const string &user_answer);
string delete_results(connection &C, const string &results_id);