#include "header.h"

using namespace std;

const int PORT = 8080;

// RAII-класс для автоматического закрытия сокета при выходе из области видимости
class RAII {
public:
    RAII(int fd) : fd_(fd) {}
    ~RAII() {
        if (close(fd_) < 0) {
            cerr << "Ошибка при закрытии сокета!" << endl;
        }
    }
    int get() const { return fd_; }
private:
    int fd_;
};

// Функция обработки подключённого клиента в отдельном потоке
void handle_client(shared_ptr<RAII> client_socket) {
    cout << "Клиент подключился!" << endl;

    // Создаем буфер на 1024 байт 
    char buffer[1024] = {0};
    int bytesReceived;

    try {
        // Обработка входящих данных от клиента
        while ((bytesReceived = read(client_socket->get(), buffer, sizeof(buffer) - 1)) > 0) {
            string input(buffer);
            cout << "Получен запрос: " << input << endl;

            // Передаем данные в функцию обработки
            string communication = communicate_with_client(input, client_socket->get());
            
            send(client_socket->get(), communication.c_str(), communication.size(), 0);
            cout << "Ответ отправлен клиенту!" << endl;

            memset(buffer, 0, sizeof(buffer)); // очищаем буфер
        }
        if (bytesReceived < 0) {
            throw runtime_error("Ошибка при чтении данных от клиента!");
        }
    } catch (const exception& e) {
        cerr << e.what() << endl;
    }

    cout << "Клиент отключен!" << endl;
}

void start_server() {
    // Создаём TCP-сокет
    RAII server_socket(socket(AF_INET, SOCK_STREAM, 0));
    if (server_socket.get() < 0) {
        throw runtime_error("Ошибка создания сокета!");
    }

    // Разрешаем повторное использование адреса и порта
    int opt = 1;
    if (setsockopt(server_socket.get(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        throw runtime_error("Ошибка установки параметров сокета!");
    }

    // Настраиваем структуру с адресом и портом сервера
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(server_socket.get(), (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw runtime_error("Ошибка привязки сокета!");
    }

    if (listen(server_socket.get(), 3) < 0) {
        throw runtime_error("Ошибка при прослушивании!");
    }

    string start_message = before_login(); 
    cout << start_message;

    // Цикл ожидания и обработки клиентов
    while (true) {
        struct sockaddr_in client_address;
        socklen_t addrlen = sizeof(client_address);
        int new_socket = accept(server_socket.get(), (struct sockaddr*)&client_address, &addrlen);
        if (new_socket < 0) {
            cerr << "Ошибка принятия соединения!" << endl;
            continue;
        }

        auto client_socket = make_shared<RAII>(new_socket); // создаем умный указатель на сокет
        thread client_thread(handle_client, client_socket);
        client_thread.detach(); // поток работает независимо от основного
    }
}

int main() {
    try {
        start_server();
    } catch (const exception& e) {
        cerr << e.what() << endl;
        return -1;
    }

    return 0;
}