#include "server.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <thread>
#include <future>

using namespace std::chrono;

template<class F, class... Args>
void set_interval(std::atomic_bool& cancel_token, size_t interval, F&& f, Args&... args)
{
    cancel_token.store(true);
    auto cb = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    std::thread([=, &cancel_token]()mutable
    {
        while(cancel_token.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            cb();
        }
    }).detach();
}


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
    //set_interval(b1, 1000, printf, "hi");
    sockaddr_in client;
    socklen_t addr_size = sizeof(client);
    double pos1 = 0.5;
    double pos2 = 0.5;
    double dest1 = 0.5;
    double dest2 = 0.5;
    double x = 0.5;
    double y = 0.5;
    double vx = 0;
    double vy = 0;
    int blocks = 0x00ffffff;
    std::atomic_bool b1;
    int cnt = 0;
    set_interval(b1, 3000, [&]()
    {
        //cnt++;
        if(vx == 0 && vy == 0) vx = 0.8, vy = 0.4;
        std::cout << vx << ' ' << vy << ' ' << x << ' ' << y << '\n';
    });
    sockaddr * target;
    while(1)
    {
        std::string buf(1024, 0);
        int ln = recvfrom(listener, buf.data(), 1024, 0, reinterpret_cast<sockaddr *>(&client), &addr_size);
        buf[ln] = 0;
        std::stringstream ss;
        ss << buf;
        int id;
        long long date;
        double dest;
        double pos;
        std::string str;
        for(int i = 0; i < 4; i++)
        {
            ss >> str;
            if(str == "id") ss >> id;
            if(str == "dest") ss >> dest;
            if(str == "pos") ss >> pos;
            if(str == "date") ss >> date;
        }
        long long last = 2*duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() - date;
        //auto now = duration_cast< milliseconds >(system_clock::now().time_since_epoch());
        //std::cout << date << ' ' << last << '\n';

        dest = !!(dest-pos)*0.05 + pos;
        std::string resp;
        if(id1 == id)
        {
            client1 = client;
            target = reinterpret_cast<sockaddr *>(&client2);
            resp = std::string("{\"x\": " + std::to_string(x) + ", \"y\": " + std::to_string(y) +
                    ", \"vx\": " + std::to_string(vx) + ", \"vy\": " + std::to_string(vy) + ", \"blocks\": " + std::to_string(blocks) +
                    ", \"pos\": " + std::to_string(1-pos) + ", \"dest\": " + std::to_string(1-dest) + ", \"date\": " + std::to_string(date) + "}");

        }
        else
        {
            client2 = client;
            target = reinterpret_cast<sockaddr *>(&client1);
            resp = std::string("{\"x\": " + std::to_string(1-x) + ", \"y\": " + std::to_string(1-y) +
                               ", \"vx\": " + std::to_string(-vx) + ", \"vy\": " + std::to_string(-vy) +", \"blocks\": " + std::to_string(blocks) +
                               ", \"pos\": " + std::to_string(1-pos) + ", \"dest\": " + std::to_string(-dest) + ", \"date\": " + std::to_string(date) + "}");
        }
        std::cout << buf.data() << '\n';
        sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, target, sizeof(sockaddr_in));
    }
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
                //std::async(std::launch::async, play_game, q_id, q_client, id, client, new_port);
                std::thread(play_game, q_id, q_client, id, client, new_port).detach();
                std::cout << "Does?";
                q_id = -1;
            }
        }

    }
}
