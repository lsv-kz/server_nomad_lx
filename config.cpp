#include "main.h"

using namespace std;

static Config c;
const Config* const conf = &c;
const int minOpenFD = 6;
//======================================================================
int check_path(String & path)
{
    struct stat st;

    int ret = stat(path.c_str(), &st);
    if (ret == -1)
    {
        fprintf(stderr, "<%s:%d> Error stat(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "<%s:%d> [%s] is not directory\n", __func__, __LINE__, path.c_str());
        return -1;
    }

    char path_[PATH_MAX] = "";
    if (!realpath(path.c_str(), path_))
    {
        fprintf(stderr, "<%s:%d> Error realpath(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    path = path_;

    return 0;
}
//======================================================================
void create_conf_file(const char *path)
{
    ofstream fconf(path, ios::binary);
    if (!fconf.is_open())
    {
        fprintf(stderr, "<%s> Error create conf file (%s): %s\n", __func__, path, strerror(errno));
        exit(1);
    }

    fconf << "ServerSoftware   " << "x" << "\n";
    fconf << "ServerAddr   " << "0.0.0.0" << "\n";
    fconf << "Port         ?\n";
    fconf << "tcp_cork   " << c.tcp_cork << "\n";
    fconf << "TcpNoDelay   y\n\n";
    fconf << "DocumentRoot " << c.rootDir.c_str() << "\n";
    fconf << "ScriptPath   " << c.cgiDir.c_str() << "\n";
    fconf << "LogPath      " << c.logDir.c_str() << "\n\n";

    fconf << "MaxRequestsPerThr " << c.MaxRequestsPerThr << "\n\n";

    fconf << "ListenBacklog " << c.ListenBacklog << "\n\n";

    fconf << "SndBufSize   " << c.SNDBUF_SIZE << "\n";
    fconf << "MaxRequests " << c.MAX_REQUESTS << "\n\n";

    fconf << "SendFile  " << c.SEND_FILE << "\n";
    fconf << "TimeoutPoll  " << c.TIMEOUT_POLL << "\n\n";

    fconf << "NumProc " << c.NumProc << "\n";
    fconf << "MaxThreads " << c.MaxThreads << "\n";
    fconf << "MinThreads " << c.MinThreads << "\n\n";

    fconf << "MaxCgiProc " << c.MaxCgiProc << "\n\n";

    fconf << "KeepAlive  " << c.KeepAlive << " #   y/n" << "\n";
    fconf << "TimeoutKeepAlive " << c.TimeoutKeepAlive << "\n";
    fconf << "TimeOut    " << c.TimeOut << "\n";
    fconf << "TimeoutCGI " << c.TimeoutCGI << "\n\n";

    fconf << "MaxRanges  %d\n\n" << c.MaxRanges << "\n\n";

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
}
//======================================================================
int getLine(ifstream &fi, String &ss)
{
    ss = "";
    char ch;
    int n = 0, wr = 1;

    while (fi.get(ch))
    {
        if ((char)ch == '\n')
        {
            if (n)
                return n;
            else
            {
                wr = 1;
                ss = "";
                continue;
            }
        }

        if ((wr == 0) || (ch == '\r'))
            continue;

        switch (ch)
        {
            case ' ':
            case '\t':
                if (n)
                {
                    ss << ' ';
                    ++n;
                }
                break;
            case '#':
                wr = 0;
                break;
            case '{':
            case '}':
                if (n)
                    fi.seekg(-1, ios::cur);
                else
                {
                    ss << ch;
                    ++n;
                }
                return n;
            default:
                ss << ch;
                ++n;
        }
    }

    if (n)
        return n;
    return -1;
}
//======================================================================
int find_bracket(ifstream &fi)
{
    char ch;
    int grid = 0;

    while (fi.get(ch))
    {
        if (ch == '#')
            grid = 1;
        
        if (ch == '\n')
            grid = 0;
        
        if ((ch == '}') && (grid == 0))
            return 0;
        
        if ((ch == '{') && (grid == 0))
            return 1;
    }

    return 0;
}
//======================================================================
void create_fcgi_list(fcgi_list_addr **l, const String &s1, const String &s2)
{
    if (l == NULL)
    {
        fprintf(stderr, "<%s:%d> Error pointer = NULL\n", __func__, __LINE__);
        exit(errno);
    }

    fcgi_list_addr *t;
    try {
        t = new fcgi_list_addr;
    }
    catch (...)
    {
        fprintf(stderr, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(errno);
    }

    t->next = *l;
    t->scrpt_name = s1;
    t->addr = s2;
    *l = t;
}
//======================================================================
void read_conf_file(const char *path_conf)
{
    String s, ss, nameFile;

    nameFile << path_conf;
    nameFile << "/server.conf";

    ifstream fconf(nameFile.c_str(), ios::binary);
    if (!fconf.is_open())
    {
        create_conf_file(nameFile.c_str());
        fprintf(stderr, " Correct config file: %s\n", nameFile.c_str());
        exit(1);
    }

    c.index_html = c.index_php = c.index_pl = c.index_fcgi = 'n';
    c.fcgi_list = NULL;

    while (getLine(fconf, ss) > 0)
    {
        ss >> s;

        if (s ==  "ServerAddr")
            ss >> c.host;
        else if (s == "Port")
            ss >> c.servPort;
        else if (s == "ServerSoftware")
            ss >> c.ServerSoftware;
        else if (s == "tcp_cork")
            ss >> c.tcp_cork;
        else if (s == "TcpNoDelay")
            ss >> c.TcpNoDelay;
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
        else if (s == "TimeoutPoll")
            ss >> c.TIMEOUT_POLL;
        else if (s == "SndBufSize")
            ss >> c.SNDBUF_SIZE;
        else if (s == "MaxRequests")
            ss >> c.MAX_REQUESTS;
        else if (s == "NumProc")
            ss >> c.NumProc;
        else if (s == "MaxThreads")
            ss >> c.MaxThreads;
        else if (s == "MinThreads")
            ss >> c.MinThreads;
        else if (s == "MaxCgiProc")
            ss >> c.MaxCgiProc;
        else if (s == "KeepAlive")
            ss >> c.KeepAlive;
        else if (s == "TimeoutKeepAlive")
            ss >> c.TimeoutKeepAlive;
        else if (s == "TimeOut")
            ss >> c.TimeOut;
        else if (s == "TimeoutCGI")
            ss >> c.TimeoutCGI;
        else if (s == "MaxRanges")
            ss >> c.MaxRanges;
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
            if (find_bracket(fconf) == 0)
            {
                fprintf(stderr, "<%s:%d> Error Error not found \"{\"\n", __func__, __LINE__);
                exit(1);
            }

            while (getLine(fconf, ss) > 0)
            {
                ss >> s;
                if (s == "}")
                    break;

                if (s == "{")
                {
                    fprintf(stderr, "<%s:%d> Error read config file\n", __func__, __LINE__);
                    exit(1);
                }

                if (s == "index.html")
                    c.index_html = 'y';
                else if (s == "index.php")
                    c.index_php = 'y';
                else if (s == "index.pl")
                    c.index_pl = 'y';
                else if (s == "index.fcgi")
                    c.index_fcgi = 'y';
                else
                    printf("<%s:%d> Error read conf file(): \"index\" [%s]\n", __func__, __LINE__, s.c_str()), exit(1);
            }

            if (s != "}")
            {
                fprintf(stderr, "<%s:%d> Error not found \"}\"\n", __func__, __LINE__);
                exit(1);
            }
        }
        else if (s == "User")
            ss >> c.user;
        else if (s == "Group")
            ss >> c.group;
        else if (s == "fastcgi")
        {
            if (find_bracket(fconf) == 0)
            {
                fprintf(stderr, "<%s:%d> Error Error not found \"{\"\n", __func__, __LINE__);
                exit(1);
            }

            while (getLine(fconf, ss) > 0)
            {
                ss >> s;
                if (s == "}")
                    break;
                if (s == "{")
                {
                    fprintf(stderr, "<%s:%d> Error read config file\n", __func__, __LINE__);
                    exit(1);
                }

                String sTmp;
                ss >> sTmp;
                create_fcgi_list(&c.fcgi_list, s, sTmp);
            }

            if (s != "}")
            {
                fprintf(stderr, "<%s:%d> Error not found \"}\"\n", __func__, __LINE__);
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error read config file: [%s]\n", __func__, __LINE__, s.c_str());
            exit(1);
        }
    }

    fconf.close();

    fcgi_list_addr *i = c.fcgi_list;
    for (; i; i = i->next)
    {
        fprintf(stderr, "[%s] = [%s]\n", i->scrpt_name.c_str(), i->addr.c_str());
    }
    //------------------------------------------------------------------
    if (check_path(c.logDir) == -1)
    {
        fprintf(stderr, "!!! Error LogPath [%s]\n", c.logDir.c_str());
        exit(1);
    }
    create_logfiles(c.logDir, c.ServerSoftware);
    //------------------------------------------------------------------
    if (check_path(c.rootDir) == -1)
    {
        fprintf(stderr, "!!! Error DocumentRoot [%s]\n", c.rootDir.c_str());
        exit(1);
    }
    //------------------------------------------------------------------
    if (check_path(c.cgiDir) == -1)
    {
        c.cgiDir = "";
        fprintf(stderr, "!!! Error ScriptPath [%s]\n", c.cgiDir.c_str());
    }

    if ((c.NumProc < 1) || (c.NumProc > 8))
    {
        print_err("<%s:%d> Error: Number of Processes = %d; [1 < NumChld <= 6]\n", __func__, __LINE__, c.NumProc);
        exit(1);
    }

    if (c.MinThreads > c.MaxThreads)
    {
        print_err("<%s:%d> Error: NumThreads > MaxThreads\n", __func__, __LINE__);
        exit(1);
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
        long max_fd = (c.MAX_REQUESTS * 2) + minOpenFD;
        if (max_fd > (long)lim.rlim_cur)
        {
            if (max_fd > (long)lim.rlim_max)
                lim.rlim_cur = lim.rlim_max;
            else
                lim.rlim_cur = max_fd;

            if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
                print_err("<%s:%d> Error setrlimit(RLIMIT_NOFILE): %s\n", __func__, __LINE__, strerror(errno));
            max_fd = (long)sysconf(_SC_OPEN_MAX);
            if (max_fd > 1)
            {
                print_err("<%s:%d> _SC_OPEN_MAX=%d\n", __func__, __LINE__, (int)max_fd);
                c.MAX_REQUESTS = (max_fd - minOpenFD)/2;
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
        c.server_uid = strtol(c.user.c_str(), &p, 0);
        if (*p == '\0')
        {
            struct passwd *passwdbuf = getpwuid(c.server_uid);
            if (!passwdbuf)
            {
                print_err("<%s:%d> Error getpwuid(): %s\n", __func__, __LINE__, c.server_uid);
                exit(1);
            }
        }
        else
        {
            struct passwd *passwdbuf = getpwnam(c.user.c_str());
            if (!passwdbuf)
            {
                print_err("<%s:%d> Error getpwnam(): %s\n", __func__, __LINE__, c.user.c_str());
                exit(1);
            }
            c.server_uid = passwdbuf->pw_uid;
        }

        c.server_gid = strtol(c.group.c_str(), &p, 0);
        if (*p == '\0')
        {
            struct group *groupbuf = getgrgid(c.server_gid);
            if (!groupbuf)
            {
                print_err("<%s:%d> Error getgrgid(): %u\n", __func__, __LINE__, c.server_gid);
                exit(1);
            }
        }
        else
        {
            struct group *groupbuf = getgrnam(c.group.c_str());
            if (!groupbuf)
            {
                print_err("<%s:%d> Error getgrnam(): %s\n", __func__, __LINE__, c.group.c_str());
                exit(1);
            }
            c.server_gid = groupbuf->gr_gid;
        }
        //--------------------------------------------------------------
        if (c.server_uid != uid)
        {
            if (setuid(c.server_uid) == -1)
            {
                print_err("<%s:%d> Error setuid(%u): %s\n", __func__, __LINE__, c.server_uid, strerror(errno));
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
void set_sndbuf(int n)
{
    c.SNDBUF_SIZE = n;
}
