#ifndef SERVER_H_
#define SERVER_H_
#define _FILE_OFFSET_BITS 64

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <climits>
#include <iomanip>

#include <mutex>
#include <thread>
#include <condition_variable>

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>
#include <sys/resource.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>

#include "String.h"

#define     MAX_PATH          2048
#define     MAX_NAME           256
#define     LEN_BUF_REQUEST   8192
#define     MAX_HEADERS         25

typedef struct fcgi_list_addr {
    String scrpt_name;
    String addr;
    struct fcgi_list_addr *next;
} fcgi_list_addr;

enum {
    RS101 = 101,
    RS200 = 200,RS204 = 204,RS206 = 206,
    RS301 = 301, RS302,
    RS400 = 400,RS401,RS402,RS403,RS404,RS405,RS406,RS407,
    RS408,RS411 = 411,RS413 = 413,RS414,RS415,RS416,RS417,RS418,
    RS500 = 500,RS501,RS502,RS503,RS504,RS505
};

enum {
    M_GET = 1, M_HEAD, M_POST, M_OPTIONS, M_PUT,
    M_PATCH, M_DELETE, M_TRACE, M_CONNECT   
};

enum { HTTP09 = 1, HTTP10, HTTP11, HTTP2 };

enum { cgi_ex = 1, php_cgi, php_fpm, fast_cgi };

enum { EXIT_THR = 1 };

const int NO_PRINT_LOG = -1000;

void print_err(const char *format, ...);
//----------------------------------------------------------------------
struct Config
{
    String host = "0.0.0.0";
    String ServerSoftware = "x";
    String servPort = "20000";
    char tcp_cork = 'n';
    char TcpNoDelay = 'y';
    int NumChld = 1;
    int MaxThreads = 15;
    int MinThreads = 6;
    int MaxRequestsPerThr = 50;
    int MaxChldsCgi = 5;
    int ListenBacklog = 128;
    int WR_BUFSIZE = 16284;
    int TIMEOUT_POLL = 10;
    char SEND_FILE = 'n';
    int MAX_REQUESTS = 256;
    char KeepAlive = 'y';
    int TimeoutKeepAlive = 5;
    int TimeOut = 30;
    int TimeoutCGI = 5;

    String rootDir = "";
    String cgiDir = "";
    String logDir = "";

    long int ClientMaxBodySize = 1000000;

    String UsePHP = "n";
    String PathPHP = "";

    fcgi_list_addr *fcgi_list = NULL;

    char index_html = 'n';
    char index_php = 'n';
    char index_pl = 'n';
    char index_fcgi = 'n';

    char ShowMediaFiles = 'n';

    uid_t server_uid;
    gid_t server_gid;
    
    String user = "";
    String group = "";
};
//----------------------------------------------------------------------
extern const Config* const conf;
//======================================================================
struct hdr {
    char *ptr;
    int len;
};

class Connect
{
public:
    Connect *prev;
    Connect *next;
    static int serverSocket;
    
    unsigned int numConn, numReq;
    int       numChld, clientSocket;
    int       err;
    time_t    sock_timer;
    int       timeout;
    int       event;
    
    char      remoteAddr[NI_MAXHOST];
    char      remotePort[NI_MAXSERV];
    
    char      bufReq[LEN_BUF_REQUEST];
    
    int       i_bufReq;
    char      *p_newline;
    char      *tail;
    int       lenTail;
    int       i_arrHdrs;
    hdr       arrHdrs[MAX_HEADERS + 1];
    
    char      decodeUri[LEN_BUF_REQUEST];
    unsigned int lenDecodeUri;

    char      *uri;
    unsigned int uriLen;
    //------------------------------------------------------------------
    char      *sReqParam;
    char      *sRange;
    
    int       reqMethod;
    int       httpProt;
    int       connKeepAlive;
    
    struct  {
        int  iConnection;
        int  iHost;
        int  iUserAgent;
        int  iReferer;
        int  iUpgrade;
        int  iReqContentType;
        int  iReqContentLength;
        int  iAcceptEncoding;
        int  iRange;
        int  iIfRange;
        
        long long reqContentLength;
        
        int       countReqHeaders;
        const char      *Name[MAX_HEADERS + 1];
        const char      *Value[MAX_HEADERS + 1];
    } req_hdrs;
    //--------------------------------------
    struct {
        int       respStatus;
        String    sLogTime;
        long long respContentLength;
        const char *respContentType;
        long long fileSize;
        int       countRespHeaders;
        
        int       scriptType;
        const char *scriptName;
        
        int       numPart;
        int       fd;
        off_t offset;
        long long send_bytes;
    } resp;
    
    void init();
    int hd_read();
    int empty_line();
};
//----------------------------------------------------------------------
class RequestManager
{
private:
    Connect *list_start;
    Connect *list_end;

    std::mutex mtx_thr;
    
    std::condition_variable cond_list;
    std::condition_variable cond_new_thr, cond_exit_thr;
    
    int num_wait_thr, size_list;
    int count_thr, stop_manager;
    
    int numChld;
    unsigned long all_thr;

    RequestManager() {}
public:
    RequestManager(const RequestManager&) = delete;
    RequestManager(int);
    ~RequestManager();
    //-------------------------------
    int get_num_chld(void);
    int get_num_thr(void);
    int get_all_thr(void);
    int start_thr(void);
    void wait_exit_thr(int n);
    friend void push_resp_list(Connect *req, RequestManager *);
    Connect *pop_resp_list();
    int end_thr(int);
    int wait_create_thr(int*);
    void close_manager();
    
    void print_intr();
};
//----------------------------------------------------------------------
extern char **environ;
//----------------------------------------------------------------------
void response1(RequestManager *ReqMan);
int response2(Connect *req);
int options(Connect *req);
int index_dir(Connect *req, String& path);
int cgi(Connect *req);
int fcgi(Connect *req);
int create_fcgi_socket(const char *host);
void free_fcgi_list();
//----------------------------------------------------------------------
int encode(const char *s_in, char *s_out, int len_out);
int decode(const char *s_in, int len_in, char *s_out, int len);
//----------------------------------------------------------------------
int read_timeout(int fd, char *buf, int len, int timeout);

int write_to_client(Connect *req, const char *buf, int len, int timeout);
int write_to_script(int fd, const char *buf, int len, int timeout);

int client_to_script(Connect *req, int fd_out, long long *cont_len);
void client_to_cosmos(Connect *req, long long *size);

long cgi_to_cosmos(int fd_in, int timeout);
long fcgi_to_cosmos(int fd_in, unsigned int size, int timeout);

int script_to_file(int fd_in, int fd_out, int cont_len, int timeout);

int fcgi_read_stderr(int fd, int cont_len, int timeout);

int send_largefile(Connect *req, char *buf, int size, off_t offset, long long *cont_len);
//----------------------------------------------------------------------
void get_time_run(int a, int b, struct timeval *time1, struct timeval *time2);
void send_message(Connect *req, const char *msg, String *);
int send_response_headers(Connect *req, String *hdrs);
//----------------------------------------------------------------------
String get_time();
void get_time(String& s);
const char *strstr_case(const char * s1, const char *s2);
int strlcmp_case(const char *s1, const char *s2, int len);

int get_int_method(char *s);
const char *get_str_method(int i);

int get_int_http_prot(char *s);
const char *get_str_http_prot(int i);

int clean_path(char *path);
const char *content_type(const char *s);

const char *base_name(const char *path);
int parse_startline_request(Connect *req, char *s, int len);
int parse_headers(Connect *req, char *s, int len);
const char *str_err(int i);
//----------------------------------------------------------------------
void create_logfiles(const String &, const String &);
void close_logs(void);
void print_err(Connect *req, const char *format, ...);
void print_log(Connect *req);
//----------------------------------------------------------------------
int timedwait_close_cgi(int numChld, int MaxChldsCgi);
void cgi_dec();
//----------------------------------------------------------------------
void end_response(Connect *req);

//----------------------------------------------------------------------
void event_handler(RequestManager *ReqMan);
void push_pollin_list(Connect *req);
void push_pollout_list(Connect *req);
void close_event_handler();

#endif
