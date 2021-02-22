#include "classes.h"

using namespace std;

int send_multy_part(Connect *req, ArrayRanges& rg, char *rd_buf, int size);
int create_multipart_head(Connect *req, Range *ranges, char *buf, int len_buf);
const char boundary[] = "---------a9b5r7a4c0a2d5a1b8r3a";
//======================================================================
long long file_size(const char *s)
{
    struct stat st;

    if (!stat(s, &st))
        return st.st_size;
    else
        return -1;
}
//======================================================================
int fastcgi(Connect* req, const char* uri)
{
    const char* p = strrchr(uri, '/');
    if (!p) return -RS404;
    
    fcgi_list_addr* i = conf->fcgi_list;
    for (; i; i = i->next)
    {
        if (i->scrpt_name[0] == '~')
        {
            if (!strcmp(p, i->scrpt_name.str() + 1))
                break;
        }
        else
        {
            if (uri == i->scrpt_name)
                break;
        }
        
    }

    if (!i)
        return -RS404;
    req->resp.scriptType = fast_cgi;
    req->resp.scriptName = i->scrpt_name.str();
    int ret = fcgi(req);
    req->resp.scriptName = NULL;
    return ret;
}
//======================================================================
int response(Connect *req)
{
    struct stat st;
    char *p = strstr(req->decodeUri, ".php");
    if (p && (*(p + 4) == 0))
    {
        int ret;
        if ((conf->UsePHP != "php-cgi") && (conf->UsePHP != "php-fpm"))
        {
            print_err(req, "<%s:%d> Not found: %s\n", __func__, __LINE__, req->decodeUri);
            return -RS404;
        }
        
        struct stat st;
        if (stat(req->decodeUri + 1, &st) == -1)
        {
            print_err(req, "<%s:%d> script (%s) not found\n", __func__, __LINE__, req->decodeUri);
            return -RS404;
        }
        
        string s = req->decodeUri;
        req->resp.scriptName = s.c_str();

        if (conf->UsePHP == "php-fpm")
        {
            req->resp.scriptType = php_fpm;
            ret = fcgi(req);
        }
        else if (conf->UsePHP == "php-cgi")
        {
            req->resp.scriptType = php_cgi;
            ret = cgi(req);
        }
        else
            ret = -1;

        req->resp.scriptName = NULL;
        return ret;
    }
    
    if (!strncmp(req->decodeUri, "/cgi-bin/", 9) 
        || !strncmp(req->decodeUri, "/cgi/", 5))
    {
        int ret;
        req->resp.scriptType = cgi_ex;
        
        string s = req->decodeUri;
        req->resp.scriptName = s.c_str();

        ret = cgi(req);
        return ret;
    }
    //------------------------- dir or file ----------------------------
    String path;
//  path.reserve(req->conf->rootDir.size() + req->lenDecodeUri + 256);
//  path = req->conf->rootDir;
    
    path.reserve(1 + req->lenDecodeUri + 16);
    path << '.';
    
    path << req->decodeUri;
    if (path[path.len()-1] == '/')
        path.resize(path.len() - 1);

    if (lstat(path.str(), &st) == -1)
    {
    //    print_err(req, "<%s:%d> Error lstat(\"%s\"): %s\n", __func__, __LINE__, req->decodeUri, strerror(errno));
        if (errno == EACCES)
            return -RS403;
        return fastcgi(req, req->decodeUri);
    }
    else
    {
        if ((!S_ISDIR(st.st_mode)) && (!S_ISREG(st.st_mode)))
        {
            print_err(req, "<%s:%d> Error: file (!S_ISDIR && !S_ISREG) \n", __func__, __LINE__);
            return -RS403;
        }
    }
    //------------------------ list dir --------------------------------
    if (S_ISDIR(st.st_mode))
    {
        if (req->uri[req->uriLen - 1] != '/')
        {
            req->uri[req->uriLen] = '/';
            req->uri[req->uriLen + 1] = '\0';
            req->resp.respStatus = RS301;
            
            String hdrs(127);
            hdrs << "Location: " << req->uri << "\r\n";
            if (hdrs.error())
            {
                print_err(req, "<%s:%d> Error\n", __func__, __LINE__);
                return -RS500;
            }
            
            String s(256);
            s << "The document has moved <a href=\"" << req->uri << "\">here</a>.";
            if (s.error())
            {
                print_err(req, "<%s:%d> Error create_header()\n", __func__, __LINE__);
                return -RS500;
            }

            send_message(req, s.str(), &hdrs);
            return 0;
        }
        //--------------------------------------------------------------
        int len = path.len();
        path << "/index.html";
        if ((stat(path.str(), &st) != 0) || (conf->index_html != 'y'))
        {
            errno = 0;
            path.resize(len);

            int ret;
            if ((conf->UsePHP != "n") && (conf->index_php == 'y'))
            {
                path << "/index.php";
                if (!stat(path.str(), &st))
                {
                    String s = req->decodeUri;
                    s << "index.php";
                    req->resp.scriptName = s.str();
                    
                    if (conf->UsePHP == "php-fpm")
                    {
                        req->resp.scriptType = php_fpm;
                        ret = fcgi(req);
                    }
                    else if (conf->UsePHP == "php-cgi")
                    {
                        req->resp.scriptType = php_cgi;
                        ret = cgi(req);
                    }
                    else
                        ret = -1;
                    
                    req->resp.scriptName = NULL;
                    return ret;
                }
                path.resize(len);
            }
            
            if (conf->index_pl == 'y')
            {
                req->resp.scriptType = cgi_ex;
                req->resp.scriptName = "/cgi-bin/index.pl";
                ret = cgi(req);
                req->resp.scriptName = NULL;
                if (ret == 0)
                    return ret;
            }
            else if (conf->index_fcgi == 'y')
            {
                req->resp.scriptType = fast_cgi;
                req->resp.scriptName = "/index.fcgi";
                ret = fcgi(req);
                req->resp.scriptName = NULL;
                if (ret == 0)
                    return ret;
            }

            path.reserve(path.capacity() + 256);
            return index_dir(req, path);
        }
    }
    //----------------------- send file --------------------------------
    req->resp.fileSize = file_size(path.str());
    req->resp.numPart = 0;
    req->resp.respContentType = content_type(path.str());

    ArrayRanges rg;
    if (req->req_hdrs.iRange >= 0)
    {
        if (!req->sRange)
            return -RS416;
        req->resp.numPart = rg.get_ranges(req->sRange, req->resp.fileSize);
        if (rg.error())
        {
            print_err(req, "<%s:%d> Error create_ranges\n", __func__, __LINE__);
            return -1;
        }
        
        if (req->resp.numPart > 1)
        {
            if (req->reqMethod == M_HEAD)
                return -RS405;
            req->resp.respStatus = RS206;
        }
        else if (req->resp.numPart == 1)
        {
            req->resp.respStatus = RS206;
            Range *pr = rg.get(0);
            if (pr)
            {
                req->resp.offset = pr->start;
                req->resp.respContentLength = pr->part_len;
            }
        }
        else
        {
            return -RS416;
        }
    }
    else
    {
        req->resp.respStatus = RS200;
        req->resp.offset = 0;
        req->resp.respContentLength = req->resp.fileSize;
    }
    //------------------------------------------------------------------
    req->resp.fd = open(path.str(), O_RDONLY);
    if (req->resp.fd == -1)
    {
        if (errno == EACCES)
            return -RS403;
        else
        {
            print_err(req, "<%s:%d> Error open(%s): %s\n", __func__, __LINE__, 
                                    path.str(), strerror(errno));
            return -RS500;
        }
    }
    
    path.reserve(0);
    
    if (req->resp.numPart > 1)
    {
        int size = conf->WR_BUFSIZE;
        char *rd_buf = new(nothrow) char [size];
        if (!rd_buf)
        {
            print_err(req, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
            close(req->resp.fd);
            return -1;
        }

        int n = send_multy_part(req, rg, rd_buf, size);
        close(req->resp.fd);
        delete [] rd_buf;
        return n;
    }
    
    if (send_response_headers(req, NULL))
    {
        close(req->resp.fd);
        return -1;
    }
    
    if (req->reqMethod == M_HEAD)
    {
        close(req->resp.fd);
        return 0;
    }

    push_resp_queue1(req);

    return 1;
}
//======================================================================
int send_multy_part(Connect *req, ArrayRanges& rg, char *rd_buf, int size)
{
    int n;
    long long send_all_bytes = 0;
    char buf[1024];
    //------------------------------------------------------------------
    long long all_bytes = 0;
                
    all_bytes += 2;
    Range *range;
    for (int i = 0; (range = rg.get(i)) && (i < req->resp.numPart); ++i)
    {
        all_bytes += (range->part_len + 2);
        all_bytes += create_multipart_head(req, range, buf, sizeof(buf));
    }
    all_bytes += snprintf(buf, sizeof(buf), "--%s--\r\n", boundary);
    req->resp.respContentLength = all_bytes;

    String hdrs(256);
    hdrs << "Content-Type: multipart/byteranges; boundary=" << boundary << "\r\n";
    hdrs << "Content-Length: " << all_bytes << "\r\n";
    if (hdrs.error())
    {
        print_err(req, "<%s:%d> Error create response headers\n", __func__, __LINE__);
        return -1;
    }
    
    if (send_response_headers(req, &hdrs))
    {
        return -1;
    }
    
    send_all_bytes += 2;
    
    for (int i = 0; (range = rg.get(i)) && (i < req->resp.numPart); ++i)
    {
        if ((n = create_multipart_head(req, range, buf, sizeof(buf))) == 0)
        {
            print_err(req, "<%s:%d> Error create_multipart_head()=%d\n", __func__, __LINE__, n);
            return -1;
        } 

        n = write_to_client(req, buf, strlen(buf), conf->TimeOut);
        if (n < 0)
        {
            print_err(req, "<%s:%d> Error: write_timeout(), %lld bytes from %lld bytes\n", __func__, __LINE__, send_all_bytes, all_bytes);
            return -1;
        }
        
        send_all_bytes += n;
        send_all_bytes += range->part_len;

        if (send_largefile(req, rd_buf, size, range->start, &range->part_len))
        {
            print_err(req, "<%s:%d> Error: send_file_ux()\n", __func__, __LINE__);
            return -1;
        }

        n = write_to_client(req, "\r\n", 2, conf->TimeOut);
        if (n < 0)
        {
            print_err(req, "<%s:%d> Error: write_timeout() %lld bytes from %lld bytes\n", __func__, __LINE__, send_all_bytes, all_bytes);
            return -1;
        }
        send_all_bytes += n;
    }

    req->resp.send_bytes = send_all_bytes;
    snprintf(buf, sizeof(buf), "--%s--\r\n", boundary);
    n = write_to_client(req, buf, strlen(buf), conf->TimeOut);
    if (n < 0)
    {
        print_err(req, "<%s:%d> Error: write_timeout() %lld bytes from %lld bytes\n", __func__, __LINE__, send_all_bytes, all_bytes);
        return -1;
    }
    req->resp.send_bytes += n;
    return 0;
}
/*====================================================================*/
int create_multipart_head(Connect *req, struct Range *ranges, char *buf, int len_buf)
{
    int n, all = 0;
    
    n = snprintf(buf, len_buf, "--%s\r\n", boundary);
    buf += n;
    len_buf -= n;
    all += n;

    if (req->resp.respContentType && (len_buf > 0))
    {
        n = snprintf(buf, len_buf, "Content-Type: %s\r\n", req->resp.respContentType);
        buf += n;
        len_buf -= n;
        all += n;
    }
    else
        return 0;
    
    if (len_buf > 0)
    {
        n = snprintf(buf, len_buf,
            "Content-Range: bytes %lld-%lld/%lld\r\n\r\n",
             ranges->start, ranges->end, req->resp.fileSize);
        buf += n;
        len_buf -= n;
        all += n;
    }
    else
        return 0;

    return all;
}
