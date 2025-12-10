FROM ubuntu:24.04

ARG UID
ARG GID
ARG TZ

# Установка зависимостей
RUN apt-get update && apt-get install -y \
    g++ \
    libpq-dev \
    libpqxx-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Создание пользователя
RUN groupadd -g ${GID} tester && \
    useradd -m -u ${UID} -g tester tester

# Рабочая директория
WORKDIR /app

# Папка для конфига
RUN mkdir -p /app/config

# Копирование исходников
COPY server /app/server

COPY config/config.json /app/config/config.json

# Права
RUN chown -R tester:tester /app

# Временная зона
ENV TZ=${TZ}

# Переключение на пользователя
USER tester

# Компиляция
WORKDIR /app/server
RUN g++ server.cpp sql_functions.cpp -o server -lpqxx -lpq -lcrypto

# Запуск
CMD ["./server"]