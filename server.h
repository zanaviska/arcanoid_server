#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <pqxx/pqxx>
#include <functional>

    void play_game(long long id1, sockaddr_in client1, long long id2, sockaddr_in client2, int port);
    void main_handle();

#endif //SERVER_SERVER_H
