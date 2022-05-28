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
    FILE *f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "<%s> Error create conf file (%s): %s\n", __func__, path, strerror(errno));
        exit(1);
    }

    fprintf(f, "ServerSoftware   %s\n", c.ServerSoftware.c_str());
    fprintf(f, "ServerAddr   %s\n", c.host.c_str());
    fprintf(f, "Port         %s\n", c.servPort.c_str());
    fprintf(f, "tcp_cork   %c\n", c.tcp_cork);
    fprintf(f, "TcpNoDelay   y\n");
    fprintf(f, "DocumentRoot %s\n", c.rootDir.c_str());
    fprintf(f, "ScriptPath   %s\n", c.cgiDir.c_str());
    fprintf(f, "LogPath      %s\n", c.logDir.c_str());
    fprintf(f, "MaxRequestsPerThr %d\n", c.MaxRequestsPerThr);
    fprintf(f, "ListenBacklog %d\n", c.ListenBacklog);
    fprintf(f, "SndBufSize   %d\n", c.SNDBUF_SIZE);
    fprintf(f, "MaxRequests %d\n", c.MAX_REQUESTS);
    fprintf(f, "SendFile  %c\n", c.SEND_FILE);
    fprintf(f, "TimeoutPoll  %d\n", c.TIMEOUT_POLL);
    fprintf(f, "NumProc %d\n", c.NumProc);
    fprintf(f, "MaxThreads %d\n", c.MaxThreads);
    fprintf(f, "MinThreads %d\n", c.MinThreads);
    fprintf(f, "MaxCgiProc %d\n", c.MaxCgiProc);
    fprintf(f, "KeepAlive  %c   #  y/n\n", c.KeepAlive);
    fprintf(f, "TimeoutKeepAlive %d\n", c.TimeoutKeepAlive);
    fprintf(f, "TimeOut    %d\n", c.TimeOut);
    fprintf(f, "TimeoutCGI %d\n\n", c.TimeoutCGI);
    fprintf(f, "MaxRanges  %d\n\n", c.MaxRanges);
    fprintf(f, "ClientMaxBodySize %ld\n", c.ClientMaxBodySize);
    fprintf(f, " UsePHP     n  # php-fpm # php-cgi \n");
    fprintf(f, "# PathPHP   /usr/bin/php-cgi\n");
    fprintf(f, "# PathPHP  127.0.0.1:9000  #  /run/php/php7.0-fpm.sock \n\n");
    fprintf(f, "index {\n"
                "\t#index.html\n"
                "}\n\n");

    fprintf(f, "fastcgi {\n"
                "\t#/test  127.0.0.1:9009\n"
                "}\n\n");

    fprintf(f, "ShowMediaFiles %c #  y/n\n", c.ShowMediaFiles);
    fprintf(f, "User nobody     # www-data\n");
    fprintf(f, "Group nogroup   # www-data\n\n");

    fclose(f);
}
//======================================================================
int getLine(FILE *f, String &ss)
{
    ss = "";
    int ch, len = 0, numWords = 0, wr = 1, wrSpace = 0;

    while (((ch = getc(f)) != EOF))
    {
        if ((char)ch == '\n')
        {
            if (len)
                return ++numWords;
            else
            {
                wr = 1;
                ss = "";
                wrSpace = 0;
                continue;
            }
        }

        if (wr == 0)
            continue;

        switch (ch)
        {
            case ' ':
            case '\t':
                if (len)
                    wrSpace = 1;
            case '\r':
                break;
            case '#':
                wr = 0;
                break;
            case '{':
            case '}':
                if (len)
                    fseek(f, -1, 1); // ungetc(ch, f);
                else
                {
                    ss << (char)ch;
                    ++len;
                }
                
                return ++numWords;
            default:
                if (wrSpace)
                {
                    ss << " ";
                    ++len;
                    ++numWords;
                    wrSpace = 0;
                }
                
                ss << (char)ch;
                ++len;
        }
    }

    if (len)
        return ++numWords;
    return -1;
}
//======================================================================
int isnumber(const char *s)
{
    if (!s)
        return 0;
    int n = isdigit((int)*(s++));
    while (*s && n)
        n = isdigit((int)*(s++));
    return n;
}
//======================================================================
int isbool(const char *s)
{
    if (!s)
        return 0;
    if (strlen(s) != 1)
        return 0;
    return ((s[0] == 'y') || (s[0] == 'n'));
}
//======================================================================
int find_bracket(FILE *f)
{
    int ch, grid = 0;

    while (((ch = getc(f)) != EOF))
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
        fprintf(stderr, "<%s:%d> Error pointer = NULL\n", __func__, __LINE__), exit(errno);

    fcgi_list_addr *t;
    try
    {
        t = new fcgi_list_addr;
    }
    catch (...)
    {
        fprintf(stderr, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        exit(errno);
    }

    t->scrpt_name = s1;
    t->addr = s2;
    t->next = *l;
    *l = t;
}
//======================================================================
void read_conf_file(const char *path_conf)
{
    String ss, nameFile;

    nameFile << path_conf;
    nameFile << "/server.conf";

    FILE *fconf = fopen(nameFile.c_str(), "r");
    if (!fconf)
    {
        create_conf_file(nameFile.c_str());
        fprintf(stderr, " Correct config file: %s\n", nameFile.c_str());
        exit(1);
    }

    c.index_html = c.index_php = c.index_pl = c.index_fcgi = 'n';
    c.fcgi_list = NULL;

    int n;
    while ((n = getLine(fconf, ss)) > 0)
    {
        if (n == 2)
        {
            String s1, s2;
            ss >> s1;
            ss >> s2;

            if (s1 ==  "ServerAddr")
                s2 >> c.host;
            else if (s1 == "Port")
                s2 >> c.servPort;
            else if (s1 == "ServerSoftware")
                s2 >> c.ServerSoftware;
            else if ((s1 == "tcp_cork") && isbool(s2.c_str()))
                s2 >> c.tcp_cork;
            else if ((s1 == "TcpNoDelay") && isbool(s2.c_str()))
                s2 >> c.TcpNoDelay;
            else if ((s1 == "ListenBacklog") && isnumber(s2.c_str()))
                s2 >> c.ListenBacklog;
            else if ((s1 == "SendFile") && isbool(s2.c_str()))
                s2 >> c.SEND_FILE;
            else if ((s1 == "SndBufSize") && isnumber(s2.c_str()))
                s2 >> c.SNDBUF_SIZE;
            else if ((s1 == "MaxRequests") && isnumber(s2.c_str()))
                s2 >> c.MAX_REQUESTS;
            else if ((s1 == "TimeoutPoll") && isnumber(s2.c_str()))
                s2 >> c.TIMEOUT_POLL;
            else if (s1 == "DocumentRoot")
                s2 >> c.rootDir;
            else if (s1 == "ScriptPath")
                s2 >> c.cgiDir;
            else if (s1 == "LogPath")
                s2 >> c.logDir;
            else if ((s1 == "NumProc") && isnumber(s2.c_str()))
                s2 >> c.NumProc;
            else if ((s1 == "MaxThreads") && isnumber(s2.c_str()))
                s2 >> c.MaxThreads;
            else if ((s1 == "MinThreads") && isnumber(s2.c_str()))
                s2 >> c.MinThreads;
            else if ((s1 == "MaxCgiProc") && isnumber(s2.c_str()))
                s2 >> c.MaxCgiProc;
            else if ((s1 == "MaxRequestsPerThr") && isnumber(s2.c_str()))
                s2 >> c.MaxRequestsPerThr;
            else if ((s1 == "KeepAlive") && isbool(s2.c_str()))
                s2 >> c.KeepAlive;
            else if ((s1 == "TimeoutKeepAlive") && isnumber(s2.c_str()))
                s2 >> c.TimeoutKeepAlive;
            else if ((s1 == "TimeOut") && isnumber(s2.c_str()))
                s2 >> c.TimeOut;
            else if ((s1 == "TimeoutCGI") && isnumber(s2.c_str()))
                s2 >> c.TimeoutCGI;
            else if ((s1 == "MaxRanges") && isnumber(s2.c_str()))
                s2 >> c.MaxRanges;
            else if (s1 == "UsePHP")
                s2 >> c.UsePHP;
            else if (s1 == "PathPHP")
                s2 >> c.PathPHP;
            else if ((s1 == "ShowMediaFiles") && isbool(s2.c_str()))
                s2 >> c.ShowMediaFiles;
            else if ((s1 == "ClientMaxBodySize") && isnumber(s2.c_str()))
                s2 >> c.ClientMaxBodySize;
            else if (s1 == "User")
                s2 >> c.user;
            else if (s1 == "Group")
                s2 >> c.group;
            else
                fprintf(stderr, "<%s:%d> Error read config file: [%s]\n", __func__, __LINE__, ss.c_str()), exit(1);
        }
        else if (n == 1)
        {
            if (ss == "index")
            {
                if (find_bracket(fconf) == 0)
                {
                    fprintf(stderr, "<%s:%d> Error Error not found \"{\"\n", __func__, __LINE__);
                    exit(1);
                }

                while (getLine(fconf, ss) == 1)
                {
                    if (ss == "}")
                        break;

                    if (ss == "{")
                        fprintf(stderr, "<%s:%d> Error read config file\n", __func__, __LINE__), exit(1);

                    if (ss == "index.html")
                        c.index_html = 'y';
                    else if (ss == "index.php")
                    c.index_php = 'y';
                    else if (ss == "index.pl")
                        c.index_pl = 'y';
                    else if (ss == "index.fcgi")
                        c.index_fcgi = 'y';
                    else
                        fprintf(stderr, "<%s:%d> Error read conf file(): \"index\" [%s]\n", __func__, __LINE__, ss.c_str()), exit(1);
                }

                if (ss != "}")
                {
                    fprintf(stderr, "<%s:%d> Error not found \"}\"\n", __func__, __LINE__);
                    exit(1);
                }
            }
            else if (ss == "fastcgi")
            {
                if (find_bracket(fconf) == 0)
                {
                    fprintf(stderr, "<%s:%d> Error Error not found \"{\"\n", __func__, __LINE__);
                    exit(1);
                }

                while (getLine(fconf, ss) == 2)
                {
                    String s1, s2;
                    ss >> s1;
                    ss >> s2;

                    create_fcgi_list(&c.fcgi_list, s1, s2);
                }

                if (ss != "}")
                {
                    fprintf(stderr, "<%s:%d> Error not found \"}\"\n", __func__, __LINE__);
                    exit(1);
                }
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error read config file: [%s]\n", __func__, __LINE__, ss.c_str());
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error read config file: [%s]\n", __func__, __LINE__, ss.c_str());
            exit(1);
        }
    }

    fclose(fconf);

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
