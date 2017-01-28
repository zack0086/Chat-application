#include <iostream>
#include <vector>
#include <pthread.h>
#include <sys/epoll.h>
#include "common.h"
using namespace std;

class Client {
private:
    int sock, nfds, epollfd;
    struct epoll_event ev, events[MAX_EVENTS];
private:
    static void* concurrent_hdl(void* context);
    int run_concurrent();
    int process();
    pthread_t td;
    int file_size(int fd);
    int set_uint32(char* s, int k);
    int get_uint32(char* s);
    void set_namelist(char* buf, int& offset, vector<char*>& namev);
    void process_file(char* buf, int offset);
    void process_msg(char* buf, int offset);
    void* monitor();
public:
    Client();
    int create_and_connect(const char* s, int len, int port);
    int send_file(const char* s, int len, bool block, vector<char*>& namev); 
    int send_msg(const char*s, int len, bool block, vector<char*>& namev);
    int send_reg(const char* s, int len);
    void shutconn();
};
