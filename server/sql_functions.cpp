#include "header.h"

// Хеширование пароля 
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
string add_user(connection &C, const string &login, const string &password) {
    try {
        work W(C);

        // Проверяем существование пользователя
        result R = W.exec_params(
            "SELECT COUNT(*) FROM users WHERE login = $1",
            login
        );

        if (R[0][0].as<int>() > 0) {
            W.commit();
            return "Пользователь " + login + " уже существует!\n";
        }

        result R2 = W.exec(
            "SELECT id FROM roles WHERE role_name = 'Посетитель'"
        );

        if (R2.empty()) {
            W.commit();
            return "Ошибка: роль 'Посетитель' не найдена!\n";
        }

        int role_id = R2[0][0].as<int>();

        string passwd_hash = sha256(password);

        W.exec_params(
            "INSERT INTO users (role_id, login, passwd_hash) "
            "VALUES ($1, $2, $3)",
            role_id,
            login,
            passwd_hash
        );

        W.commit();
        return "Пользователь " + login + " успешно создан!\n";
    }
    catch (const exception &e) {
        return string("Ошибка создания пользователя: ") + e.what() + "\n";
    }
}

// Функция аутентификации пользователя
string authenticate_user(connection &C, const string &login, const string &password) {
    try {
        work W(C);

        // Получаем хеш пароля и роль
        result R1 = W.exec_params(
            "SELECT passwd_hash, role_id FROM users WHERE login = $1",
            login
        );

        if (R1.empty()) {
            W.commit();
            return "Пользователь с логином " + login + " не существует!\n";
        }

        string db_hash = R1[0][0].as<string>();
        int role_id = R1[0][1].as<int>();

        string input_hash = sha256(password);

        if (db_hash != input_hash) {
            W.commit();
            return "Неверный пароль!\n";
        }

        // Получаем название роли
        result R2 = W.exec_params(
            "SELECT role_name FROM roles WHERE id = $1",
            role_id
        );

        if (R2.empty()) {
            W.commit();
            return "Ошибка: роль не найдена!\n";
        }

        string role_name = R2[0][0].as<string>();

        W.commit();
        return "Вход под ролью: " + role_name + "!\n";
    }
    catch (const exception &e) {
        return string("Ошибка при аутентификации пользователя: ") + e.what() + "\n";
    }
}

// Получить id пользователя
string get_user_id(connection &C, const string &login) {
    string answer;
    work W(C);

    try {
        string check_sql = "SELECT id FROM users WHERE login = " + W.quote(login) + ";";
        result R = W.exec(check_sql);
        
        if (R.empty()) {
            answer = "Пользователь с таким логином не найден!\n";
            W.commit();
            return answer;
        }
        
        string user_id = R[0][0].as<string>();
        
        answer = "ok|" + user_id;
        W.commit();
    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка при получении id пользователя: " + string(e.what()) + "\n";
    }
    
    return answer;
}

// Функция изменения роли пользователя
string change_user_role(connection &C, const string &login, const string &new_role) {
    try {
        work W(C);

        // Получаем текущую роль пользователя
        result R1 = W.exec_params(
            "SELECT u.role_id, r.role_name, u.login "
            "FROM users u "
            "JOIN roles r ON u.role_id = r.id "
            "WHERE u.login = $1",
            login
        );

        if (R1.empty()) {
            W.commit();
            return "Пользователь с таким логином не найден!\n";
        }

        int current_role_id = R1[0][0].as<int>();
        string current_role_name = R1[0][1].as<string>();
        string login = R1[0][2].as<string>();

        if (login == "admin1") {
            W.commit();
            return "Нельзя изменить роль у главного администратора!\n";
        }

        if (current_role_name == new_role) {
            W.commit();
            return "У пользователя уже установлена роль '" + new_role + "'!\n";
        }

        // Получаем id новой роли
        result R2 = W.exec_params(
            "SELECT id FROM roles WHERE role_name = $1",
            new_role
        );

        if (R2.empty()) {
            W.commit();
            return "Роль '" + new_role + "' не найдена!\n";
        }

        int new_role_id = R2[0][0].as<int>();

        // Обновляем роль
        W.exec_params(
            "UPDATE users SET role_id = $1 WHERE login = $2",
            new_role_id,
            login
        );

        W.commit();
        return "Роль пользователя успешно изменена на '" + new_role + "'!\n";
    }
    catch (const exception &e) {
        return "Ошибка при изменении роли пользователя: " + string(e.what()) + "\n";
    }
}

// Добавление темы
string add_topic(connection &C, const string &cipher_name, const string &description, const string &theory) {
    string answer;
    work W(C);

    try {
        // Проверяем, есть ли уже тема с таким cipher_name и получаем текущие поля
        string check_query = "SELECT description, theory FROM topics WHERE cipher_name = " + W.quote(cipher_name) + ";";
        result R = W.exec(check_query);

        if (!R.empty()) {
            string current_description = R[0]["description"].c_str();
            string current_theory = R[0]["theory"].c_str();

            // Сравниваем
            if (current_description != description || current_theory != theory) {
                // Обновляем только если данные отличаются
                string update_query = 
                    "UPDATE topics SET "
                    "description = " + W.quote(description) + ", "
                    "theory = " + W.quote(theory) + " "
                    "WHERE cipher_name = " + W.quote(cipher_name) + ";";

                W.exec(update_query);
                W.commit();
                answer = "Тема '" + cipher_name + "' уже существует, данные обновлены!\n";
            } else {
                answer = "Тема '" + cipher_name + "' уже существует, изменений нет!\n";
                W.commit();
            }
            return answer;
        }

        // Если темы нет, то вставляем новую
        string insert_query =
            "INSERT INTO topics (cipher_name, description, theory) VALUES ("
            + W.quote(cipher_name) + ", "
            + W.quote(description) + ", "
            + W.quote(theory) + ");";

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
    try {
        work W(C);

        // Находим id темы
        result R = W.exec_params(
            "SELECT id FROM topics WHERE cipher_name = $1",
            cipher_name
        );

        if (R.empty()) {
            W.commit();
            return "Тема '" + cipher_name + "' не найдена!\n";
        }

        int topic_id = R[0][0].as<int>();

        // Удаляем тему по id
        W.exec_params(
            "DELETE FROM topics WHERE id = $1",
            topic_id
        );

        W.commit();
        answer = "Тема '" + cipher_name + "' успешно удалена!\n";
    } catch (const exception &e) {
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
        resultStream << "Темы не найдены\n";
    } else {
        for (auto row : R) {
            resultStream << row[0].as<string>() << "\n";
        }
    }

    W.commit();
    return resultStream.str();
}

// Получить описание темы
string get_topic_description(connection &C, const string &cipher_name) {
    stringstream resultStream;
    work W(C);

    string sql = "SELECT description FROM topics WHERE cipher_name = " + W.quote(cipher_name) +  ";";
    result R = W.exec(sql);

    if (R.empty()) {
        resultStream << "Описание отсутствует\n";
    } else {
        resultStream << R[0][0].as<string>() << "\n";
    }

    W.commit();
    return resultStream.str();
}

// Получить теорию по теме
string get_topic_theory(connection &C, const string &cipher_name) {
    stringstream resultStream;
    work W(C);

    string sql = "SELECT theory FROM topics WHERE cipher_name = " + W.quote(cipher_name) +  ";";
    result R = W.exec(sql);

    if (R.empty()) {
        resultStream << "Теория отсутствует\n";
    } else {
        resultStream << R[0][0].as<string>() << "\n";
    }

    W.commit();
    return resultStream.str();
}

// Добавить задание для темы
string add_task(connection &C, const string &cipher_name, const string &question, const string &correct_answer) {
    string answer;
    try {
        work W(C);

        // Находим topic_id по названию темы
        result R = W.exec_params(
            "SELECT id FROM topics WHERE cipher_name = $1",
            cipher_name
        );

        if (R.empty()) {
            W.commit();
            return "Тема '" + cipher_name + "' не найдена!\n";
        }

        int topic_id = R[0][0].as<int>();

        // Вставляем новую задачу с использованием параметров
        W.exec_params(
            "INSERT INTO tasks (topic_id, question, correct_answer) VALUES ($1, $2, $3)",
            topic_id,
            question,
            correct_answer
        );

        W.commit();
        answer = "Задание добавлено успешно для темы '" + cipher_name + "'!\n";

    } catch (const exception &e) {
        answer = "Ошибка добавления задания: " + string(e.what()) + "\n";
    }

    return answer;
}

// Получить все задания для темы
string get_tasks_for_topic(connection &C, const string &cipher_name) {
    stringstream resultStream;

    try {
        work W(C);

        // Находим topic_id по названию темы
        result R_topic = W.exec_params(
            "SELECT id FROM topics WHERE cipher_name = $1",
            cipher_name
        );

        if (cipher_name == "Темы не найдены") {
            resultStream << "Отсутствуют темы\n";
            W.commit();
            return resultStream.str();
        }

        if (R_topic.empty()) {
            resultStream << "Тема '" << cipher_name << "' не найдена!\n";
            W.commit();
            return resultStream.str();
        }

        int topic_id = R_topic[0][0].as<int>();

        // Получаем все задания темы с использованием параметров
        result R_tasks = W.exec_params(
            "SELECT id, question FROM tasks WHERE topic_id = $1 ORDER BY id",
            topic_id
        );

        if (R_tasks.empty()) {
            resultStream << "Для темы '" << cipher_name << "' заданий нет!\n";
        } else {
            for (auto row : R_tasks) {
                resultStream << "[" << row[0].as<int>() << "] " << row[1].as<string>() << "\n";
            }
        }

        W.commit();

    } catch (const exception &e) {
        resultStream << "Ошибка получения заданий: " << e.what() << "\n";
    }

    return resultStream.str();
}

// Удалить задание
string delete_task(connection &C, int task_id) {
    string answer;
    work W(C);

    try {
        // Проверяем, существует ли задание
        result R = W.exec_params(
            "SELECT COUNT(*) FROM tasks WHERE id = $1;",
            to_string(task_id)
        );

        if (R[0][0].as<int>() == 0) {
            answer = "Задание с ID " + to_string(task_id) + " не найдено!\n";
            W.commit();
            return answer;
        }

        // Удаляем задание
        W.exec_params(
            "DELETE FROM tasks WHERE id = $1;",
            to_string(task_id)
        );

        W.commit();
        answer = "Задание с ID " + to_string(task_id) + " успешно удалено!\n";

    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка при удалении задания: " + string(e.what()) + "\n";
    }

    return answer;
}

// Получить правильный ответ по id задания
string get_task_correct_answer(connection &C, const string &task_id) {
    string answer;
    work W(C);

    try {
        // Используем exec_params для безопасной подстановки параметра
        result R = W.exec_params(
            "SELECT correct_answer FROM tasks WHERE id = $1;",
            task_id
        );

        if (R.empty()) {
            answer = "Верного ответа для задания с таким id нет!\n";
            W.commit();
            return answer;
        }

        string correct_answer = R[0][0].as<string>();
        answer = "ok|" + correct_answer;

        W.commit();
    } catch (const exception &e) {
        W.abort();
        answer = "Ошибка при получении ответа задания: " + string(e.what()) + "\n";
    }

    return answer;
}

// Получить список пользователей и их ролей
string get_roles_users(connection &C, const string &role_name) {
    stringstream resultStream;
    work W(C);
    try {
        string sql = "SELECT u.login, r.role_name FROM users u JOIN roles r ON u.role_id = r.id";
        if (!role_name.empty()) {
            sql += " WHERE r.role_name = " + W.quote(role_name);
        }
        sql += ";";

        result R = W.exec(sql);
        if (R.empty()) {
            resultStream << "error|Пользователей нет!\n";
        } else {
            for (const auto &row : R) {
                string login = row[0].as<string>();
                string role = row[1].as<string>();
                resultStream << login << "|" << role << "\n";
            }
        }

        W.commit();
    } catch (const exception &e) {
        W.abort();
        resultStream << "Ошибка получения пользователей и их ролей: " << e.what() << "\n";
    }
    return resultStream.str();
}

// Получить список ролей
string get_roles(connection &C) {
    stringstream resultStream;
    work W(C);

    try {
        string sql = "SELECT role_name FROM roles;";
        result R = W.exec(sql);

        if (R.empty()) {
            resultStream << "Роли отсутствуют!\n";
        } else {
            for (const auto &row : R) {
                resultStream << row[0].as<string>() << "\n";
            }
        }

        W.commit();
    } catch (const exception &e) {
        W.abort();
        resultStream << "Ошибка получения ролей: " << e.what() << "\n";
    }

    return resultStream.str();
}

// Получить роль по id
string get_role(connection &C, const string &user_id) {
    string res;
    work W(C);

    try {
        result R = W.exec_params(
            "SELECT r.role_name FROM roles r "
            "JOIN users u ON u.role_id = r.id WHERE u.id = $1;",
            user_id
        );

        if (R.empty()) {
            res = "Роль по user_id не найдена!\n";
        } else {
            res = R[0][0].as<string>() + "\n";
        }

        W.commit();
    } catch (const exception &e) {
        W.abort();
        res = "Ошибка получения роли: " + string(e.what()) + "\n";
    }

    return res;
}

// Получить список пользователей
string get_users(connection &C) {
    stringstream resultStream;
    work W(C);

    try {
        string sql = "SELECT login FROM users;";
        result R = W.exec(sql);

        if (R.empty()) {
            resultStream << "Пользователи отсутствуют!\n";
        } else {
            for (const auto &row : R) {
                resultStream << row[0].as<string>() << "\n";
            }
        }

        W.commit();
    } catch (const exception &e) {
        W.abort();
        resultStream << "Ошибка получения пользователей: " << e.what() << "\n";
    }

    return resultStream.str();
}

// Сохранить ответ на задание
string save_result(connection &C, const string &user_id, const string& task_id, const string &is_correct, const string &user_answer) {
    try {
        work W(C);

        W.exec_params(
            "INSERT INTO results (user_id, task_id, is_correct, user_answer) "
            "VALUES ($1, $2, $3, $4)",
            user_id,
            task_id,
            is_correct,
            user_answer
        );
        W.commit();

    } catch (const exception &e) {
        return e.what();
    }

    return "ok";
}

// Получение результатов для пользователя
string get_user_results(connection &C, const string &user_id, const string &topic_name) {
    stringstream resultStream;
    try {
        work W(C);

        result R;

        if (topic_name.empty()) {
            // Без фильтра по теме
            R = W.exec_params(
                "SELECT t.cipher_name, k.question, k.correct_answer, r.user_answer, r.is_correct, r.id "
                "FROM results r "
                "JOIN tasks k ON r.task_id = k.id "
                "JOIN topics t ON k.topic_id = t.id "
                "WHERE r.user_id = $1;",
                user_id
            );
        } else {
            // С фильтром по теме
            R = W.exec_params(
                "SELECT t.cipher_name, k.question, k.correct_answer, r.user_answer, r.is_correct, r.id "
                "FROM results r "
                "JOIN tasks k ON r.task_id = k.id "
                "JOIN topics t ON k.topic_id = t.id "
                "WHERE r.user_id = $1 AND t.cipher_name = $2;",
                user_id, topic_name
            );
        }

        if (R.empty()) {
            resultStream << "error|Результатов нет!\n";
        } else {
            auto sanitize = [](const string &s) {
                string out = s;
                out.erase(remove(out.begin(), out.end(), '\n'), out.end());
                out.erase(remove(out.begin(), out.end(), '\r'), out.end());
                replace(out.begin(), out.end(), '|', '/');
                return out;
            };

            for (const auto &row : R) {
                
                string topic = sanitize(row[0].as<string>());
                string task_text = sanitize(row[1].as<string>());
                string correct_answer = sanitize(row[2].as<string>());
                string user_answer = sanitize(row[3].as<string>());
                string is_correct = row[4].as<bool>() ? "TRUE" : "FALSE";
                string id = sanitize(row[5].as<string>());

                resultStream << topic << "|" << task_text << "|" << correct_answer
                             << "|" << user_answer << "|" << is_correct << "|" << id << "\n";
            }
        }

        W.commit();
    } catch (const exception &e) {
        resultStream << "error|Ошибка получения результатов: " << e.what() << "\n";
    }

    return resultStream.str();
}

// Получение результатов пользователя по логину
string get_user_results_for_admin(connection &C, const string &user_login, const string &topic_name) {
    stringstream resultStream;
    try {
        work W(C);

        result R;

        if (topic_name.empty()) {
            // Без фильтра по теме
            R = W.exec_params(
                "SELECT t.cipher_name, k.question, k.correct_answer, r.user_answer, r.is_correct, r.id "
                "FROM results r "
                "JOIN tasks k ON r.task_id = k.id "
                "JOIN topics t ON k.topic_id = t.id "
                "JOIN users u ON r.user_id = u.id "
                "WHERE u.login = $1;",
                user_login
            );
        } else {
            // С фильтром по теме
            R = W.exec_params(
                "SELECT t.cipher_name, k.question, k.correct_answer, r.user_answer, r.is_correct, r.id "
                "FROM results r "
                "JOIN tasks k ON r.task_id = k.id "
                "JOIN topics t ON k.topic_id = t.id "
                "JOIN users u ON r.user_id = u.id "
                "WHERE u.login = $1 AND t.cipher_name = $2;",
                user_login, topic_name
            );
        }

        if (R.empty()) {
            resultStream << "error|Результатов нет!\n";
        } else {
            auto sanitize = [](const string &s) {
                string out = s;
                out.erase(remove(out.begin(), out.end(), '\n'), out.end());
                out.erase(remove(out.begin(), out.end(), '\r'), out.end());
                replace(out.begin(), out.end(), '|', '/');
                return out;
            };

            for (const auto &row : R) {
                string topic = sanitize(row[0].as<string>());
                string task_text = sanitize(row[1].as<string>());
                string correct_answer = sanitize(row[2].as<string>());
                string user_answer = sanitize(row[3].as<string>());
                string is_correct = row[4].as<bool>() ? "TRUE" : "FALSE";
                string id = sanitize(row[5].as<string>());

                resultStream << topic << "|" << task_text << "|" << correct_answer
                             << "|" << user_answer << "|" << is_correct << "|" << id << "\n";
            }
        }

        W.commit();
    } catch (const exception &e) {
        resultStream << "error|Ошибка получения результатов: " << e.what() << "\n";
    }

    return resultStream.str();
}

// Функция удаления результатов пользователя по id результата
string delete_results(connection &C, const string &results_id) {
    if (results_id.empty()) return "error|Список id результатов пустой!";

    try {
        work W(C);
        set<string> res_id;

        stringstream ss(results_id);
        string token;
        while (getline(ss, token, ',')) {
            try { res_id.insert(token); } catch (...) {}
        }

        if (res_id.empty()) return "error|Нет корректных ID результатов!";

        for (string res : res_id) {
            W.exec_params(
                "DELETE FROM results WHERE id = $1",
                res
            );
        }

        W.commit();
        return "ok|Результаты успешно удалены!";
    }
    catch (const exception &e) {
        return string("error|Ошибка при удалении результатов: ") + e.what();
    }
}

string before_login(){
    string result;
    try{
        connection C("dbname=coursework user=postgres password=root host=db port=5432");
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

    // Разделяем сообщение по '|'
    getline(ss, command, '|');
    try {
        connection C("dbname=coursework user=postgres password=root host=db port=5432");
        if (command == "register"){
            string login;
            string passwd;
            getline(ss, login, '|');
            getline(ss, passwd, '|');
            result = add_user(C, login, passwd);
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
        else if (command == "deletetopic") {
            string cipher;
            getline(ss, cipher, '|');
            result = delete_topic(C, cipher);
        }
        else if (command == "get_topics") {
            result = get_all_topics(C);
        }
        else if (command == "get_roles") {
            result = get_roles(C);
        }
        else if (command == "get_role") {
            string user_id;
            getline(ss, user_id, '|');
            result = get_role(C, user_id);
        }
        else if (command == "get_users") {
            result = get_users(C);
        }
        else if (command == "addtask") {
            string cipher;
            string task;
            string answer;
            getline(ss, cipher, '|');
            getline(ss, task, '|');
            getline(ss, answer, '|');
            result = add_task(C, cipher, task, answer);
        }
        else if (command == "get_tasks") {
            string cipher;
            getline(ss, cipher, '|');
            result = get_tasks_for_topic(C, cipher);
        }
        else if (command == "deletetask") {
            string task_id;
            getline(ss, task_id, '|');
            result = delete_task(C, stoi(task_id));
        }
        else if (command == "changerole") {
            string username;
            string userrole;
            getline(ss, username, '|');
            getline(ss, userrole, '|');
            result = change_user_role(C, username, userrole);
        }
        else if (command =="roles_users") {
            string role;
            getline(ss, role, '|');
            result = get_roles_users(C, role);
        }
        else if (command == "get_topic_description") {
            string cipher_name;
            getline(ss, cipher_name, '|');
            result = get_topic_description(C, cipher_name);
        }
        else if (command == "get_topic_theory") {
            string cipher_name;
            getline(ss, cipher_name, '|');
            result = get_topic_theory(C, cipher_name);
        }
        else if (command == "get_user_id") {
            string login;
            getline(ss, login, '|');
            result = get_user_id(C, login);
        }
        else if (command == "get_task_correct_answer") {
            string task_id;
            getline(ss, task_id, '|');
            result = get_task_correct_answer(C, task_id);
        }
        else if (command == "save_result") {
            string user_id, task_id, is_correct, user_answer;
            getline(ss, user_id, '|');
            getline(ss, task_id, '|');
            getline(ss, is_correct, '|');
            getline(ss, user_answer, '|');
            result = save_result(C, user_id, task_id, is_correct, user_answer);
        }
        else if (command == "get_user_results") {
            string user_id;
            string topic;
            getline(ss, user_id, '|');
            getline(ss, topic, '|');
            result = get_user_results(C, user_id, topic);
        }
        else if (command == "get_user_results_for_admin") {
            string user_login;
            string topic;
            getline(ss, user_login, '|');
            getline(ss, topic, '|');
            result = get_user_results_for_admin(C, user_login, topic);
        }
        else if (command == "delete_results") {
            string results;
            getline(ss, results, '|');
            result = delete_results(C, results);
        }
        else {
            result = "Ошибка: неизвестная команда!\n";
        }
    } catch (const exception &e) {
            cerr << e.what() << endl;
            result = "Ошибка: проблема с подключением к базе данных!\n";
        }
    return result;
}