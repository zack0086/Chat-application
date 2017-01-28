#include <iostream>
#include <thread>
#include <errno.h>
#include <chrono>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <set>
#include <sys/sendfile.h>
#include "server.h"

extern void buginfo(const char* f, ...);
extern int set_uint32(char* s, int k);
extern int get_uint32(char* s);
extern int ksrecieve(int fd, int sock, int tsz);

using namespace std;

class Trie {
public:
    int ch[maxnode][sigma_size];
    int val[maxnode], sz, fds[maxnode];
    inline void clear() { sz=1; memset(ch[0], 0, sizeof(ch[0]));}
    inline int idx(char c) { return c - '0';}
    void insert(char* s, int len, int fd);
    int find(char* s, int len);
    void del(char* s, int len);
};

void Trie::insert(char* s, int len, int fd) {
    int u = 0;
    for(int i=0; i<len; ++i) {
        int c = idx(s[i]);
        if (!ch[u][c]) {
            memset(ch[sz], 0, sizeof(ch[sz]));
            ch[u][c] = sz++;
        }
        u = ch[u][c];
        ++val[u];
    }
    fds[u] = fd; 
}

int Trie::find(char* s, int len) {
    int u = 0;
    for(int i=0; i<len; ++i) {
        int c = idx(s[i]);
        if (!ch[u][c]) return -1;
        u = ch[u][c];
    }
    return fds[u];
}

void Trie::del(char* s, int len) {
    int u = 0;
    for(int i=0; i<len; ++i) {
        int c = idx(s[i]);
        int tu = ch[u][c]; 
        if (--val[ch[u][c]]<=0) ch[u][c]=0;
        u = tu;
    }
    fds[u] = -1;
}

Server::Server() {
    //trie = new Trie;
    td = 0;
}

void* Server::concurrent_hdl(void* obj) {
    return ((Server*)obj)->monitor();    
}

Server::~Server() {
    //delete trie;
    if (td!=0)
        pthread_cancel(td);
}

int Server::start() {
    // re-init, close fds
    if (lsnfd) close(lsnfd);
    if (connfd) close(connfd);
    if (nfds) close(nfds);
    if (epollfd) close(epollfd);

    // clear db
    trie.clear();
    fd2str.clear();

    setup_listen();    

    run_concurrent(&Server::monitor);


    return 0;
}

int Server::stop() {

    return 0;
}

int Server::run_concurrent(Mfunc f) {
    if (td != 0)
        pthread_cancel(td);
    pthread_create(&td, NULL, concurrent_hdl, this);

    return 0;
}

int Server::setnonblocking(int fd) {
    int flg;
    flg = fcntl(fd, F_GETFL, 0);
    if (flg == -1) {
        buginfo("Fuck you.\n");
        return -1;
    }
    flg |= SOCK_NONBLOCK;
    if (fcntl(fd, F_SETFL, flg) == -1) {
        buginfo("Fuck you fcntl.\n");
    }

    return 0;
}

int Server::file_size(int fd) {
    struct stat s;
    if (fstat(fd, &s) == -1) {
        buginfo("\nfstat error;\n");
        return -1;
    }
    return s.st_size;
}

int Server::local_ip_address(struct sockaddr_in* res, int port) {
    struct ifaddrs* addrs;
    getifaddrs(&addrs);
    
    struct sockaddr_in* cur_addr = NULL;
    for (struct ifaddrs* p=addrs; p->ifa_next; p=p->ifa_next) {
        if (p->ifa_addr->sa_family==AF_INET && strcmp(p->ifa_name, "en0")) {
            cur_addr = (struct sockaddr_in*) p->ifa_addr;
        }
    }

    if (cur_addr == 0) return -1;

    cur_addr->sin_port = htons(port);
    cur_addr->sin_family = AF_INET;
    char s[48];
    inet_ntop(AF_INET, &cur_addr->sin_addr.s_addr, s, 30);
    buginfo("local ip addrss is: %s\n", s);
    
    //memcpy(res, cur_addr, sizeof(struct sockaddr_in));
    memcpy(&res->sin_addr.s_addr, &cur_addr->sin_addr.s_addr, sizeof(cur_addr->sin_addr.s_addr));

    freeifaddrs(addrs);
    return 0;
}

int Server::setup_listen () {
    char s[50];
    if (lsnfd != 0) close(lsnfd);

    lsnfd = socket(AF_INET, SOCK_STREAM, 0);
    // set reusable
    int optval = 1;
    setsockopt(lsnfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 

    // set family, port && ip-address
    struct sockaddr_in sv_addr;
    memset(&sv_addr, 0, sizeof(sv_addr));
    sv_addr.sin_port = htons((unsigned short)def_port);
    sv_addr.sin_family = AF_INET;
    if (local_ip_address(&sv_addr, def_port) < 0) {
        buginfo("\nFailed to get IP address, check your network!\n");
        return -1;
    }

    // binding...
    if (bind(lsnfd, (struct sockaddr*) &sv_addr, sizeof(sv_addr)) < 0) {
        printf("\n Error in Binding lsnfd...\n");
        memset(s, 0, 50);
        inet_ntop(AF_INET, &sv_addr.sin_addr, s, 30);
        buginfo("local ip addrss is: %s\n", s);
        return -1;
    }

    if (listen(lsnfd, 10) < 0) {
        printf("\nFailed in listening...\n");
        return -1;
    }

    // successful
    inet_ntop(AF_INET, &sv_addr.sin_addr, s, 30);
    printf("\nsuccessfully Listening %s %d\n", s, ntohs(sv_addr.sin_port)); 

    return 0;
}

// monitor the fds, process incoming data
void* Server::monitor (void) {
    
    epollfd = epoll_create1(0);
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = lsnfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, lsnfd, &ev) == -1 ) {
        buginfo("\nepoll_ctl, lsnfd failed\n.");
        return 0;
    }

    while(true) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        for(int i=0; i<nfds; ++i) {
            if ((events[i].events & EPOLLERR) || (events[i].events \
                    & EPOLLHUP)) {
                /*In our case, this must be sth wrong*/
                buginfo("epoll error or peer closed connection\n");
                close(events[i].data.fd);
                delfd(events[i].data.fd);
                continue;
            }
            if (events[i].events & EPOLLRDHUP) {
                buginfo("A peer closed connection!\n");
                close(events[i].data.fd);
                delfd(events[i].data.fd);
                continue;
            }
            if (events[i].data.fd == lsnfd) { // new connection coming
                struct sockaddr_in addr;
                socklen_t len = sizeof(sockaddr_in);
                connfd = accept(lsnfd, (struct sockaddr*)&addr, &len); 
                if (connfd == -1) {
                    buginfo("accepted wrong connfd...\n");
                    return 0;
                } else {
                    printf("Accepted new connection!\n");
                }
                setnonblocking(connfd); //ET needs non-blocking
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connfd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                    buginfo("Config error in connfd...\n");
                    return 0;
                }
            } else {
                buginfo("New data arrived!\n");
                if (process(events[i].data.fd) < 0) {
                    printf("Peer shutdown or rogue data\n");
                }
            }
        }
    }

    return 0;
}

// do processing while new data coming.
int Server::process(int sock) {
    buginfo("Starting processing...\n");
    // extract header;
    char* buf = (char*)malloc(buf_size);
    memset(buf, 0, buf_size);
    int offset = 0;

    // read first byte
    if (read(sock, buf+offset, 1) <= 0) {
        buginfo("Peer down or rogue data.\n");
        return -1; 
    }
    offset += 1;
    
    assert(offset == 1);

    // decide type
    if (buf[0] == 0) 
        process_reg(sock, buf, offset);
    else if (buf[0] == 1) 
        process_msg(sock, buf, offset);
    else if (buf[0] == 2)
        process_file(sock, buf, offset);
    else {
        // something must be wrong.
        printf("Recieved rogue data: \n");
        printf("%c", buf[0]);
        while(true) {
            memset(buf, 0, buf_size);
            offset = 0;

            int b = read(sock, buf, buf_size);
            if (b <= 0) break;

            for (int i=0; i<b; ++i) 
                printf("%c", buf[i]);
        }
        //printf("fd2str size is: %d\n", fd2str.size());
        buf[0] = 'f'; buf[1] = 'u';
        for (auto c: fd2str) {
            int fd = c.first;
            write(fd, buf, 2);
            int res = write(sock, buf, 2);
            printf("res is: %d\n", res);
        }
    }

    free(buf);
    buginfo("completed processing msg\n");

    return 0;
}

int Server::get_namelist(int sock, char* buf, int& offset, vector<int>& ans) {
    // read flg && name-strings-size: 5 bytes total 
    int flgst = offset;
    int szst = offset+1;
    while(offset < flgst+5)
        offset += read(sock, buf+offset, flgst+5-offset);
    int sz = get_uint32(buf+szst);
    int flg = buf[flgst]; // flg indicates whether it's blacklist

    // read name strings 
    int strst = offset;
    while(offset < strst+sz) 
        offset += read(sock, buf+offset, strst+sz-offset);
    
    // query from trie, get fds
    set<int> tfds;
    for(int i=strst; i<strst+sz; ) {
        while(i<strst+sz && buf[i]==0) ++i;
        if (i == strst+sz) break;
        string z = string(buf+i);
        i += z.size();

        //cout << "z is: " << z << endl;
        auto c = trie.find(z);
        if (c != trie.end()) tfds.insert(c->second);
        
    }
    
        
    // write ans
    ans.clear();
    if (flg == 0) {
        ans.resize(tfds.size());
        copy(tfds.begin(), tfds.end(), ans.begin());
    } else if(flg == 1) {
        for (auto it: fd2str) {
            int p = it.first;
            if (tfds.count(p) > 0) continue;
            ans.push_back(p);
        }
    }
      
    return 0;
}

int Server::del_fdinfo(int fd) {
    if (fd2str.count(fd) == 0) return 0;
    auto c = fd2str.find(fd);
    char* p = c->second;
    trie.erase(string(p));
    fd2str.erase(c);
    free(p);
    return 0;

}

int Server::delfd(int fd) {
    del_fdinfo(fd);
    close(fd);

    return 0;
}

int Server::process_reg(int sock, char* buf, int offset) {
    buginfo("Processing register info...\n");
    // read name size
    int naszst = offset;
    while(offset < naszst+4) 
        offset += read(sock, buf+offset, naszst+4-offset);
    int nasz = get_uint32(buf+naszst);
    
    // read name string
    int nast = offset;
    while(offset < nast + nasz) // nasz included the last '\0'
        offset += read(sock, buf+offset, nast+nasz-offset); 
    
    // check if this is a rename
    if (fd2str.count(sock) > 0) {
        del_fdinfo(sock);
    }

    // update information
    trie[string(buf+nast)] = sock;
    char* str = (char*) malloc(nasz);
    copy(buf+nast, buf+nast+nasz, str);
    fd2str.insert(make_pair(sock, str));
   
    return 0;
}


int Server::process_msg(int sock, char* buf, int offset) {
    buginfo("processing msg...\n");
    vector<int> name_fds;
    get_namelist(sock, buf, offset, name_fds); 

    // msg size
    buginfo("In msg size...\n");
    int msgszst = offset;
    while(offset < msgszst+4)
        offset += read(sock, buf+offset, msgszst+4-offset);
    int tsz = get_uint32(buf+msgszst);

    // read all
    buginfo("read contexts...\n");
    int msgst = offset;
    while(offset < msgst+tsz) 
        offset += read(sock, buf+offset, msgst+tsz-offset);

    // send msg to net.
    buginfo("Send data to network...\n");
    for(auto fd: name_fds) {
        write(fd, buf, 1); // 1 byte type 

        // add sender's name
        char* p = fd2str[sock];
        int plen = strlen(p)+1; // inlcude last '\0'
        int32_t a = htonl(plen);
        write(fd, &a, 4);  
        write(fd, p, plen);

        // msg size && msg
        int32_t t = htonl(tsz);
        write(fd, &t, 4);
        write(fd, buf+msgst, tsz); 
    }

    buginfo("Job done...\n");
    return 0;
}

int Server::process_file(int sock, char* buf, int offset) {
    buginfo("Processing file...\n");
    vector<int> name_fds;
    get_namelist(sock, buf, offset, name_fds);

    // file size and its' name-size
    int fszst = offset;
    offset += read(sock, buf+offset, 8);
    int32_t tsz = get_uint32(buf+fszst);
    int32_t nasz = get_uint32(buf+fszst+4);

    // get file-name
    int namest = offset;
    while(offset < namest+nasz)
        offset += read(sock, buf+offset, namest+nasz-offset); 

    // save file
    int filest = offset;
    int file = open(buf+namest, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (file < 0) {
        buginfo("\n Failed to open file: %s\n", buf+namest);
        return -1;
    }
    if (ksrecieve(file, sock, tsz) < 0) {
        perror("Save file failed");
    }

    // assemble segment && send file
    file = open(buf+namest, O_RDONLY, 0666);
    assert(tsz == file_size(file));
    if (file < 0) {
        buginfo("\n Re-open file %s error!\n", buf+namest);
        return -1;
    }

    // construct headers
    char* obuf = (char*)malloc(buf_size);
    offset = 0;
    memset(obuf, 0, buf_size);
    obuf[offset++] = 2; // file type

    // source name size
    char* p = fd2str[sock];
    int plen = strlen(p)+1;
    set_uint32(obuf+offset, plen); 
    offset += 4;

    // source name
    copy(p, p+plen, obuf+offset);
    offset += plen;

    // filesz 4 bytes and filename 4 bytes
    copy(buf+fszst, buf+filest, obuf+offset); 
    offset += filest - fszst;

    for (int fd: name_fds) { // can be concurrent in future;
        int optval = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int));
         
        // write header
        write(fd, obuf, offset);

        // file content
        off_t stp = 0;
        sendfile(fd, file, &stp, tsz); 

        optval = 0;
        setsockopt(fd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int));
    }

    free(obuf);
    buginfo("End processing file.\n");
    return 0;
}

