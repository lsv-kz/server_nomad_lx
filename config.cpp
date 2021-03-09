#include "main.h"
#include <sstream>

using namespace std;

static Config c;
const Config* const conf = &c;
//======================================================================
int check_path(String & path)
{
    struct stat st;
    
    int ret = stat(path.str(), &st);
    if (ret == -1)
    {
        fprintf(stderr, "<%s:%d> Error stat(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "<%s:%d> [%s] is not directory\n", __func__, __LINE__, path.str());
        return -1;
    }
    
    char path_[PATH_MAX] = "";
    if (!realpath(path.str(), path_))
    {
        fprintf(stderr, "<%s:%d> Error realpath(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }
    
    path = path_;

    return 0;
}
//======================================================================
int create_conf_file(const char *path)
{
    ofstream fconf(path, ios::binary);
    if (!fconf.is_open())
    {
        cerr << __func__ << "(): Error create conf file (" << path << "): "
                 << strerror(errno) << "\n";
        cin.get();
        exit(1);
    }

    fconf << "ServerAddr   " << "0.0.0.0" << "\n";
    fconf << "Port         ?\n";
    fconf << "ServerSoftware   " << "x" << "\n";
    
    fconf << "tcp_cork   " << c.tcp_cork << "\n";
    
    fconf << "DocumentRoot " << c.rootDir.str() << "\n";
    fconf << "ScriptPath   " << c.cgiDir.str() << "\n";
    fconf << "LogPath      " << c.logDir.str() << "\n\n";

    fconf << "MaxRequestsPerThr " << c.MaxRequestsPerThr << "\n\n";
    
    fconf << "ListenBacklog " << c.ListenBacklog << "\n\n";
    
    fconf << "WrBufSize   " << c.WR_BUFSIZE << "\n";
    fconf << "MaxRequests " << c.MAX_REQUESTS << "\n\n";
    
    fconf << "SendFile  " << c.SEND_FILE << "\n\n";
    
    fconf << "NumChld " << c.NumChld << "\n";
    fconf << "MaxThreads " << c.MaxThreads << "\n";
    fconf << "MinThreads " << c.MinThreads << "\n\n";
    
    fconf << "MaxChldsCgi " << c.MaxChldsCgi << "\n\n";

    fconf << "KeepAlive  " << c.KeepAlive << " #   y/n" << "\n";
    fconf << "TimeoutKeepAlive " << c.TimeoutKeepAlive << "\n";
    fconf << "TimeOut    " << c.TimeOut << "\n";
    fconf << "TimeoutThreadCond   " << c.TimeoutThreadCond << "\n";
    fconf << "TimeoutCGI " << c.TimeoutCGI << "\n\n";

    fconf << "ClientMaxBodySize " << c.ClientMaxBodySize << "\n\n";
    
    fconf << " UsePHP     n  # php-fpm # php-cgi \n";
    fconf << "# PathPHP   /usr/bin/php-cgi\n";
    fconf << "# PathPHP  127.0.0.1:9000  #  /run/php/php7.0-fpm.sock \n\n";

    fconf << "index {\n"
                "\t#index.html\n"
                "}\n\n";
    
    fconf << "fastcgi {\n"
                "\t#/test  127.0.0.1:9009\n"
                "}\n\n";
    
    fconf << "ShowMediaFiles " << c.ShowMediaFiles << " #  y/n" << "\n\n";

    fconf << "User nobody     # www-data\n";
    fconf << "Group nogroup   # www-data\n\n";

    fconf.close();
    return 0;
}
//======================================================================
fcgi_list_addr *create_fcgi_list()
{
    fcgi_list_addr *tmp;
    tmp = new(nothrow) fcgi_list_addr;
    if (!tmp)
    {
        fprintf(stderr, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(errno);
    }
    
    tmp->next = NULL;
    
    return tmp;
}
//======================================================================
int getLine(ifstream & fi, String &s)
{
    int n = 0;
    char c;
    s.clear();
    while (fi.get(c))
    {
        if (c == '\n') break;
        s << c;
        ++n;
    }

    return n;
}
//======================================================================
void read_conf_file(const char *path_conf)
{
    String s, ss, nameFile;

    nameFile << path_conf;
    nameFile << "/server.conf";
    fcgi_list_addr *end = NULL;
    
    ifstream fconf(nameFile.str(), ios::binary);
    if (!fconf.is_open())
    {
        if (create_conf_file(nameFile.str()))
        {
            cerr << __func__ << "(): Error create conf file (" << nameFile.str() << "): "
                 << strerror(errno) << "\n";
            cin.get();
            exit(1);
        }
        
        cout << " Correct config file: " << nameFile.str() << "\n";
        cin.get();
        exit(1);
    }

    while (!fconf.eof())
    {
        getLine(fconf, ss);
        ss >> s;

        if (s[0] == '#')
            continue;

        if (s ==  "ServerAddr")
            ss >> c.host;
        else if (s == "Port")
            ss >> c.servPort;
        else if (s == "ServerSoftware")
            ss >> c.ServerSoftware;
        else if (s == "tcp_cork")
            ss >> c.tcp_cork;
        else if (s == "DocumentRoot")
            ss >> c.rootDir;
        else if (s == "ScriptPath")
            ss >> c.cgiDir;
        else if (s == "LogPath")
            ss >> c.logDir;
        else if (s == "MaxRequestsPerThr")
            ss >> c.MaxRequestsPerThr;
        else if (s == "ListenBacklog")
            ss >> c.ListenBacklog;
        else if (s == "SendFile")
            ss >> c.SEND_FILE;
        else if (s == "WrBufSize")
            ss >> c.WR_BUFSIZE;
        else if (s == "MaxRequests")
            ss >> c.MAX_REQUESTS;
        else if (s == "NumChld")
            ss >> c.NumChld;
        else if (s == "MaxThreads")
            ss >> c.MaxThreads;
        else if (s == "MinThreads")
            ss >> c.MinThreads;
        else if (s == "MaxChldsCgi")
            ss >> c.MaxChldsCgi;
        else if (s == "KeepAlive")
            ss >> c.KeepAlive;
        else if (s == "TimeoutKeepAlive")
            ss >> c.TimeoutKeepAlive;
        else if (s == "TimeOut")
            ss >> c.TimeOut;
        else if (s == "TimeoutCGI")
            ss >> c.TimeoutCGI;
        else if (s == "TimeoutThreadCond")
            ss >> c.TimeoutThreadCond;
        else if (s == "UsePHP")
            ss >> c.UsePHP;
        else if (s == "PathPHP")
            ss >> c.PathPHP;
        else if (s == "ShowMediaFiles")
            ss >> c.ShowMediaFiles;
        else if (s == "ClientMaxBodySize")
            ss >> c.ClientMaxBodySize;
        else if (s == "index")
        {
            ss >> s;
            if (s != "{")
            {
                getLine(fconf, ss);
                ss >> s;
                if (s != "{")
                    continue;
            }
            
            while (!fconf.eof())
            {
                getLine(fconf, ss);
                ss >> s;

                if ((s[0] == '#') || (s[0] == '{'))
                    continue;
                else if (s[0] == '}')
                    break;

                if (s == "index.html")
                    c.index_html = 'y';
                else if (s == "index.php")
                    c.index_php = 'y';
                else if (s == "index.pl")
                    c.index_pl = 'y';
                else if (s == "index.fcgi")
                    c.index_fcgi = 'y';
            }
            
            if (s[0] != '}')
            {
                cerr << "   Error read config file\n";
                cin.get();
                exit(1);
            }
        }
        else if (s == "User")
            ss >> c.user;
        else if (s == "Group")
            ss >> c.group;
        else if (s == "fastcgi")
        {
            ss >> s;
            if (s != "{")
            {
                getLine(fconf, ss);
                ss >> s;
                if (s != "{")
                    continue;
            }
            
            while (!fconf.eof())
            {
                getLine(fconf, ss);
                ss >> s;

                if ((s[0] == '#') || (s.len() == 0) || (s[0] == '{'))
                    continue;
                else if (s[0] == '}')
                    break;

                if (!end)
                {
                    end = c.fcgi_list = create_fcgi_list();
                    c.fcgi_list->scrpt_name = s;
                    ss >> c.fcgi_list->addr;
                }
                else
                {
                    end->next = create_fcgi_list();
                    end->next->scrpt_name = s;
                    ss >> end->next->addr;
                    end = end->next;
                }
            }
            if (s[0] != '}')
            {
                cerr << "   Error read config file\n";
                cin.get();
                exit(1);
            }
        }
    }
    
    fconf.close();
/*
    fcgi_list_addr *i = c.fcgi_list;
    for (; i; i = i->next)
    {
        cerr << "[" << i->scrpt_name.str() << "] = [" << i->addr.str() << "]\n";
    }*/
//-------------------------log_dir--------------------------------------
    if (check_path(c.logDir) == -1)
    {
        cerr << "!!! Error LogPath [" << c.logDir.str() << "]\n";
        cin.get();
        exit(1);
    }
    create_logfiles(conf->logDir, conf->ServerSoftware);
//-------------------------root_dir-------------------------------------
    if (check_path(c.rootDir) == -1)
    {
        cerr << "!!! Error DocumentRoot [" << c.rootDir.str()  << "]\n";
        cin.get();
        exit(1);
    }
//-------------------------cgi_dir--------------------------------------
    if (check_path(c.cgiDir) == -1)
    {
        c.cgiDir = "";
        cerr << "!!! Error ScriptPath [" << c.cgiDir.str() << "]\n";
    }
    
    if (c.MinThreads < 1)
        c.MinThreads = 1;
    
    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) == -1)
    {
        print_err("<%s:%d> Error getrlimit(RLIMIT_NOFILE): %s\n", __func__, __LINE__, strerror(errno));
    }
    else
    {
        printf("<%s:%d> lim.rlim_max=%lu, lim.rlim_cur=%lu\n", __func__, __LINE__, (unsigned long)lim.rlim_max, (unsigned long)lim.rlim_cur);
        long max_fd = (c.MAX_REQUESTS * 2) + 6;
        if (max_fd > (long)lim.rlim_cur)
        {
            if (max_fd > (long)lim.rlim_max)
                lim.rlim_cur = lim.rlim_max;
            else
                lim.rlim_cur = max_fd;
            
            if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
                print_err("<%s:%d> Error setrlimit(RLIMIT_NOFILE): %s\n", __func__, __LINE__, strerror(errno));
            max_fd = sysconf(_SC_OPEN_MAX);
            if (max_fd > 1)
            {
                print_err("<%s:%d> _SC_OPEN_MAX=%d\n", __func__, __LINE__, max_fd);
                c.MAX_REQUESTS = (max_fd - 6)/2;
                printf("<%s:%d> MaxRequests=%d, _SC_OPEN_MAX=%ld\n", __func__, __LINE__, c.MAX_REQUESTS, max_fd);
            }
            else
            {
                print_err("<%s:%d> Error sysconf(_SC_OPEN_MAX): %s\n", __func__, __LINE__, strerror(errno));
                close_logs();
                exit(1);
            }
        }
    }
    //----------------------------------------------------------------------
    uid_t uid = getuid();
    if (uid == 0)
    {
        char *p;
        c.server_uid = strtol(conf->user.str(), &p, 0);
        if (conf->user.len() && *p != '\0')
        {
            struct passwd *passwdbuf = getpwnam(conf->user.str());
            if (!passwdbuf)
            {
                cerr << "[" << __func__ << ":" << __LINE__ << "] Error getpwnam(): " << conf->user.str() << "\n";
                cout << "[" << __func__ << ":" << __LINE__ << "] Error getpwnam(): " << conf->user.str() << "\n";
                cin.get();
                exit(1);
            }
            c.server_uid = passwdbuf->pw_uid;
        }
        else
        {
            struct passwd *passwdbuf;
            passwdbuf = getpwuid(conf->server_uid);
            if (passwdbuf == NULL)
            {
                cerr << "[" << __func__ << ":" << __LINE__ << "] Error getpwuid(): " << conf->server_uid << "\n";
                cout << "[" << __func__ << ":" << __LINE__ << "] Error getpwuid(): " << conf->server_uid << "\n";
                cin.get();
                exit(1);
            }
        }
        
        c.server_gid = strtol(conf->group.str(), &p, 0);
        if (conf->group.len() && *p != '\0')
        {
            struct group *groupbuf = getgrnam(conf->group.str());
            if (!groupbuf)
            {
                cerr << "[" << __func__ << ":" << __LINE__ << "] Error getgrnam(): " << conf->group.str() << "\n";
                cout << "[" << __func__ << ":" << __LINE__ << "] Error getgrnam(): " << conf->group.str() << "\n";
                cin.get();
                exit(1);
            }
            c.server_gid = groupbuf->gr_gid;
        }
        else
        {
            struct group *groupbuf;
            groupbuf = getgrgid(conf->server_gid);
            if (groupbuf == NULL)
            {
                cerr << "[" << __func__ << ":" << __LINE__ << "] Error getgrgid(): " << conf->server_gid << "\n";
                cout << "[" << __func__ << ":" << __LINE__ << "] Error getgrgid(): " << conf->server_gid << "\n";
                cin.get();
                exit(1);
            }
        }
        //--------------------------------------------------------------
        if (conf->server_uid >= conf->server_gid)
        {
            if (setgid(conf->server_uid) == -1)
            {
                perror("setgid");
                cout << "[" << __func__ << ":" << __LINE__ << "] Error setgid(" << conf->server_gid << "): " << strerror(errno)
                    << "; uid=" << getgid() << "\n";
                cin.get();
                exit(1);
            }
        }
        else
        {
            if (setgid(conf->server_gid) == -1)
            {
                perror("setgid");
                cout << "[" << __func__ << ":" << __LINE__ << "] Error setgid(" << conf->server_gid << "): " << strerror(errno)
                    << "; uid=" << getuid() << "\n";
                cin.get();
                exit(1);
            }
        }
        
        if (conf->server_uid != uid)
        {
            if (setuid(conf->server_uid) == -1)
            {
                perror("setgid");
                cout << "[" << __func__ << ":" << __LINE__ << "] Error setuid(" << conf->server_uid << "): " << strerror(errno)
                    << "; uid=" << getuid() << "\n";
                cin.get();
                exit(1);
            }
        }
    }
    else
    {
        c.server_uid = getuid();
        c.server_gid = getgid();
    }
}
//======================================================================
void free_fcgi_list()
{
    fcgi_list_addr *prev;
    while (c.fcgi_list)
    {
        prev = c.fcgi_list;
        c.fcgi_list = c.fcgi_list->next;
        if (prev) delete prev;
    }
}
//======================================================================
void set_sndbuf(int n)
{
    if (c.WR_BUFSIZE > n)
        c.WR_BUFSIZE = n;
}
