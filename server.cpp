#include "main.h"

using namespace std;

int sockServer;

int Connect::serverSocket;
int create_server_socket(const Config *c);
void read_conf_file(const char *path_conf);
//======================================================================
static void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        print_err("<main> ####### SIGINT #######\n");
    }
    else if (sig == SIGSEGV)
    {
        print_err("<main> ####### SIGSEGV #######\n");
        exit(1);
    }
    else
    {
        fprintf(stderr, "<%s:%d> ? sig=%d\n", __func__, __LINE__, sig);
    }
}
//======================================================================
pid_t create_child(int num_chld);
//======================================================================
int main(int argc, char *argv[])
{
    if (argc == 1)
        read_conf_file("./server.conf");
    else
        read_conf_file(argv[1]);
    
    signal(SIGPIPE, SIG_IGN);

    pid_t pid = getpid();
    //------------------------------------------------------------------
    sockServer = create_server_socket(conf);
    if (sockServer == -1)
    {
        fprintf(stderr, "<%s:%d> Error: create_server_socket()=%d\n", __func__, __LINE__, sockServer);
        close_logs();
        exit(1);
    }
    //------------------------------------------------------------------
    cout << " [" << get_time().c_str() << "] - server \"" << conf->ServerSoftware.c_str() << "\" run\n"
         << "\n   ip = " << conf->host.c_str()
         << "\n   Port = " << conf->servPort.c_str()
         << "\n   ListenBacklog = " << conf->ListenBacklog
         << "\n   tcp_cork = " << conf->tcp_cork
         << "\n   TcpNoDelay = " << conf->TcpNoDelay
         << "\n   SndBufSize = " << conf->SNDBUF_SIZE
         << "\n   SendFile = " << conf->SEND_FILE
         << "\n   MaxRequests = " << conf->MAX_REQUESTS
         << "\n   MaxEventSock = " << conf->MAX_EVENT_SOCK
         
         << "\n\n   NumProc = " << conf->NumProc
         << "\n   MaxThreads = " << conf->MaxThreads
         << "\n   MimThreads = " << conf->MinThreads
         << "\n   MaxRequestsPerThr = " << conf->MaxRequestsPerThr
         << "\n   MaxCgiProc = " << conf->MaxCgiProc
         
         << "\n\n   KeepAlive " << conf->KeepAlive
         << "\n   TimeoutPoll = " << conf->TIMEOUT_POLL
         << "\n   TimeoutKeepAlive = " << conf->TimeoutKeepAlive
         << "\n   TimeOut = " << conf->TimeOut
         << "\n   TimeoutCGI = " << conf->TimeoutCGI
         << "\n   MaxRanges = " << conf->MaxRanges
         << "\n\n   UsePHP: " << conf->UsePHP.c_str()
         << "\n   PathPHP: " << conf->PathPHP.c_str()
         << "\n   root_dir = " << conf->rootDir.c_str()
         << "\n   cgi_dir = " << conf->cgiDir.c_str()
         << "\n   log_dir = " << conf->logDir.c_str()
         << "\n   ShowMediaFiles = " << conf->ShowMediaFiles
         << "\n   ClientMaxBodySize = " << conf->ClientMaxBodySize
         << "\n   index_html = " << conf->index_html
         << "\n   index_php = " << conf->index_php
         << "\n   index_pl = " << conf->index_pl
         << "\n   index_fcgi = " << conf->index_fcgi
         << "\n\n";

    cerr << "   uid=" << getuid() << "; gid=" << getgid() << "\n\n";
    cout << "   uid=" << getuid() << "; gid=" << getgid() << "\n\n";
    cout << "   pid main proc: "  << pid <<  "\n";
    //------------------------------------------------------------------
    for ( ; environ[0]; )
    {
        char *p, buf[512];
        if ((p = (char*)memccpy(buf, environ[0], '=', strlen(environ[0]))))
        {
            *(p - 1) = 0;
            unsetenv(buf);
        }
    }
    
    Connect::serverSocket = sockServer;
    
    pid_t pid_child;
    int numProc = 0;
    while (numProc < conf->NumProc)
    {
        pid_child = create_child(numProc);
        if (pid_child < 0)
        {
            print_err("<%s:%d> Error create_child() %d \n", __func__, __LINE__, numProc);
            exit(3);
        }

        ++numProc;
    }

    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        print_err("<%s:%d> Error signal(SIGINT): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (signal(SIGSEGV, signal_handler) == SIG_ERR)
    {
        print_err("<%s:%d> Error signal(SIGSEGV): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    close(sockServer);

    while ((pid = wait(NULL)) != -1)
    {
        print_err("<> wait() pid: %d\n", pid);
    }

    print_err("<%s:%d> Exit server\n", __func__, __LINE__);
    close_logs();
    return 0;
}
//======================================================================
void manager(int sock, int num);
//======================================================================
pid_t create_child(int num_chld)
{
    pid_t pid;

    errno = 0;
    pid = fork();

    if (pid == 0)
    {
        uid_t uid = getuid();
        if (uid == 0)
        {
            if (setgid(conf->server_gid) == -1)
            {
                fprintf(stderr, "<%s:%d> Error setgid(%u): %s\n", __func__, __LINE__, conf->server_gid, strerror(errno));
                exit(1);
            }
            
            if (setuid(conf->server_gid) == -1)
            {
                fprintf(stderr, "<%s:%d> Error setuid(%u): %s\n", __func__, __LINE__, conf->server_gid, strerror(errno));
                exit(1);
            }
        }
        
        manager(sockServer, num_chld);
        close_logs();
        exit(0);
    }
    else if (pid < 0)
    {
        print_err("<> Error fork(): %s\n", strerror(errno));
    }
    
    return pid;
}
