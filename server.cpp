#include "main.h"

using namespace std;

static int sockServer;
int Connect::serverSocket;

int create_server_socket(const Config *c);
int read_conf_file(const char *path_conf);
void manager(int, int);
int set_uid();

static int main_proc();
static pid_t create_child(int num_chld);

static String pidFile;
static String conf_path;
static pid_t pidArr[8];

static int start = 0, run = 1;
//======================================================================
void send_sig(const int sig)
{
    int i = 0;
    while (i < conf->NumProc)
    {
        kill(pidArr[i], sig);
        ++i;
    }
}
//======================================================================
static void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        print_err("<main> ####### SIGINT #######\n");
        run = 0;
    }
    else if (sig == SIGSEGV)
    {
        print_err("<main> ####### SIGSEGV #######\n");
        exit(1);
    }
    else if (sig == SIGUSR1)
    {
        fprintf(stderr, "<%s> ####### SIGUSR1 #######\n", __func__);
        send_sig(SIGUSR1);
        run = 1;
    }
    else if (sig == SIGUSR2)
    {
        fprintf(stderr, "<%s> ####### SIGUSR2 #######\n", __func__);
        send_sig(SIGUSR1);
        run = 0;
    }
    else
    {
        fprintf(stderr, "<%s:%d> ? sig=%d\n", __func__, __LINE__, sig);
    }
}
//======================================================================
void print_help(const char *name)
{
    fprintf(stderr, "Usage: %s [-l] [-c configfile] [-s signal]\n"
                    "Options:\n"
                    "   -h              : help\n"
                    "   -l              : print system limits\n"
                    "   -c configfile   : default: \"./server.conf\"\n"
                    "   -s signal       : restart, close\n", name);
}
//======================================================================
void print_limits()
{
    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) == -1)
        fprintf(stdout, "<%s:%d> Error getrlimit(RLIMIT_NOFILE): %s\n", __func__, __LINE__, strerror(errno));
    else
        printf(" RLIMIT_NOFILE: cur=%ld, max=%ld\n", (long)lim.rlim_cur, (long)lim.rlim_max);
}
//======================================================================
void print_config()
{
    print_limits();
    
    cout << "   ServerSoftware       : " << conf->ServerSoftware.c_str()
         << "\n\n   ServerAddr           : " << conf->ServerAddr.c_str()
         << "\n   ServerPort           : " << conf->ServerPort.c_str()
         << "\n\n   ListenBacklog        : " << conf->ListenBacklog
         << "\n   tcp_cork             : " << conf->tcp_cork
         << "\n   tcp_nodelay          : " << conf->tcp_nodelay

         << "\n\n   SndBufSize           : " << conf->SndBufSize
         << "\n   SendFile             : " << conf->SendFile

         << "\n\n   OverMaxConnections   : " << conf->OverMaxConnections
         << "\n   MaxWorkConnections   : " << conf->MaxWorkConnections
         << "\n   MaxConnections       : " << conf->MaxConnections

         << "\n\n   MaxEventConnections  : " << conf->MaxEventConnections

         << "\n\n   NumProc              : " << conf->NumProc
         << "\n   MaxThreads           : " << conf->MaxThreads
         << "\n   MimThreads           : " << conf->MinThreads
         << "\n   MaxCgiProc           : " << conf->MaxCgiProc

         << "\n\n   MaxRequestsPerClient : " << conf->MaxRequestsPerClient
         << "\n   TimeoutKeepAlive     : " << conf->TimeoutKeepAlive
         << "\n   Timeout              : " << conf->Timeout
         << "\n   TimeoutCGI           : " << conf->TimeoutCGI
         << "\n   TimeoutPoll          : " << conf->TimeoutPoll

         << "\n\n   MaxRanges            : " << conf->MaxRanges

         << "\n\n   ClientMaxBodySize    : " << conf->ClientMaxBodySize

         << "\n\n   ShowMediaFiles       : " << conf->ShowMediaFiles

         << "\n\n   index_html           : " << conf->index_html
         << "\n   index_php            : " << conf->index_php
         << "\n   index_pl             : " << conf->index_pl
         << "\n   index_fcgi           : " << conf->index_fcgi
         << "\n\n   DocumentRoot         : " << conf->DocumentRoot.c_str()
         << "\n   ScriptPath           : " << conf->ScriptPath.c_str()
         << "\n   LogPath              : " << conf->LogPath.c_str()
         << "\n\n   UsePHP               : " << conf->UsePHP.c_str()
         << "\n   PathPHP              : " << conf->PathPHP.c_str()
         << "\n\n   User                 : " << conf->user.c_str()
         << "\n   Group                : " << conf->group.c_str()
         << "\n";
         
    cout << "   ------------- FastCGI -------------\n";
    fcgi_list_addr *i = conf->fcgi_list;
    for (; i; i = i->next)
    {
        cout << "   [" << i->scrpt_name.c_str() << " : " << i->addr.c_str() << "]\n";
    }
    
}
//======================================================================
int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    if (argc == 1)
        conf_path = "server.conf";
    else
    {
        int c, arg_print = 0;
        pid_t pid_ = 0;
        char *sig = NULL, *conf_dir_ = NULL;
        while ((c = getopt(argc, argv, "c:s:h:p")) != -1)
        {
            switch (c)
            {
                case 'c':
                    conf_dir_ = optarg;
                    break;
                case 's':
                    sig = optarg;
                    break;
                case 'h':
                    print_help(argv[0]);
                    return 0;
                case 'p':
                    arg_print = 1;
                    break;
                default:
                    print_help(argv[0]);
                    return 0;
            }
        }

        if (conf_dir_)
            conf_path = conf_dir_;
        else
            conf_path = "server.conf";

        if (arg_print)
        {
            if (read_conf_file(conf_path.c_str()))
                return 1;
            print_config();
            return 0;
        }

        if (sig)
        {
            if (read_conf_file(conf_path.c_str()))
                return 1;
            pidFile = conf->PidFilePath;
            pidFile << "/pid.txt";
            FILE *fpid = fopen(pidFile.c_str(), "r");
            if (!fpid)
            {
                fprintf(stderr, "<%s:%d> Error open PidFile(%s): %s\n", __func__, __LINE__, pidFile.c_str(), strerror(errno));
                return 1;
            }

            fscanf(fpid, "%u", &pid_);
            fclose(fpid);

            if (!strcmp(sig, "restart"))
                kill(pid_, SIGUSR1);
            else if (!strcmp(sig, "close"))
                kill(pid_, SIGUSR2);
            else
            {
                fprintf(stderr, "<%d> ? option -s: %s\n", __LINE__, sig);
                print_help(argv[0]);
                return 1;
            }

            return 01;
        }
    }

    while (run)
    {
        run = 0;

        if (read_conf_file(conf_path.c_str()))
            return 1;

        pidFile = conf->PidFilePath;
        pidFile << "/pid.txt";
        FILE *fpid = fopen(pidFile.c_str(), "w");
        if (!fpid)
        {
            fprintf(stderr, "<%s:%d> Error open PidFile(%s): %s\n", __func__, __LINE__, pidFile.c_str(), strerror(errno));
            return 1;
        }

        fprintf(fpid, "%u\n", getpid());
        fclose(fpid);

        sockServer = create_server_socket(conf);
        if (sockServer == -1)
        {
            fprintf(stderr, "<%s:%d> Error: create_server_socket(%s:%s)\n", __func__, __LINE__, 
                        conf->ServerAddr.c_str(), conf->ServerPort.c_str());
            return 1;
        }

        Connect::serverSocket = sockServer;
        
        if (start == 0)
        {
            start = 1;
            set_uid();

            if (signal(SIGINT, signal_handler) == SIG_ERR)
            {
                fprintf(stderr, "<%s:%d> Error signal(SIGINT): %s\n", __func__, __LINE__, strerror(errno));
                return 1;
            }

            if (signal(SIGSEGV, signal_handler) == SIG_ERR)
            {
                fprintf(stderr, "<%s:%d> Error signal(SIGSEGV): %s\n", __func__, __LINE__, strerror(errno));
                return 1;
            }

            if (signal(SIGUSR1, signal_handler) == SIG_ERR)
            {
                fprintf(stderr, "<%s:%d> Error signal(SIGUSR1): %s\n", __func__, __LINE__, strerror(errno));
                return 1;
            }

            if (signal(SIGUSR2, signal_handler) == SIG_ERR)
            {
                fprintf(stderr, "<%s:%d> Error signal(SIGUSR2): %s\n", __func__, __LINE__, strerror(errno));
                return 1;
            }
        }

        if (main_proc())
            break;
    }

    return 0;
}
//======================================================================
int main_proc()
{
    create_logfiles(conf->LogPath);
    pid_t pid = getpid();
    //------------------------------------------------------------------
    cout << "\n[" << get_time().c_str() << "] - server \"" << conf->ServerSoftware.c_str() << "\" run port: " << conf->ServerPort.c_str() << "\n";
    cerr << "   pid="  << pid << "; uid=" << getuid() << "; gid=" << getgid() << "\n";
    cout << "   pid="  << pid << "; uid=" << getuid() << "; gid=" << getgid() << "\n";
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
    //------------------------------------------------------------------
    int numProc = 0;
    while (numProc < conf->NumProc)
    {
        pid = create_child(numProc);
        if (pid < 0)
        {
            print_err("<%s:%d> Error create_child() %d \n", __func__, __LINE__, numProc);
            exit(1);
        }

        pidArr[numProc] = pid;
        ++numProc;
    }
    //------------------------------------------------------------------
    close(sockServer);

    while ((pid = wait(NULL)) != -1)
    {
        print_err("<> wait() pid: %d\n", pid);
    }

    shutdown(sockServer, SHUT_RDWR);

    if (run == 0)
    {
        remove(pidFile.c_str());
        fprintf(stderr, "<%s> ***** Close *****\n", __func__);
    }
    else
        fprintf(stderr, "<%s> ***** Reload *****\n", __func__);

    close_logs();
    return 0;
}
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
