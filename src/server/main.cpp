#include <iostream>

#include "server.hpp"
#include "client.hpp"

int main(int argc, char **argv)
{
    Server server;
    int portNumber = argc > 1 ? atoi(argv[1]) : 3000;
    server.SetPortNumber(portNumber);
    server.Start();
    return 0;
}
