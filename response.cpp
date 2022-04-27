#include "classes.h"

using namespace std;

//======================================================================
void response1(RequestManager *ReqMan)
{
    const char *p;
    Connect *req;

    while(1)
    {
        req = ReqMan->pop_resp_list();
        if (!req)
        {
            ReqMan->end_thr(1);
            return;
        }
        else if (req->clientSocket < 0)
        {
            ReqMan->end_thr(1);
            delete req;
            return;
        }
        /*------------------------------------------------------------*/
        get_time(req->resp.sLogTime);
        int ret = parse_startline_request(req, req->arrHdrs[0].ptr, req->arrHdrs[0].len);
        if (ret)
        {
            print_err(req, "<%s:%d>  Error parse_startline_request(): %d\n", __func__, __LINE__, ret);
            goto end;
        }
     
        for (int i = 1; i < req->i_arrHdrs; ++i)
        {
            ret = parse_headers(req, req->arrHdrs[i].ptr, req->arrHdrs[i].len);
            if (ret < 0)
            {
                print_err(req, "<%s:%d>  Error parse_headers(): %d\n", __func__, __LINE__, ret);
                goto end;
            }
        }
        
        if (conf->tcp_cork == 'y')
        {
            int optval = 1;
            setsockopt(req->clientSocket, SOL_TCP, TCP_CORK, &optval, sizeof(optval));
        }
        /*--------------------------------------------------------*/
        if ((req->httpProt != HTTP10) && (req->httpProt != HTTP11))
        {
            req->httpProt = HTTP11;
            req->connKeepAlive = 0;
            req->err = -RS505;
            goto end;
        }

        if (req->numReq >= (unsigned int)conf->MaxRequestsPerThr || (conf->KeepAlive == 'n') || (req->httpProt == HTTP10))
            req->connKeepAlive = 0;
        else if (req->req_hdrs.iConnection == -1)
            req->connKeepAlive = 1;

        if ((p = strchr(req->uri, '?')))
        {
            req->uriLen = p - req->uri;
            req->sReqParam = req->uri + req->uriLen + 1;
        }
        else
        {
            req->sReqParam = NULL;
            req->uriLen = strlen(req->uri);
        }

        if (decode(req->uri, req->uriLen, req->decodeUri, sizeof(req->decodeUri)) <= 0)
        {
            print_err(req, "<%s:%d> Error: decode URI\n", __func__, __LINE__);
            req->err = -RS404;
            goto end;
        }
        
        clean_path(req->decodeUri);
        req->lenDecodeUri = strlen(req->decodeUri);

        if (strstr(req->uri, ".php") && (conf->UsePHP != "php-cgi") && (conf->UsePHP != "php-fpm"))
        {
            print_err(req, "<%s:%d> Error UsePHP=%s\n", __func__, __LINE__, conf->UsePHP.c_str());
            req->err = -RS404;
            goto end;
        }

        if (req->req_hdrs.iUpgrade >= 0)
        {
            print_err(req, "<%s:%d> req->upgrade: %s\n", __func__, __LINE__, req->req_hdrs.Value[req->req_hdrs.iUpgrade]);
            req->connKeepAlive = 0;
            req->err = -RS505;
            goto end;
        }
        //--------------------------------------------------------------
        if ((req->reqMethod == M_GET) || (req->reqMethod == M_HEAD) || (req->reqMethod == M_POST))
        {
            int ret = response2(req);
            if (ret == 1)
            {// "req" may be free !!!
                ret = ReqMan->end_thr(0);
                if (ret == EXIT_THR)
                    return;
                else
                    continue;
            }
            
            req->err = ret;
        }
        else if (req->reqMethod == M_OPTIONS)
        {   
            req->err = options(req);
        }
        else
            req->err = -RS501;

    end:

        if (req->err <= -RS101)
        {
            req->resp.respStatus = -req->err;
            send_message(req, "", NULL);

            if ((req->reqMethod == M_POST) || (req->reqMethod == M_PUT))
                req->connKeepAlive = 0;
        }

        end_response(req);
        
        ret = ReqMan->end_thr(0);
        if (ret)
        {
            return;
        }
    }
}
//======================================================================
int send_file(Connect *req);
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
    if (!p)
        return -RS404;
    
    fcgi_list_addr* i = conf->fcgi_list;
    for (; i; i = i->next)
    {
        if (i->scrpt_name[0] == '~')
        {
            if (!strcmp(p, i->scrpt_name.c_str() + 1))
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
    req->resp.scriptName = i->scrpt_name.c_str();
    int ret = fcgi(req);
    req->resp.scriptName = NULL;
    return ret;
}
//======================================================================
int response2(Connect *req)
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

        req->resp.scriptName = req->decodeUri;

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
        req->resp.scriptName = req->decodeUri;
        ret = cgi(req);
        return ret;
    }
    //------------------------------------------------------------------
    String path;
//  path.reserve(req->conf->rootDir.size() + req->lenDecodeUri + 256);
//  path = req->conf->rootDir;
    
    path.reserve(1 + req->lenDecodeUri + 16);
    path << '.';
    
    path << req->decodeUri;
    if (path[path.size()-1] == '/')
        path.resize(path.size() - 1);

    if (lstat(path.c_str(), &st) == -1)
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
    //------------------------------------------------------------------
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

            send_message(req, s.c_str(), &hdrs);
            return 0;
        }
        //--------------------------------------------------------------
        int len = path.size();
        path << "/index.html";
        if ((stat(path.c_str(), &st) != 0) || (conf->index_html != 'y'))
        {
            errno = 0;
            path.resize(len);

            int ret;
            if ((conf->UsePHP != "n") && (conf->index_php == 'y'))
            {
                path << "/index.php";
                if (!stat(path.c_str(), &st))
                {
                    String s = req->decodeUri;
                    s << "index.php";
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
    //------------------------------------------------------------------
    req->resp.fileSize = file_size(path.c_str());
    req->resp.numPart = 0;
    req->resp.respContentType = content_type(path.c_str());
    //------------------------------------------------------------------
    if (req->reqMethod == M_HEAD)
    {
        if (send_response_headers(req, NULL))
            return -1;
        
        return 0;
    }
    //------------------------------------------------------------------
    req->resp.fd = open(path.c_str(), O_RDONLY);
    if (req->resp.fd == -1)
    {
        if (errno == EACCES)
            return -RS403;
        else
        {
            print_err(req, "<%s:%d> Error open(%s): %s\n", __func__, __LINE__, 
                                    path.c_str(), strerror(errno));
            return -RS500;
        }
    }
    path.reserve(0);
    
    int ret = send_file(req);
    if (ret != 1)
        close(req->resp.fd);
    
    return ret;
}
//======================================================================
int send_file(Connect *req)
{
    if (req->req_hdrs.iRange >= 0)
    {
        if (!req->sRange)
            return -RS416;
        
        int err;
        
        ArrayRanges rg(req->sRange, req->resp.fileSize);
        if ((err = rg.error()))
        {
            print_err(req, "<%s:%d> Error create_ranges\n", __func__, __LINE__);
            return err;
        }
        
        req->resp.numPart = rg.size();
        if (req->resp.numPart > 1)
        {
            req->resp.respStatus = RS206;
            
            int size = conf->SNDBUF_SIZE; // 8192
            char *rd_buf = new(nothrow) char [size];
            if (!rd_buf)
            {
                print_err(req, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
                return -1;
            }

            int n = send_multy_part(req, rg, rd_buf, size);
            delete [] rd_buf;
            return n;
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
            print_err(req, "<%s:%d> ????????????????????????\n", __func__, __LINE__);
            exit(1);
            return -RS416;
        }
    }
    else
    {
        req->resp.respStatus = RS200;
        req->resp.offset = 0;
        req->resp.respContentLength = req->resp.fileSize;
    }
    
    if (send_response_headers(req, NULL))
        return -1;
    
    push_pollout_list(req);

    return 1;
}
//======================================================================
int send_multy_part(Connect *req, ArrayRanges& rg, char *rd_buf, int size)
{
    int n;
    long long send_all_bytes = 0;
    char buf[1024];
    Range *range;
    
    for (int i = 0; (range = rg.get(i)) && (i < req->resp.numPart); ++i)
    {
        send_all_bytes += (range->part_len);
        send_all_bytes += create_multipart_head(req, range, buf, sizeof(buf));
    }
    send_all_bytes += snprintf(buf, sizeof(buf), "\r\n--%s--\r\n", boundary);
    req->resp.respContentLength = send_all_bytes;

    String hdrs(256);
    hdrs << "Content-Type: multipart/byteranges; boundary=" << boundary << "\r\n";
    hdrs << "Content-Length: " << send_all_bytes << "\r\n";
    if (hdrs.error())
    {
        print_err(req, "<%s:%d> Error create response headers\n", __func__, __LINE__);
        return -1;
    }
    
    if (send_response_headers(req, &hdrs))
    {
        return -1;
    }
    
    send_all_bytes = 0;
    
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
            print_err(req, "<%s:%d> Error: write_timeout(), %lld bytes\n", __func__, __LINE__, send_all_bytes);
            return -1;
        }
        
        send_all_bytes += n;
        send_all_bytes += range->part_len;

        if (send_largefile(req, rd_buf, size, range->start, &range->part_len))
        {
            print_err(req, "<%s:%d> Error: send_file_ux()\n", __func__, __LINE__);
            return -1;
        }
    }

    req->resp.send_bytes = send_all_bytes;
    snprintf(buf, sizeof(buf), "\r\n--%s--\r\n", boundary);
    n = write_to_client(req, buf, strlen(buf), conf->TimeOut);
    if (n < 0)
    {
        print_err(req, "<%s:%d> Error: write_timeout() %lld bytes\n", __func__, __LINE__, send_all_bytes);
        return -1;
    }
    req->resp.send_bytes += n;
    return 0;
}
//======================================================================
int create_multipart_head(Connect *req, struct Range *ranges, char *buf, int len_buf)
{
    int n, all = 0;
    
    n = snprintf(buf, len_buf, "\r\n--%s\r\n", boundary);
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
//======================================================================
int options(Connect *req)
{
    req->resp.respStatus = RS200;
    if (send_response_headers(req, NULL))
    {
        return -1;
    }

    return 0;
}
