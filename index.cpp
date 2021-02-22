#include "classes.h"

using namespace std;
//======================================================================
int isimage(const char *name)
{
    const char *p;

    p = strrchr(name, '.');
    if(!p)
        return 0;

    if(!strlcmp_case(p, ".gif", 4)) return 1;
    else if(!strlcmp_case(p, ".png", 4)) return 1;
    else if(!strlcmp_case(p, ".ico", 4)) return 1;
    else if(!strlcmp_case(p, ".svg", 4)) return 1;
    else if(!strlcmp_case(p, ".jpeg", 5) || !strlcmp_case(p, ".jpg", 4)) return 1;
    return 0;
}
//======================================================================
int isaudiofile(const char *name)
{
    const char *p;

    if(!(p = strrchr(name, '.'))) return 0;

    if(!strlcmp_case(p, ".wav", 4)) return 1;
    else if(!strlcmp_case(p, ".mp3", 4)) return 1;
    else if(!strlcmp_case(p, ".ogg", 4)) return 1;
    return 0;
}
//======================================================================
int cmp(const void *a, const void *b)
{
    unsigned int n1, n2;
    int i;

    if((n1 = atoi(*(char **)a)) > 0)
    {
        if((n2 = atoi(*(char **)b)) > 0)
        {
            if(n1 < n2) i = -1;
            else if(n1 == n2)
                i = strcmp(*(char **)a, *(char **)b);
            else i = 1;
        }
        else i = strcmp(*(char **)a, *(char **)b);
    }
    else i = strcmp(*(char **)a, *(char **)b);

    return i;
}
//======================================================================
int index_chunked(Connect *req, char **list, int numFiles, String& path)
{
    const int len_path = path.len();
    int n, i;
    long long size;
    struct stat st;
    int chunk;
    if (req->reqMethod == M_HEAD)
        chunk = NO_SEND;
    else
        chunk = ((req->httpProt == HTTP11) && req->connKeepAlive) ? SEND_CHUNK : SEND_NO_CHUNK;

    ClChunked chunk_buf(req, chunk);
//print_err(req, "<%s:%d> ---------------\n", __func__, __LINE__);
    req->resp.respStatus = RS200;
    String hdrs(64);
    if (hdrs.error())
    {
        print_err(req, "<%s:%d> Error create String object\n", __func__, __LINE__);
        return -RS500;
    }
    if (chunk == SEND_CHUNK)
    {
        hdrs << "Transfer-Encoding: chunked\r\n";
    }

    hdrs << "Content-Type: text/html\r\n";
    req->resp.respContentLength = -1;
    
    if (chunk)
    {
        if (send_response_headers(req, &hdrs))
        {
            return -1;
        }
    }
    //------------------------------------------------------------------
    chunk_buf << "<!DOCTYPE HTML>\n"
            "<html>\n"
            " <head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <title>Index of " << req->decodeUri << " (ch)</title>\n"
            "  <style>\n"
            "    body {\n"
            "     margin-left:100px; margin-right:50px;\n"
            "    }\n"
            "  </style>\n"
            "  <link href=\"/styles.css\" type=\"text/css\" rel=\"stylesheet\">"
            " </head>\n"
            " <body id=\"top\">\n"
            "  <h3>Index of " << req->decodeUri << "</h3>\n"
            "  <table cols=\"2\" width=\"100\%\">\n"
            "   <tr><td><h3>Directories</h3></td><td></td></tr>\n";
    if (chunk_buf.error())
    {
        print_err("<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //------------------------------------------------------------------
    if(!strcmp(req->decodeUri, "/"))
        chunk_buf << "   <tr><td></td><td></td></tr>\n";
    else
        chunk_buf << "   <tr><td><a href=\"../\">Parent Directory/</a></td><td></td></tr>\n";
    if (chunk_buf.error())
    {
        print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //-------------------------- Directories ---------------------------
    for (i = 0; (i < numFiles); i++)
    {
        char buf[1024];
        path << list[i];
        n = lstat(path.str(), &st);
        path.resize(len_path);
        if ((n == -1) || !S_ISDIR (st.st_mode))
            continue;
        
        if (!encode(list[i], buf, sizeof(buf)))
        {
            print_err(req, "<%s:%d> Error: encode()\n", __func__, __LINE__);
            continue;
        }

        chunk_buf << "   <tr><td><a href=\"" << buf << "/\">" << list[i] << "/</a></td><td align=right></td></tr>\n";
        if (chunk_buf.error())
        {
            print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
            return -1;
        }
    }
    //------------------------------------------------------------------
    chunk_buf << "   <tr><td><hr></td><td><hr></td></tr>\n"
                "   <tr><td><h3>Files</h3></td><td></td></tr>\n";
    if (chunk_buf.error())
    {
        print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //---------------------------- Files -------------------------------
    for (i = 0; i < numFiles; i++)
    {
        char buf[1024];
        path << list[i];
        n = lstat(path.str(), &st);
        path.resize(len_path);
        if ((n == -1) || !S_ISREG (st.st_mode))
            continue;

        if (!encode(list[i], buf, sizeof(buf)))
        {
            print_err(req, "<%s:%d> Error: encode()\n", __func__, __LINE__);
            continue;
        }

        size = (long long)st.st_size;

        if(isimage(list[i]) && (conf->ShowMediaFiles == 'y'))
        {
            if(size < 15000LL)
                chunk_buf << "   <tr><td><a href=\"" << buf << "\"><img src=\"" << buf << "\"></a><br>"
                        << list[i] << "</td><td align=\"right\">" << size << " bytes</td></tr>\n   <tr><td></td><td></td></tr>\n";
            else
                chunk_buf << "   <tr><td><a href=\"" << buf << "\"><img src=\"" << buf << "\" width=\"300\"></a><br>"
                        << list[i] << "</td><td align=\"right\">" << size << " bytes</td></tr>\n   <tr><td></td><td></td></tr>\n";
        }
        else if(isaudiofile(list[i]) && (conf->ShowMediaFiles == 'y'))
            chunk_buf << "   <tr><td><audio preload=\"none\" controls src=\"" << buf << "\"></audio><a href=\""
                    << buf << "\">" << list[i] << "</a></td><td align=\"right\">" << size << " bytes</td></tr>\n";
        else
            chunk_buf << "   <tr><td><a href=\"" << buf << "\">" << list[i] << "</a></td><td align=\"right\">" 
                    << size << " bytes</td></tr>\n";
        
        if (chunk_buf.error())
        {
            print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
            return -1;
        }
    }
    //------------------------------------------------------------------
    chunk_buf << "  </table>\n"
              "  <hr>\n"
              "  " << req->resp.sLogTime << "\n"
              "  <a href=\"#top\" style=\"display:block;\n"
              "         position:fixed;\n"
              "         bottom:30px;\n"
              "         left:10px;\n"
              "         width:50px;\n"
              "         height:40px;\n"
              "         font-size:60px;\n"
              "         background:gray;\n"
              "         border-radius:10px;\n"
              "         color:black;\n"
              "         opacity: 0.7\">^</a>\n"
              " </body>\n"
              "</html>";
    if (chunk_buf.error())
    {
        print_err(req, "<%s:%d>   Error chunk\n", __func__, __LINE__);
        return -1;
    }
    //------------------------------------------------------------------
    n = chunk_buf.end();
    req->resp.respContentLength = chunk_buf.all();
    if (n < 0)
    {
        print_err(req, "<%s:%d>   Error chunk_buf.end(): %d\n", __func__, __LINE__, n);
        return -1;
    }
    
    if (chunk == NO_SEND)
    {
//print_err("<%s:%d> chunk.all() = %d\n", __func__, __LINE__, chunk.all());
        if (send_response_headers(req, &hdrs))
        {
            print_err("<%s:%d> Error send_header_response()\n", __func__, __LINE__);
            return -1;
        }
    }
    else
        req->resp.send_bytes = req->resp.respContentLength;

    return 0;
}
//======================================================================
int index_dir(Connect *req, String& path)
{
    DIR *dir;
    struct dirent *dirbuf;
    int maxNumFiles = 1024, numFiles = 0;
    char *list[maxNumFiles];
    int ret;

    path << '/';
    
    dir = opendir(path.str());
    if (dir == NULL)
    {
        if(errno == EACCES)
            return -RS403;
        else
        {
            print_err(req, "<%s:%d>  Error opendir(\"%s\"): %s\n", __func__, __LINE__, path.str(), strerror(errno));
            return -RS500;
        }
    }
    
    while ((dirbuf = readdir(dir)))
    {        
        if(numFiles >= maxNumFiles )
        {
            print_err(req, "<%s:%d> number of files per directory >= %d\n", __func__, __LINE__, numFiles);
            break;
        }
        
        if (dirbuf->d_name[0] == '.')
            continue;
        list[numFiles] = dirbuf->d_name;
        ++numFiles;
    }
    
    qsort(list, numFiles, sizeof(char *), cmp);
    ret = index_chunked(req, list, numFiles, path);

    closedir(dir);

    return ret;
}
