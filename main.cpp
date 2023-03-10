#include "server.hpp"

int main()
{
    Server myAsewsomeServer;
    try
    {
        myAsewsomeServer.Run();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}