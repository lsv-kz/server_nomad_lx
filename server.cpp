#include "main.h"

using namespace std;

int sockServer;

int Connect::serverSocket;
int create_server_socket(const Config *c);
void read_conf_file(const char *path_conf);
int *nConn;
//======================================================================
static void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        print_err("<main> ####### SIGINT #######\n");
        for (int n = 0; n < conf->NumChld; n++)
        {
            print_err("<main> ####### SIGINT ####### nConn[%d] = %d\n", n, nConn[n]);
        }
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
std::mutex mtx_nConn;
std::condition_variable cond_nConn;

pid_t create_child(int num_chld, int *pfd, int *fifoFd);
//======================================================================
int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        read_conf_file(".");
    }
    else
    {
        read_conf_file(argv[1]);
    }
    
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        cerr << "   Error signal(SIGPIPE,)!\n";
        cin.get();
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();
//----------------------------------------------------------------------
    sockServer = create_server_socket(conf);
    if (sockServer == -1)
    {
        cerr << "<" << __LINE__ << ">   server: failed to bind\n";
        cin.get();
        exit(1);
    }
//----------------------------------------------------------------------
    if ((conf->NumChld < 1) || (conf->NumChld > 8))
    {
        print_err("<%s:%d> Error Number of Processes = %d; [1 < NumChld <= 8]\n", __func__, __LINE__, conf->NumChld);
        exit(1);
    }
    
    cout << " [" << get_time().str() << "] - server \"" << conf->ServerSoftware.str() << "\" run\n"
         << "   pid = " << pid
         << "\n   ip = " << conf->host.str()
         << "\n   Port = " << conf->servPort.str()
         << "\n   ListenBacklog = " << conf->ListenBacklog
         << "\n   tcp_cork = " << conf->tcp_cork
         << "\n   TcpNoDelay = " << conf->TcpNoDelay
         << "\n   WrBufSize = " << conf->WR_BUFSIZE
         << "\n   SendFile = " << conf->SEND_FILE
         << "\n   MaxRequests = " << conf->MAX_REQUESTS
         
         << "\n\n   NumChld = " << conf->NumChld
         << "\n   MaxThreads = " << conf->MaxThreads
         << "\n   MimThreads = " << conf->MinThreads
         << "\n   MaxRequestsPerThr = " << conf->MaxRequestsPerThr
         << "\n   MaxChldsCgi = " << conf->MaxChldsCgi
         
         << "\n\n   KeepAlive " << conf->KeepAlive
         << "\n   TimeoutKeepAlive = " << conf->TimeoutKeepAlive
         << "\n   TimeOut = " << conf->TimeOut
         << "\n   TimeoutCGI = " << conf->TimeoutCGI
         << "\n   TimeoutThreadCond = " << conf->TimeoutThreadCond
         << "\n\n   UsePHP: " << conf->UsePHP.str()
         << "\n   PathPHP: " << conf->PathPHP.str()
         << "\n   root_dir = " << conf->rootDir.str()
         << "\n   cgi_dir = " << conf->cgiDir.str()
         << "\n   log_dir = " << conf->logDir.str()
         << "\n   ShowMediaFiles = " << conf->ShowMediaFiles
         << "\n   ClientMaxBodySize = " << conf->ClientMaxBodySize
         << "\n\n";

    cerr << "  uid=" << getuid() << "; gid=" << getgid() << "\n\n";
    cout << "  uid=" << getuid() << "; gid=" << getgid() << "\n\n";
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

    if (conf->MinThreads > conf->MaxThreads)
    {
        cerr << "<" << __func__ << "():" << __LINE__ << "> Error: NumThreads > MaxThreads\n";
        cin.get();
        exit(1);
    }
    //------------------------------------------------------------------
    Connect::serverSocket = sockServer;
    
    int Start[2];
    if (pipe(Start) < 0)
    {
        printf("<%s():%d> Error pipe(): %s\n", __FUNCTION__, __LINE__, strerror(errno));
        exit(1);
    }
    //------------------------------------------------------------------
    int fifoFD[8];
    
    pid_t pid_child;
    int numChld = 0;
    
    while (numChld < conf->NumChld)
    {
        char s[16];
        snprintf(s, sizeof(s), "./fifo_%d", numChld);
        
        if (mkfifo(s, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
        {
            printf("<%s:%d> Error mkfifo: %s\n", __func__, __LINE__, strerror(errno));
            exit(1);
        }
        
        pid_child = create_child(numChld, Start, fifoFD);
        if (pid_child < 0)
        {
            print_err("<%s:%d> Error create_child() %d \n", __func__, __LINE__, numChld);
            exit(3);
        }
        
        if ((fifoFD[numChld] = open(s, O_WRONLY)) == -1)
        {
            printf("<%s:%d> Error open: %s\n", __func__, __LINE__, strerror(errno));
            exit(1);
        }
remove(s);
        ++numChld;
    }
    
    close(Start[1]);

    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        cerr << "   Error signal(SIGINT)\n";
        print_err("<%s:%d> Error signal(SIGINT): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (signal(SIGSEGV, signal_handler) == SIG_ERR)
    {
        cerr << "   Error signal(SIGSEGV)!\n";
        print_err("<%s:%d> Error signal(SIGSEGV): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    nConn = new(nothrow) int [conf->NumChld];
    if (!nConn)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    memset(nConn, 0, conf->NumChld * sizeof(int));
    
    struct pollfd fdrd[2];
    
    fdrd[0].fd = Start[0];
    fdrd[0].events = POLLIN;
    
    fdrd[1].fd = sockServer;
    fdrd[1].events = POLLIN;
    
    int numFD = 2, waitRD = 0;
    int close_server = 0;
    while (!close_server)
    {
        int ret = poll(fdrd, numFD, -1);
        if (ret == -1)
        {
            print_err("<%s:%d> Error poll()=-1: %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            break;
        }

        if ((fdrd[1].revents & (POLLERR | POLLHUP | POLLNVAL)) && (!waitRD))
        {
            print_err("<%s:%d> Error fdrd[1].revents=0x%x\n", __func__, __LINE__, fdrd[1].revents);
            break;
        }

        if ((fdrd[1].revents == POLLIN) && (!waitRD))
        {
            char ch = conf->NumChld;

            for (int i = 0; i < conf->NumChld; ++i)
            {
                if (nConn[i] < (conf->MAX_REQUESTS))
                {
                    ch = i;
                    break;
                }
            }

            if (ch < conf->NumChld)
            {
                ret = write(fifoFD[(int)ch], &ch, 1);
                if (ret < 0)
                {
                    print_err("<%s:%d> Error write()=-1: %s\n", __func__, __LINE__, strerror(errno));
                    close_server = 1;
                    break;
                }
                waitRD = 1;
            }
            numFD = 1;
        }

        if (fdrd[0].revents == POLLIN)
        {
            char ch[32];
            ret = read(Start[0], ch, 32);
            if (ret < 0)
            {
                print_err("<%s:%d> Error read()=-1: %s\n", __func__, __LINE__, strerror(errno));
                close_server = 1;
                break;
            }

            for (int i = 0; i < ret; ++i)
            {
                if (ch[i] & 0x80)
                {
                    waitRD = 0;
                    ch[i] = ch[i] & 0x7f;
                    (*(nConn + ch[i]))++;
                    numFD = 3;
                }
                else
                {
                    (*(nConn + ch[i]))--;
                    if (waitRD == 0)
                        numFD = 2;
                }
            }
        }
    }

    for (numChld = 0; numChld < conf->NumChld; ++numChld)
    {
        close(fifoFD[numChld]);
    }

    close(sockServer);
    
    delete [] nConn;

    while ((pid = wait(NULL)) != -1)
    {
        print_err("<> wait() pid: %d\n", pid);
        continue;
    }
        
    free_fcgi_list();

    print_err("<%s:%d> Exit server\n", __func__, __LINE__);
    close_logs();
    return 0;
}
//======================================================================
void manager(int sock, int num, int);
//======================================================================
pid_t create_child(int num_chld, int *pfd1, int *fifoFd)
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
                perror("setgid");
                cout << "[" << __func__ << "] Error setgid(" << conf->server_gid << "): " << strerror(errno) << "\n";
                cin.get();
                exit(1);
            }
            
            if (setuid(conf->server_gid) == -1)
            {
                perror("setuid");
                cout << "[" << __func__ << "] Error setuid(" << conf->server_uid << "): " << strerror(errno) << "\n";
                cin.get();
                exit(1);
            }
        }
        
        for (int i = 0; i < num_chld; ++i)
        {
            close(fifoFd[i]);
        }
        
        close(pfd1[0]);
        manager(sockServer, num_chld, pfd1[1]);
        close(pfd1[1]);
        close_logs();
        
        exit(0);
    }
    else if (pid < 0)
    {
        print_err("<> Error fork(): %s\n", strerror(errno));
    }
    
    return pid;
}
