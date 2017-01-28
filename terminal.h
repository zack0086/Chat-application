#include <iostream>
#include <set>
#include <string>
#include "client.h"
#include "server.h"

class Terminal {
private:
    Client client;
    Server server;
    set<string> mblock;
    set<string> mblock_bc;
    int status; //0-non-set 1-server 2-client
    void mblock2raw(vector<char*>& res);
    void mblock_bc2raw(vector<char*>& res);
private:
    int parse(string line);
    void parseClient(vector<string>& v);
    int blockcast_hdl(vector<string>& v);
    int unicast_hdl(vector<string>& v);
    int broadcast_hdl(vector<string>& v);
    void correctUsage();
public:
    int run();
    Terminal();

};
