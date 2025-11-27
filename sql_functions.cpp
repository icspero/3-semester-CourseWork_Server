#include "header.h"

string sha256(const string &password) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new(); // создаем контекст для хеширования
    const EVP_MD *md = EVP_sha256(); // алгоритм sha256
    unsigned char hash[EVP_MAX_MD_SIZE]; // для готового хеша
    unsigned int hash_len;

    EVP_DigestInit_ex(mdctx, md, NULL); // начинаем хеширование
    EVP_DigestUpdate(mdctx, password.c_str(), password.size()); // даем алгоритму пароль
    EVP_DigestFinal_ex(mdctx, hash, &hash_len); // получаем готовый хеш в массив hash
    EVP_MD_CTX_free(mdctx); // освобождаем память

    // преобразуем байты хеша в шестнадцатеричную строку
    stringstream ss;
    for(unsigned int i = 0; i < hash_len; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    return ss.str();
}

// Функция добавления первого админа(если его еще нет)
string add_first_admin(connection &C) {
    string answer;
    work W(C);

    // Проверка существования администратора
    string check = "SELECT COUNT(*) FROM users WHERE login = 'admin1';";
    result R = W.exec(check);
    int user_exists = R[0][0].as<int>();

    if (user_exists > 0) { 
        answer = "Пользователь admin1 уже существует!\n";
        W.commit();
        return answer;
    }

    // Данные для создания
    string login = "admin1";
    string passwd_hash = sha256("12345");

    // Добавление в БД
    string insert_query = "INSERT INTO users (role_id, login, passwd_hash) VALUES (1, " + W.quote(login) + ", " + W.quote(passwd_hash) + ");";

    try {
        W.exec(insert_query);
        W.commit();
        answer = "Администратор admin1 успешно создан!\n";
    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка создания администратора: " + string(e.what()) + "\n";
    }

    return answer;
}

// Функция создания пользователей
string add_user(connection &C, const string &login, const string &password, const int &role) {
    string answer;
    work W(C);

    string check = "SELECT COUNT(*) FROM users WHERE login = " + W.quote(login) + ";";
    result R = W.exec(check);
    int user_exists = R[0][0].as<int>();

    if (user_exists > 0) { 
        answer = "Пользователь " + login + " уже существует!\n";
        W.commit();
        return answer;
    }

    string passwd_hash = sha256(password);
    string insert_query = "INSERT INTO users (role_id, login, passwd_hash) VALUES (" + to_string(role) + ", " + W.quote(login) + ", " + W.quote(passwd_hash) + ");";

    try {
        W.exec(insert_query);
        W.commit();
        answer = "Пользователь " + login + " успешно создан!\n";
    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка создания пользователя: " + string(e.what()) + "\n";
    }

    return answer;
}

// Функция аутентификации пользователя
string authenticate_user(connection &C, const string &login, const string &password) {
    string answer;
    work W(C);

    try {
        string check = "SELECT passwd_hash, role_id FROM users WHERE login = " + W.quote(login) + ";";
        result R1 = W.exec(check);

        if (R1.empty()) {
            answer = "Пользователь " + login + " с таким логином не найден!\n";
            W.commit();
            return answer;
        }

        string input_hash = sha256(password);
        string db_hash = R1[0][0].as<string>();
        int role_id = R1[0][1].as<int>();

        string check_role = "SELECT role_name FROM roles WHERE id = " + W.quote(role_id) + ";";
        result R2 = W.exec(check_role);
        string role_name = R2[0][0].as<string>();

        // Сравниваем пароли
        if (db_hash == input_hash) {
            answer = "Вход под ролью: " + role_name + "!\n";
        } else {
            answer = "Неверный пароль!\n";
        }

        W.commit();
    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка при аутентификации пользователя: " + string(e.what()) + "\n";
    }

    return answer;
}

// Функция изменения роли пользователя
string change_user_role(connection &C, const string &login, const string &new_role) {
    string answer;
    work W(C);
    
    try {
        // Проверяем текущую роль пользователя
        string check_sql = "SELECT role_id FROM users WHERE login = " + W.quote(login) + ";";
        result R = W.exec(check_sql);
        
        if (R.empty()) {
            answer = "Пользователь с таким логином не найден.\n";
            W.commit();
            return answer;
        }
        
        int role_id = R[0][0].as<int>();
        string check_role = "SELECT role_name FROM roles WHERE id = " + W.quote(role_id) + ";";
        result R2 = W.exec(check_sql);
        string current_role = R2[0][0].as<string>();
        
        if (current_role == new_role) {
            answer = "У пользователя уже установлена роль '" + new_role + "'!\n";
            W.commit();
            return answer;
        }
        
        // Обновляем роль
        string update_sql = "UPDATE users SET user_role = " + W.quote(new_role) + " WHERE user_login = " + W.quote(login) + ";";
        W.exec(update_sql);
        W.commit();
        
        answer = "Роль пользователя успешно изменена на '" + new_role + "'!\n";
    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка при изменении роли пользователя: " + string(e.what()) + "\n";
    }
    
    return answer;
}

// Добавление темы
string add_topic(connection &C, const string &cipher_name, const string &description, const string &theory) {
    string answer;
    work W(C);

    // Проверяем, есть ли уже тема с таким cipher_name
    string check_query = "SELECT COUNT(*) FROM topics WHERE cipher_name = " + W.quote(cipher_name) + ";";

    result R = W.exec(check_query);
    int topic_exists = R[0][0].as<int>();

    if (topic_exists > 0) {
        answer = "Тема '" + cipher_name + "' уже существует!\n";
        W.commit();
        return answer;
    }

    // Формируем запрос на вставку
    string insert_query =
        "INSERT INTO topics (cipher_name, description, theory) VALUES ("
        + W.quote(cipher_name) + ", "
        + W.quote(description) + ", "
        + W.quote(theory) + ");";

    try {
        W.exec(insert_query);
        W.commit();
        answer = "Тема '" + cipher_name + "' успешно добавлена!\n";
    }
    catch (const exception &e) {
        W.abort();
        answer = "Ошибка добавления темы: " + string(e.what()) + "\n";
    }

    return answer;
}

// Удаление темы
string delete_topic(connection &C, const string &cipher_name) {
    string answer;
    work W(C);

    try {
        // Проверяем, существует ли тема
        string check_query =
            "SELECT id FROM topics WHERE cipher_name = " + W.quote(cipher_name) + ";";

        result R = W.exec(check_query);

        if (R.empty()) {
            answer = "Тема '" + cipher_name + "' не найдена!\n";
            W.commit();
            return answer;
        }

        int topic_id = R[0][0].as<int>();

        string delete_query = "DELETE FROM topics WHERE id = " + to_string(topic_id) + ";";

        W.exec(delete_query);
        W.commit();

        answer = "Тема '" + cipher_name + "' успешна удалена!\n";
    }
    catch (const exception &e) {
        W.abort();
        answer = "Ошибка удаления темы: " + string(e.what()) + "\n";
    }

    return answer;
}

// Получить список всех тем
string get_all_topics(connection &C) {
    stringstream resultStream;
    work W(C);

    string sql = "SELECT cipher_name FROM topics ORDER BY cipher_name;";
    result R = W.exec(sql);

    if (R.empty()) {
        resultStream << "Темы не найдены!\n";
    } else {
        for (auto row : R) {
            resultStream << row[0].as<string>() << "\n";  // список названий
        }
    }

    W.commit();
    return resultStream.str();
}

// Добавить задание для темы
string add_task(connection &C, const string &cipher_name, const string &question, const string &correct_answer) {
    string answer;
    work W(C);

    try {
        // Находим topic_id по названию темы
        string find_topic_sql = "SELECT id FROM topics WHERE cipher_name = " + W.quote(cipher_name) + ";";
        result R = W.exec(find_topic_sql);

        if (R.empty()) {
            answer = "Тема '" + cipher_name + "' не найдена!\n";
            W.commit();
            return answer;
        }

        int topic_id = R[0][0].as<int>();

        // Вставляем новую задачу
        string insert_sql = "INSERT INTO tasks (topic_id, question, correct_answer) VALUES ("
                            + to_string(topic_id) + ", "
                            + W.quote(question) + ", "
                            + W.quote(correct_answer) + ");";

        W.exec(insert_sql);
        W.commit();
        answer = "Задание добавлено успешно для темы '" + cipher_name + "'!\n";

    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка добавления задания: " + string(e.what()) + "\n";
    }

    return answer;
}

// Получить все задания для темы
string get_tasks_for_topic(connection &C, const string &cipher_name) {
    stringstream resultStream;
    work W(C);

    try {
        // Находим topic_id по названию темы
        string find_topic_sql =
            "SELECT id FROM topics WHERE cipher_name = " + W.quote(cipher_name) + ";";
        result R_topic = W.exec(find_topic_sql);

        if (R_topic.empty()) {
            resultStream << "Тема '" << cipher_name << "' не найдена!\n";
            W.commit();
            return resultStream.str();
        }

        int topic_id = R_topic[0][0].as<int>();

        // Получаем все задания темы
        string get_tasks_sql = "SELECT id, question FROM tasks WHERE topic_id = " + to_string(topic_id) + " ORDER BY id;";
        result R_tasks = W.exec(get_tasks_sql);

        if (R_tasks.empty()) {
            resultStream << "Для темы '" << cipher_name << "' заданий нет!\n";
        } else {
            for (auto row : R_tasks) {
                resultStream << "[" << row[0].as<int>() << "] " << row[1].as<string>() << "\n";
            }
        }

        W.commit();

    } catch (const exception &e) {
        W.abort();
        resultStream << "Ошибка получения заданий: " << e.what() << "\n";
    }

    return resultStream.str();
}

string delete_task(connection &C, int task_id) {
    string answer;
    work W(C);

    try {
        // Проверяем, существует ли задание
        string check_sql = "SELECT COUNT(*) FROM tasks WHERE id = " + to_string(task_id) + ";";
        result R = W.exec(check_sql);

        if (R[0][0].as<int>() == 0) {
            answer = "Задание с ID " + to_string(task_id) + " не найдено!\n";
            W.commit();
            return answer;
        }

        string delete_sql = "DELETE FROM tasks WHERE id = " + to_string(task_id) + ";";
        W.exec(delete_sql);

        W.commit();
        answer = "Задание с ID " + to_string(task_id) + " успешно удалено!\n";

    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка при удалении задания: " + string(e.what()) + "\n";
    }

    return answer;
}

string get_task(connection &C, int task_id) {
    stringstream resultStream;
    work W(C);

    try {
        string sql = "SELECT question FROM tasks WHERE id = " + to_string(task_id) + ";";
        result R = W.exec(sql);

        if (R.empty()) {
            resultStream << "Задание с ID " << task_id << " не найдено!\n";
        } else {
            resultStream << "Вопрос: " << R[0][0].as<string>() << "\n";
        }

        W.commit();
    } catch (const exception &e) {
        W.abort();
        resultStream << "Ошибка получения задания: " << e.what() << "\n";
    }

    return resultStream.str();
}

string before_login(){
    string result;
    try{
        connection C("dbname=coursework user=postgres password=root host=localhost port=5432");
        result = add_first_admin(C);
    }
    catch (const exception &e) {
            cerr << e.what() << endl;
            result = "Ошибка: проблема с подключением к базе данных!\n";
        }
    return result;
}

string communicate_with_client(string& message, int client_socket_fd){
    if (!message.empty() && message.back() == '\n') message.pop_back();
    string result;
    stringstream ss(message);
    string command;
    // Разделяем сообщение по разделителю '|'
    getline(ss, command, '|');
    try {
        connection C("dbname=coursework user=postgres password=root host=localhost port=5432");
        if (command == "register"){
            string login;
            string passwd;
            string role;
            getline(ss, login, '|');
            getline(ss, passwd, '|');
            getline(ss, role, '|');
            result = add_user(C, login, passwd, stoi(role));
        }
        else if (command == "auth") {
            string login;
            string passwd;
            getline(ss, login, '|');
            getline(ss, passwd, '|');
            result = authenticate_user(C, login, passwd);
        }
        else if (command == "addtopic") {
            string cipher;
            string description;
            string theory;
            getline(ss, cipher, '|');
            getline(ss, description, '|');
            getline(ss, theory, '|');
            result = add_topic(C, cipher, description, theory);
        }
        /*
        else if (command == "change") {
            string username;
            string userrole;
            getline(ss, username, '|');
            getline(ss, userrole, '|');
            result = change_user_role(C, username, userrole);
        }
        else if (command == "1") {
            string shopName;
            string shopAddress;
            // Получаем название и адрес магазина
            getline(ss, shopName, '|');
            getline(ss, shopAddress, '|');
            result = add_shop(C, shopName, shopAddress);
        } else if (command == "2"){
            string shopName;
            string shopAddress;
            string productName;
            string cost;
            string count;

            getline(ss, shopName, '|');
            getline(ss, shopAddress, '|');
            getline(ss, productName, '|');
            getline(ss, cost, '|');
            getline(ss, count, '|');

            double price;
            int quantity;
            if (isValidDouble(cost)){
                price = stringToDouble(cost);
            }
            else{
                result = "Ошибка: Неккоректный ввод стоимости\n";
                return result;
            }
            if (isValidNumber(count)){
                quantity = stringToInt(count);
            }
            else{
                result = "Ошибка: Неккоректный ввод количества\n";
                return result;
            }
            result = add_product_to_shop(C, shopName, shopAddress, productName, price, quantity);
        } else if (command == "3"){
            string shopName;
            getline(ss, shopName, '|');
            result = find_shop_addresses(C, shopName);
        } else if (command == "4"){
            string productName;
            getline(ss, productName, '|');
            result = find_product(C, productName);
        } else if (command == "5"){
            string shopName;
            string shopAddress;
            // Получаем название и адрес магазина
            getline(ss, shopName, '|');
            getline(ss, shopAddress, '|');
            result = delete_shop(C, shopName, shopAddress);
        } else if (command == "6"){
            string shopName;
            string shopAddress;
            string productName;
            getline(ss, shopName, '|');
            getline(ss, shopAddress, '|');
            getline(ss, productName, '|');
            result = delete_product_from_shop(C, shopName, productName, shopAddress);
        } */
        else {
            result = "Ошибка: неизвестная команда!\n";
        }
    } catch (const exception &e) {
            cerr << e.what() << endl;
            result = "Ошибка: проблема с подключением к базе данных!\n";
        }
    return result;
}