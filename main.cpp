#include "server.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <thread>
#include <future>

using namespace std;

void play_game(long long id1, sockaddr_in client1, long long id2, sockaddr_in client2, int port)
{
    std::cout << "logged\n";
    int listener = socket(AF_INET, SOCK_DGRAM, 0);//
    sockaddr_in me;//
    memset(&me, 0, sizeof(me));
    me.sin_family = AF_INET;
    me.sin_port = htons(port);
    me.sin_addr.s_addr = inet_addr("0.0.0.0");
    bind(listener, reinterpret_cast<const sockaddr *>(&me), sizeof(me));
    std::string resp{"{\"port\": " + std::to_string(port) + "}"};
    sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client1), sizeof(sockaddr_in));
    sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client2), sizeof(sockaddr_in));
}

int main()
{
    int port = 35662;//
    int listener = socket(AF_INET, SOCK_DGRAM, 0);//
    system("ifconfig -a");
    sockaddr_in me;//
    sockaddr_in client;
    char buffer[1024];
    memset(&me, 0, sizeof(me));
    me.sin_family = AF_INET;
    me.sin_port = htons(port);
    me.sin_addr.s_addr = inet_addr("0.0.0.0");
    bind(listener, reinterpret_cast<const sockaddr *>(&me), sizeof(me));
    socklen_t addr_size = sizeof(client);
    std::string buf(1024, 0);
    int ln;
    char service[NI_MAXSERV];
    char host[NI_MAXHOST];
    if (getnameinfo((sockaddr *) &client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
    {
        std::cout << host << " connected on port " << service << '\n';
    } else
    {
        inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
        std::cout << "????" << host << " connected on port " << ntohs(client.sin_port) << '\n';
    }
    pqxx::connection c("dbname=user host=localhost user=admin password=admin");
    int q_id = -1;
    sockaddr_in q_client;
    std::string q_date;
    while (1)
    {
        ln = recvfrom(listener, buf.data(), 1024, 0, reinterpret_cast<sockaddr *>(&client), &addr_size);
        buf[ln] = 0;
        std::cout << buf.data() << '\n';
        std::stringstream ss;
        ss << buf.data();
        std::string type;
        ss >> type;
        if(type == "login")
        {
            std::string username;
            std::string password;
            std::string date;
            std::string str;
            for(int i = 0; i < 3; i++)
            {
                ss >> str;
                if(str == "username") ss >> username;
                if(str == "password") ss >> password;
                if(str == "date") ss >> date;
            }
            std::cout << username << '|' << password << "|\n";
            pqxx::work w(c);
            std::string q{"SELECT * FROM users WHERE username='" + username + "' AND PASSWORD=" + std::to_string(static_cast<long long>(std::hash<std::string>{}(password)))};
            std::cout << q << '\n';
            pqxx::result r = w.exec(q);
            w.commit();
            std::string resp;
            if(r.empty())
                resp = "{\"status\": \"failed\", \"date\": " + date + "}";
            else
                resp = "{\"status\": \"OK\", \"id\": " + r[0][4].as<std::string>() + ", \"date\": " + date + ", \"wins\": " + r[0][3].as<std::string>() + ", \"games\": " + r[0][2].as<std::string>() + "}";
            sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client), addr_size);
        }
        else if(type == "registration")
        {
            std::string username;
            std::string password;
            std::string date;
            std::string str;
            for(int i = 0; i < 3; i++)
            {
                ss >> str;
                if(str == "username") ss >> username;
                if(str == "password") ss >> password;
                if(str == "date") ss >> date;
                pqxx::work w(c);
                std::string q{"INSERT INTO users (username, password) VALUES ('" + username + "', " + std::to_string(static_cast<long long>(std::hash<std::string>{}(password))) + ")"};
                std::cout << q << '\n';
                std::string resp{"{}"};
                sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client), addr_size);
            }
        }
        else if (type == "start")
        {
            long long id;
            std::string date;
            std:: string s;
            for(int i = 0; i < 2; i++)
            {
                ss >> s;
                if(s == "date") ss >> date;
                if(s == "id") ss >> id;
            }
            //ss >> date >> id >> date >> date;
            if(q_id == id) continue;
            std::cout << "!!!!!! " << id << '\n';
            if(q_id == -1)
            {
                q_id = id;
                q_client = client;
                q_date = date;
            }
            else
            {
                //std::thread(play_game, q_id, q_client, id, client).detach();
                int new_port = 40000 + rand()%10000;
                std::string resp{"{\"port\": " + std::to_string(new_port) + ", \"date\": " + date + "}"};
                sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client), addr_size);
                resp = "{\"port\": " + std::to_string(new_port) + ", \"date\": " + q_date + "}";
                sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&q_client), addr_size);
                std::async(play_game, q_id, q_client, id, client, new_port);

                q_id = -1;
            }
        }

    }
}
