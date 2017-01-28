#include <string>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <cstdio>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdint.h>
#include "common.h"
/*const int MAX_EVENTS = 100;
const int MAX_THREAD_NUMS = 10;
const int maxnode = 1e3+10;
const int sigma_size = 26;
const int def_port = 8087;
const int max_file_sz = 1e6 + 10;
const int buf_size = 1024;*/

using namespace std;

int debug = false;
void buginfo(const char* f, ...) {if(!debug)return;va_list al; va_start(al, f);vprintf(f, al);va_end(al);}

char* str2raw(string& s) {
    char* ctx = (char*)malloc(s.size()+1);
    copy(s.begin(), s.end(), ctx);
    ctx[s.size()] = 0;

    return ctx;
}

int setnonblocking(int fd) {
   int flg;
   flg = fcntl(fd, F_GETFL, 0);
   if (flg == -1) {
       buginfo("Fuck you\n");
       return -1;
   }
   flg |= SOCK_NONBLOCK;
   if (fcntl(fd, F_SETFL, flg) == -1)  {
       buginfo("Fuck you fcntl.\n");
   }

   return 0;
}

int set_uint32(char* s, int k) {
    union {
        char b[4];
        uint32_t d;
    } tmp;

    tmp.d = htonl(k);
    copy(tmp.b, tmp.b+4, s);

    return 0;
}

int get_uint32(char* s) {
    union {
        char b[4];
        uint32_t d;
    } tmp;

    memcpy(&tmp.d, s, 4);

    return ntohl(tmp.d);
}

int ksrecieve(int fd, int sock, int tsz) {
    buginfo("in ksrecieve, tsz is: %d\n", tsz);
    char* buf = (char*) malloc(buf_size);
    memset(buf, 0, buf_size);
    
    while(tsz) {
        int cnt = read(sock, buf, min(tsz, buf_size));
        tsz -= cnt;
        if (write(fd, buf, cnt) < 0) {
            perror("ksrecieve erro:");
            return -1;
        }
    }

    free(buf);
    return 0;
}
