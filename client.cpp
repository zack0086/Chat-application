#include "client.h"
#include <poll.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstring>
#include <sys/sendfile.h>
using namespace std;
extern void buginfo(const char* f, ...);
extern int setnonblocking(int fd);
extern int ksrecieve(int fd, int sock, int tsz);

int Client::file_size(int fd) {
    struct stat s;
    if (fstat(fd, &s) == -1) {
        buginfo("\nfstat returned erro;");
        return -1;
    }
    return s.st_size;
}

// set int k to buf as network endian
int Client::set_uint32(char* s, int k) {
    union {
        char b[4];
        uint32_t d;
    } tmp;

    tmp.d = htonl(k);
    copy(tmp.b, tmp.b+4, s);

    return 0;
}

int Client::get_uint32(char* s) {
    union {
        char b[4];
        uint32_t d;
    } tmp;

    memcpy(&tmp.d, s, 4);

    return ntohl(tmp.d);
}

Client::Client() {
    td = 0;
}

void* Client::concurrent_hdl(void* obj) {
    return ((Client*)obj)->monitor();
}

int Client::run_concurrent() {
    if (td != 0) 
        pthread_cancel(td);
    pthread_create(&td, NULL, concurrent_hdl, this);

    return 0;
}

int Client::create_and_connect(const char* s, int len, int port) {
    if (sock!=0) close(sock);

    // create
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr;
    inet_pton(AF_INET, s, &serveraddr.sin_addr);
    serveraddr.sin_port = htons((unsigned short)port);
    serveraddr.sin_family = AF_INET;

    // connect
    if (connect(sock, (struct sockaddr*)&serveraddr, \
                sizeof(serveraddr)) < 0) {
        buginfo("Connect Failed!\n");
        return -1;
    }

    run_concurrent();
    return 0;
}

void Client::set_namelist(char* buf, int& offset, \
        vector<char*>& namev) {
    int naszst = offset;
    offset += 4;    
    int nvst = offset;
    for (const char* c: namev) {
        int tlen = strlen(c)+1;
        copy(c, c+tlen, buf+offset);
        offset += tlen;
    }

    // now we know the total name string size
    set_uint32(buf+naszst, offset-nvst);
}

int Client::send_file(const char* s, int len, bool block, vector<char*>& namev) {
    /*The header of the file-msg is fixed to be 
     * less than buf_size*/
    assert(len == strlen(s));
    char* buf = (char*)malloc(buf_size);
    int offset = 0;
    memset(buf, 0, sizeof(buf_size));

    // deal with file
    int file = open(s, O_RDONLY, 0644);
    if (file < 0) {
        buginfo("File not exist...\n");
        free(buf);
        return -1;
    }
    int file_sz = file_size(file);

    // type and flg
    buf[offset++] = 2;
    buf[offset++] = (block) ? 1:0;

    // write name lists
    set_namelist(buf, offset, namev); 

    // add file-size
    set_uint32(buf+offset, file_sz);
    offset += 4;

    // add file-name-sz and file-name
    set_uint32(buf+offset, len+1);
    offset += 4;

    copy(s, s+len+1, buf+offset);
    offset += len+1;
    
    // send to network
    int optval = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int));

    write(sock, buf, offset); 
    if (sendfile(sock, file, 0, file_sz) < 0 ) {
        buginfo("client send file failed, file_sz is : %d\n", file_sz);
        return -1;
    }

    optval = 0;
    setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int));

    free(buf);
    cout << "sending file successfully!" << endl;
    return 0;
}


int Client::send_msg(const char* s, int len, bool block, vector<char*>& namev) {
    /*The header of the file-msg is fixed to be 
     * less than buf_size*/
    buginfo("Entering send_msg\n");
    char* buf = (char*)malloc(buf_size*2);
    memset(buf, 0, sizeof(buf_size*2));
    int offset = 0;

    // type and flg
    buf[offset++] = 1;
    buf[offset++] = (block) ? 1:0;

    // name-size and name lists
    set_namelist(buf, offset, namev);

    // add msg-size and msg
    set_uint32(buf+offset, len+1); 
    offset += 4;
    copy(s, s+len+1, buf+offset);
    offset += len+1;

    // send to network
    write(sock, buf, offset);

    free(buf);
    cout << "sending message successfully!" << endl;
    buginfo("End send_msg\n");
    return 0;
}

int Client::send_reg(const char* s, int len) {
    assert(len == strlen(s));
    char* buf = (char*) malloc(100);
    int offset = 0;

    // type
    buf[offset++] = 0;

    // name sz and name string
    set_uint32(buf+offset, len+1);
    offset += 4;
    copy(s, s+len+1, buf+offset);
    offset += len+1;

    // send
    write(sock, buf, offset);

    free(buf);
    return 0;
}

void* Client::monitor() {
    setnonblocking(sock);
    epollfd = epoll_create1(0);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1 ) {
        buginfo("\nepoll_ctl, sock failed\n.");
        return 0;
    }

    while(true) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        for(int i=0; i<nfds; ++i) {
            if ((events[i].events & EPOLLERR) || (events[i].events \
                    & EPOLLHUP)) {
                /*In our case, this must be sth wrong*/
                buginfo("epoll error\n");
                close(events[i].data.fd);
                continue;
            } else if (events[i].data.fd == sock) { // new connection coming
                 buginfo("New data arrived!\n");
                if (process() < 0)
                    printf("Peer shutdown or rogue data\n");
            } else {

            }
        }
    }
}

int Client::process() {
    buginfo("Starting processsing...\n");
    int offset = 0; 
    char* buf = (char*) malloc(buf_size);
    memset(buf, 0, buf_size);

    offset += read(sock, buf, 1); 
    if (offset != 1) return -1;
    if (buf[0] == 1) { // it's message
        process_msg(buf, offset);
    } else if (buf[0] == 2) { // it's file
        process_file(buf, offset);
    }     
    buginfo("End processing\n");
    return 0;
}

void Client::process_file(char* buf, int offset) {
    buginfo("recieving file...\n");
    // get source name-size: 4 bytes
    int naszst = offset;
    while(offset < naszst+4)
        offset += read(sock, buf+offset, naszst+4-offset);
    int nasz = get_uint32(buf+naszst);

    // get source name: nasz bytes
    int nvst = offset;
    while(offset < nvst+nasz) 
        offset += read(sock, buf+offset, nvst+nasz-offset);

    // read file-size && file-name-size
    int fileszst = offset;
    while(offset < fileszst+8)
        offset += read(sock, buf+offset, fileszst+8-offset);
    int filesz = get_uint32(buf+fileszst);
    int fnamesz = get_uint32(buf+fileszst+4);

    // read file name
    int fnamevst = offset;
    while(offset < fnamevst+fnamesz) 
        offset += read(sock, buf+offset, fnamevst+fnamesz-offset);

    // open with this file
    int file = open(buf+fnamevst, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (file < 0) {
        printf("Got file: %s, but seems sth wrong in \
                creating it...", buf+fnamevst);
        return;
    }
    
    ksrecieve(file, sock, filesz);

    close(file);
    // print file info
    printf("Got file from %s:\n %s %d bytes\n", buf+nvst, \
            buf+fnamevst, filesz);
}

void Client::process_msg(char* buf, int offset) {
    buginfo("processing msg...\n");
     // get source name-size: 4 bytes
    int naszst = offset;
    while(offset < naszst+4)
        offset += read(sock, buf+offset, naszst+4-offset);
    int nasz = get_uint32(buf+naszst);

    // get source name: nasz bytes
    int nvst = offset;
    while(offset < nvst+nasz) 
        offset += read(sock, buf+offset, nvst+nasz-offset);

    // read msg size
    int msgszst = offset;
    while(offset < msgszst+4) 
        offset += read(sock, buf+offset, msgszst+4-offset);
    int msz = get_uint32(buf+msgszst);

    // read msg
    int msgst = offset;
    while(offset < msgst+msz) 
        offset += read(sock, buf+offset, msgst+msz-offset);

    // print msg info
    printf("Got Message from %s:\n %s\n", buf+nvst, buf+msgst); 
}

void Client::shutconn() {
    close(sock);
}
