# arcanoid_server

to run

create postgreSQL table "users" and user "admin" with password "admin" who can read and write into table

```
mkdir build
cd build
cmake ..
make
./server
```

Entry point of program is located in main.cpp

Declaration of all available for usage function is located in server.h file

function initialization is located in server.cpp file

# API

void main_handle(int port)

create server, that receive requsts from client using ***port*** port. Default value of ***port*** is 35662


void play_game(long long id1, sockaddr_in client1, long long id2, sockaddr_in client2, int port);

create server, that allow players with id ***id1*** and ***id2*** to interact with each other using server port ***port***
