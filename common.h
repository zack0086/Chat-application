#include <string>
using namespace std;

#define MAX_EVENTS 100
#define MAX_THREAD_NUMS 10
#define maxnode 1000
#define sigma_size 26
#define def_port 8080
#define max_file_sz 1000000
#define buf_size 1024

void buginfo(const char* f, ...);
char* str2raw(string& s);
int setnonblocking(int fd);
int set_uint32(char* s, int k);
int get_uint32(char* s);
int ksrecieve(int fd, int sock, int tsz);
