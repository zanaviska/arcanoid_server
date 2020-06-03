#include "server.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <thread>
#include <future>
#include <array>
#include <utility>

using namespace std::chrono;

template<int I>
struct generate
{
    static const int val = (generate<I/2>::val >> 1) + generate<I%2>::val;
};

template<>
struct generate<0>
{
    static const int val = 0;
};

template<>
struct generate<1>
{
    static const int val = 1<<11;
};

template<size_t ... I>
int gen_imp(std::index_sequence<I...>, const int i)
{
    constexpr std::array<int, sizeof...(I)> a = { generate<I>::val... };
    return a[i];
}

int gen(const int i)
{
    return gen_imp(std::make_index_sequence<4096>(), i);
}

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
    int last_deleted = -1;
    constexpr std::array<double, 9> x_line{{0.015, 0.215, 0.8/3-0.015, 0.8/3+0.215, 1.6/3-0.15, 1.6/3+0.215, 0.785, 0.98, 100}};
    constexpr std::array<double, 19> y_line{{0.0075, 0.0725, 0.1275, 0.225-0.0075, 0.23+0.0075, 0.325-0.0075, 0.33+0.0075, 0.425-0.0075, 0.43+0.0075, 0.525-0.0075, 0.53+0.0075, 0.625-0.0075, 0.63+0.0075, 0.725-0.0075, 0.73+0.0075, 0.88-0.0075, 0.92+0.0075, 1-0.0075, 100}};
    int x_ind = 3;
    int y_ind = 8;
    double vx = 0;
    double vy = 0;
    int blocks = 0x00ffffff;
    std::atomic_bool cancel_token;
    long long last = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
    std::function<void(const long long)> change_pos = [&](const long long next_last) -> void
    {
        if(vx == 0 && vy == 0) return;
        while(last < next_last)
        {
            //std::cout << last << '!' << next_last << ' ' << vx << ' ' << vy << ' ' << x << ' ' << y << ' ' << x_ind << ' ' << y_ind << '\n';
            double dtx;
            double dty;
            if(vx > 0) dtx = (x_line[x_ind+1] - x)/vx; else dtx = (x_line[x_ind] - x)/vx;
            if(vy > 0) dty = (y_line[y_ind+1] - y)/vy; else dty = (y_line[y_ind] - y)/vy;

            if(std::min(dtx, dty) > next_last - last)
            {
                std::cout << "min\n";
                x += vx*(next_last - last)/1000;
                y += vy*(next_last - last)/1000;
                last = next_last;
                //paddle?
            } else if(dtx < dty)
            {
                //paddle?
                x += vx*dtx;
                y += vy*dtx;
                last += dtx*1000;
                if(vx > 0)
                {
                    std::cout << 'd';
                    if(x_ind == 6) vx = -vx;
                    else if(y_ind == 1 && pos1-0.125 <= x && x <= pos1+0.125)
                    {
                        x = 2*pos1-0.25 - x;
                        vx = -vx;
                    }
                    else if(y_ind == 15 && pos2-0.125 <= x && x <= pos2+0.125)
                    {
                        x = 2*pos2-0.25 - x;
                        vx = -vx;
                    }
                    else if(x_ind%2 && 0 < x_ind && y_ind%2 && 2 < y_ind && y_ind < 14 && (blocks&((1 << (x_ind+1)/2) << 4*(y_ind/2-1))))
                    {
                        vx = -vx;
                        last_deleted = (x_ind+1)/2 + 4*(y_ind/2-1);
                        blocks ^= ((1 << (x_ind+1)/2) << 4*(y_ind/2-1));
                    } else x_ind++;
                } else
                {
                    std::cout << 'a';
                    if(x_ind == 0) vx = -vx;
                    else if(y_ind == 1 && pos1-0.125 <= x && x <= pos1+0.125)
                    {
                        x = 2*pos1+0.25 - x;
                        vx = -vx;
                    }
                    else if(y_ind == 15 && pos2-0.125 <= x && x <= pos2+0.125)
                    {
                        x = 2*pos2+0.25 - x;
                        vx = -vx;
                    }
                    else if(x_ind%2 && x_ind < 6 && y_ind%2 && 2 < y_ind && y_ind < 14 && (blocks&((1 << (x_ind/2)) << 4*(y_ind/2-1))))
                    {
                        vx = -vx;
                        last_deleted = x_ind/2 + 4*(y_ind/2-1);
                        blocks ^= (1 << (x_ind/2)) << 4*(y_ind/2-1);
                    } else x_ind--;
                }
            } else
            {
                y += vy*dty;
                x += vx*dty;
                last += dty*1000;
                if(vy > 0)
                {
                    std::cout << 'w';
                    if(y_ind == 16) vy = -vy;
                    else if(y_ind == 0 && pos1-0.125 <= x && x <= pos1+0.125) vy = -vy;
                    else if(y_ind == 14 && pos2-0.125 <= x && x <= pos2+0.125) vy = -vy;
                    else if(x_ind%2 == 0 && y_ind%2 == 0 && 1 < y_ind && y_ind < 13 && (blocks&(1 << (x_ind/2 + (y_ind/2-1)*4))))
                    {
                        vy = -vy;
                        last_deleted = (x_ind/2 + (y_ind/2-1)*4);
                        blocks &= ~(1 << (x_ind/2 + (y_ind/2-1)*4));
                    } else y_ind++;
                } else
                {
                    std::cout << 's';
                    if(y_ind == 0) vy = -vy;
                    else if(y_ind == 2 && pos1-0.125 <= x && x <= pos1+0.125) vy = -vy;
                    else if(y_ind == 16 && pos2-0.125 <= x && x <= pos2+0.125) vy = -vy;
                    else if(x_ind%2 == 0 && y_ind%2 == 0 && 3 < y_ind && y_ind < 15 && (blocks&((1 << x_ind/2) << 4*(y_ind/2-2))))
                    {
                        vy = -vy;
                        last_deleted = (x_ind/2) + 4*(y_ind/2-2);
                        blocks ^= (1 << x_ind/2) << 4*(y_ind/2-2);
                    } else y_ind--;
                }
            }
            std::cout << x_ind << ' ' << y_ind << ' ' << x << ' ' << y << '\n';
        }
    };
    int cnt = 0;
    set_interval(cancel_token, 3000, [&]() -> void
    {
        cnt++;
        if(cnt == 2) vx = 0.2, vy = 0.11, last = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
        //if(vx == 0 && vy == 0) vx = 0.8, vy = 0.4;
        std::cout << vx << ' ' << vy << ' ' << x << ' ' << y << '\n';
        const long long next_last = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
        change_pos(next_last);
        if(!blocks)
        {
            cancel_token.store(false);
            std::string q{"update users set wins = wins + 1 WHERE id=" + std::to_string(last_deleted%2 ? id1 : id2)};
            std::cout << q << '\n';
            pqxx::connection c("dbname=user host=localhost user=admin password=admin");
            pqxx::work w(c);
            pqxx::result r = w.exec(q);
            w.commit();
            return;
        }
        std::cout << "interval " << blocks << ' ' << (gen(blocks>>12))+(gen(blocks%(1<<12))<<12) << ' ' << x << ' ' << y << ' ' << x_ind << ' ' << y_ind << "\n";

        std::string resp{"{\"x\": " + std::to_string(x) + ", \"y\": " + std::to_string(y) +
                         ", \"vx\": " + std::to_string(vx) + ", \"vy\": " + std::to_string(vy) + ", \"blocks\": " + std::to_string(blocks) +
                         ", \"pos\": " + std::to_string(pos2) + ", \"dest\": " + std::to_string(1-dest2) + ", \"date\": " + std::to_string(last) + "}"};
        //std::cout << "\n\n" << resp << "\n\n";
        sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client1), sizeof(sockaddr_in));
        resp = "{\"x\": " + std::to_string(1-x) + ", \"y\": " + std::to_string(1-y) +
               ", \"vx\": " + std::to_string(-vx) + ", \"vy\": " + std::to_string(-vy) + ", \"blocks\": " + std::to_string((gen(blocks>>12))+(gen(blocks%(1<<12))<<12)) +
               ", \"pos\": " + std::to_string(1-pos1) + ", \"dest\": " + std::to_string(1-dest1) + ", \"date\": " + std::to_string(last) + "}";
        //std::cout << "\n\n" << resp << "\n\n";
        sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client2), sizeof(sockaddr_in));
    });
    sockaddr * target;
    while(blocks)
    {
        std::string buf(1024, 0);
        int ln = recvfrom(listener, buf.data(), 1024, 0, reinterpret_cast<sockaddr *>(&client), &addr_size);
        buf[ln] = 0;
        std::stringstream ss;
        ss << buf.data();
        std::cout << "@@@@@" << buf.data() << '\n';
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
        const long long next_last = 2*duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() - date;
        std::cout << "main while\n";
        change_pos(next_last);
        std::string resp;
        if(id1 == id)
        {
            client1 = client;
            pos1 = pos;
            target = reinterpret_cast<sockaddr *>(&client2);
            resp = std::string("{\"x\": " + std::to_string(1-x) + ", \"y\": " + std::to_string(1-y) +
                               ", \"vx\": " + std::to_string(-vx) + ", \"vy\": " + std::to_string(-vy) + ", \"blocks\": " + std::to_string((gen(blocks>>12))+(gen(blocks%(1<<12))<<12)) +
                               ", \"pos\": " + std::to_string(1-pos) + ", \"dest\": " + std::to_string(1-dest) + ", \"date\": " + std::to_string(date) + "}");
        }
        else
        {
            client2 = client;
            pos2 = 1-pos;
            target = reinterpret_cast<sockaddr *>(&client1);
            resp = std::string("{\"x\": " + std::to_string(x) + ", \"y\": " + std::to_string(y) +
                               ", \"vx\": " + std::to_string(vx) + ", \"vy\": " + std::to_string(vy) +", \"blocks\": " + std::to_string(blocks) +
                               ", \"pos\": " + std::to_string(1-pos) + ", \"dest\": " + std::to_string(-dest) + ", \"date\": " + std::to_string(date) + "}");
        }
        //std::cout << "\nresp\n" << resp << "\n\n";
        //std::cout << buf.data() << '\n';
        sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, target, sizeof(sockaddr_in));
    }
    std::string q{"update users set wins = wins + 1 WHERE id=" + std::to_string(last_deleted%2 ? id1 : id2)};
    pqxx::connection c("dbname=user host=localhost user=admin password=admin");
    pqxx::work w(c);
    pqxx::result r = w.exec(q);
    w.commit();
    cancel_token.store(false);
}

void main_handle()
{
    int port = 35662;
    int listener = socket(AF_INET, SOCK_DGRAM, 0);
    system("ifconfig -a");
    sockaddr_in me;
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
            }
            pqxx::work w(c);
            std::string q{"INSERT INTO users (username, password) VALUES ('" + username + "', " + std::to_string(static_cast<long long>(std::hash<std::string>{}(password))) + ")"};
            pqxx::result r = w.exec(q);
            w.commit();
            std::cout << q << '\n';
            std::string resp{"{\"status\": \"OK\", \"date\": " + date + "}"};
            sendto(listener, resp.data(), resp.length(), MSG_CONFIRM, reinterpret_cast<sockaddr *>(&client), addr_size);
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
                std::string q{"update users set games = games + 1 WHERE id=" + std::to_string(q_id) + " OR id=" + std::to_string(id)};
                pqxx::work w(c);
                pqxx::result r = w.exec(q);
                w.commit();
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
