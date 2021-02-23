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
        print_err("<main:%s:%d> ####### SIGINT #######\n", __func__, __LINE__);
    }
    else if (sig == SIGSEGV)
    {
        print_err("<main:%s:%d> ####### SIGSEGV #######\n", __func__, __LINE__);
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
    if (chdir(conf->rootDir.str()))
    {
        cerr << "!!! Error chdir(" << conf->rootDir.str()  << "): " << strerror(errno) << "\n";
        cin.get();
        exit(1);
    }
//----------------------------------------------------------------------
    if ((conf->NumChld < 1) || (conf->NumChld > 6))
    {
        print_err("<%s:%d> Error NumChld = %d; [1 < NumChld <= 6]\n", __func__, __LINE__, conf->NumChld);
        exit(1);
    }
    
    cout << " [" << get_time().str() << "] - server \"" << conf->ServerSoftware.str() << "\" run\n"
         << "   pid = " << pid
         << "\n   ip = " << conf->host.str()
         << "\n   Port = " << conf->servPort.str()
         << "\n   tcp_cork = " << conf->tcp_cork
         << "\n\n   MaxRequestsPerThr = " << conf->MaxRequestsPerThr
         << "\n   NumChld = " << conf->NumChld
         << "\n   MaxThreads = " << conf->MaxThreads
         << "\n   MimThreads = " << conf->MinThreads
         << "\n   MaxChldsCgi = " << conf->MaxChldsCgi
         << "\n\n   ListenBacklog = " << conf->ListenBacklog
         
         << "\n\n   WrBufSize = " << conf->WR_BUFSIZE
         << "\n   MaxRequests = " << conf->MAX_REQUESTS
         
         << "\n\n   SendFile = " << conf->SEND_FILE
         
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
    
    Connect::serverSocket = sockServer;
    
    pid_t pid_child;
    int numChld = 0;
    while (numChld < conf->NumChld)
    {
        pid_child = create_child(numChld);
        if (pid_child < 0)
        {
            print_err("<%s:%d> Error create_child() %d \n", __func__, __LINE__, numChld);
            exit(3);
        }

        ++numChld;
    }

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

    close(sockServer);
    
    while (wait(NULL) != -1)
    {
        print_err("<> wait()\n");
        continue;
    }
        
    free_fcgi_list();

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
				perror("setgid");
				cout << "[" << __func__ << "] Error setgid(" << conf->server_gid << "): " << strerror(errno) << "\n";
				cin.get();
				exit(1);
			}
			
			if (setuid(conf->server_gid) == -1) // server_uid
			{
				perror("setuid");
				cout << "[" << __func__ << "] Error setuid(" << conf->server_uid << "): " << strerror(errno) << "\n";
				cin.get();
				exit(1);
			}
		}
		
        manager(sockServer, num_chld);

        print_err("<%s:%d> *********** Exit child %d ***********\n", __func__, __LINE__, num_chld);
        close_logs();
        
        exit(0);
    }
    else if (pid < 0)
    {
        print_err("<> Error fork(): %s\n", strerror(errno));
    }
    
    return pid;
}
