#include "classes.h"
#include "fcgi.h"

using namespace std;

//======================================================================
int get_sock_fcgi(Connect *req, const char *script)
{
    int fcgi_sock = -1, len;
    fcgi_list_addr *ps = conf->fcgi_list;
    
    if (!script)
    {
        print_err(req, "<%s:%d> Not found\n", __func__, __LINE__);
        return -RS404;
    }

    len = strlen(script);
    if (len > 64)
    {
        print_err(req, "<%s:%d> Error len name script\n", __func__, __LINE__);
        return -RS400;
    }
    
    for (; ps; ps = ps->next)
    {
        if (!strcmp(script, ps->scrpt_name.str()))
            break;
    }

    if (ps != NULL)
    {
        fcgi_sock = create_fcgi_socket(ps->addr.str());
        if (fcgi_sock < 0)
        {
            print_err(req, "<%s:%d> Error create_client_socket(%s): %s\n", __func__, __LINE__, ps->addr.str(), strerror(-fcgi_sock));
            fcgi_sock = -RS500;
        }
    }
    else
    {
        print_err(req, "<%s:%d> Not found: %s\n", __func__, __LINE__, script);
        fcgi_sock = -RS404;
    }

    return fcgi_sock;
}
//======================================================================
int fcgi_read_header(int fcgi_sock, fcgi_header *header)
{
    int n;
    char buf[8];
    n = read_timeout(fcgi_sock, buf, 8, conf->TimeoutCGI);
    if (n <= 0)
        return -1;

    header->type = (unsigned char)buf[1];
    header->paddingLen = (unsigned char)buf[6];
    header->len = ((unsigned char)buf[4]<<8) | (unsigned char)buf[5];
    
    return n;
}
//======================================================================
int fcgi_set_header(char *p, int type, int len, int make_padd)
{
    unsigned char padding = 0;
    if (make_padd)
    {
        padding = 8 - (len % 8);
        padding = (padding == 8) ? 0 : padding;
    }

    *p++ = FCGI_VERSION_1;
    *p++ = (unsigned char)type;
    *p++ = (unsigned char) ((1 >> 8) & 0xff);
    *p++ = (unsigned char) ((1) & 0xff);
    
    *p++ = (unsigned char) ((len >> 8) & 0xff);
    *p++ = (unsigned char) ((len) & 0xff);
    
    *p++ = padding;
    *p = 0;
    return padding;
}
//======================================================================
int fcgi_read_headers(char *s, int len, String *hdrs, int *stat)
{
    char *start_ptr = s;
    int i = len;
//hex_dump_stderr(__func__, __LINE__, s, len);
    while (1)
    {
        char *end_ptr;
        if (i <= 0)
        {
            printf("<%s:%d> i = %d\n", __func__, __LINE__, i);
            return -1;
        }
        end_ptr = (char*)memchr(start_ptr, '\n', i);
        if (end_ptr)
        {
            int n = end_ptr - start_ptr + 1;
            i -= n;
            
            if ((n > 1) && (*(end_ptr - 1) == '\r'))
                n -= 2;
            else if (n == 1)
                --n;
            
            if (n == 0)
                break;
            
            if(!strncmp("Status", start_ptr, 6))
            {
                *(start_ptr + n) = 0;
                *stat = atoi(start_ptr + 7);
            }
            else
            {
                hdrs->append(start_ptr, n);
                *(hdrs) << "\r\n";
            }
            start_ptr = end_ptr + 1;
        }
        else
        {
            printf("<%s:%d> ----\n", __func__, __LINE__);
            return -1;
        }
    }

    return len - i;
}
//======================================================================
int fcgi_(Connect *req, int fcgi_sock)
{
//print_err("<%s:%d> -------------------------\n", __func__, __LINE__);
    if(req->reqMethod == M_POST)
    {
        if (req->tail)
        {
            char buf[8];

            fcgi_set_header(buf, FCGI_STDIN, req->lenTail, 0);
            if (write_to_script(fcgi_sock, buf, 8, conf->TimeoutCGI) < 0)
                return -RS502;
    
            if (write_to_script(fcgi_sock, req->tail, req->lenTail, conf->TimeoutCGI) < 0)
                return -RS502;
            
            req->req_hdrs.reqContentLength -= req->lenTail;
        }

        while (req->req_hdrs.reqContentLength > 0)
        {
            const int size_buf = 2048;
            char buf[size_buf + 16];
            int rd = (req->req_hdrs.reqContentLength > size_buf) ? size_buf : (int)req->req_hdrs.reqContentLength;
            int ret = read_timeout(req->clientSocket, buf + 8, rd, conf->TimeOut);
            if (ret < 0)
            {
                print_err(req, "<%s:%d> Error: reaf_from_client()\n", __func__, __LINE__);
                return ret;
            }

            req->req_hdrs.reqContentLength -= rd;
            
            size_t padding = fcgi_set_header(buf, FCGI_STDIN, rd, 1);
            ret = write_to_script(fcgi_sock, buf, rd + 8 + padding, conf->TimeoutCGI);
            if (ret <= 0)
            {
                print_err(req, "<%s:%d> Error: write_to_script()=%d\n", __func__, __LINE__, ret);
                return -RS502;
            }
        }
    }

    // End FCGI_STDIN
    char hd[8];
    fcgi_set_header(hd, FCGI_STDIN, 0, 0);
    int n;
    if ((n = write_to_script(fcgi_sock, hd, 8, conf->TimeoutCGI)) <= 0)
    {
        print_err(req, "<%s:%d> Error: write_to_script()=%n\n", __func__, __LINE__, n);
        return -RS502;
    }

    String hdrs(256);
    if (hdrs.error())
    {
        printf("<%s:%d> Error create String object\n", __func__, __LINE__);
        return -500;
    }
    
    int chunk_mode;
    if (req->reqMethod == M_HEAD)
        chunk_mode = NO_SEND;
    else
        chunk_mode = ((req->httpProt == HTTP11) && req->connKeepAlive) ? SEND_CHUNK : SEND_NO_CHUNK;

    ClChunked chunk(req, chunk_mode);
    
    if (chunk_mode == SEND_CHUNK)
        hdrs << "Transfer-Encoding: chunked\r\n";
    
    const int len_buf = 1024;
    char buf[len_buf + 1];
    char padd[256];
    int empty_line = 0;
    req->resp.respStatus = RS200;
    fcgi_header header = {0, 0, 0};
    
    while (1)
    {
        if (header.len == 0)
        {
            int n = fcgi_read_header(fcgi_sock, &header);
            if (n <= 0)
                return -1;
        }
        
        if (header.type == FCGI_STDOUT)
        {
            if (header.len == 0)
                break;
            int rd = (header.len <= len_buf) ? header.len : len_buf;
            int n = read_timeout(fcgi_sock, buf, rd, conf->TimeoutCGI);
            if (n <= 0)
            {
                printf("! Error: fcgi_read FCGI_STDOUT\n");
                return -1;
            }
            
            header.len -= n;
            buf[n] = 0;
            if (!empty_line)
            {
                int tail = fcgi_read_headers(buf, n, &hdrs, &req->resp.respStatus);
                if (tail >= 0)
                {
                    if (chunk_mode)
                    {
                        if (req->resp.respStatus >= RS400)
                            send_message(req, NULL, NULL);
                        else if (send_response_headers(req, &hdrs))
                            return -1;
        
                        if (req->resp.respStatus == RS204)
                        {
                            return 0;
                        }
                    }
                    
                    chunk.add_arr(buf + tail, n - tail);
                }
                else
                    return -1;
                empty_line = 1;
            }
            else
                chunk.add_arr(buf, n);
        }
        else if (header.type == FCGI_END_REQUEST)
        {
            if (header.len > 8)
                return -1;
            int n = read_timeout(fcgi_sock, padd, header.len, conf->TimeoutCGI);
            if (n > 0)
                header.len -= n;
            break;
        }
        else if (header.type == FCGI_STDERR)
        {
            int ret = fcgi_read_stderr(fcgi_sock, header.len, conf->TimeoutCGI);
            if (ret <= 0)
            {
                print_err(req, "<%s:%d> Error fcgi_read_stderr()\n", __func__, __LINE__);
                return -1;
            }
    
            header.len -= ret;
        }
        else
        {
            printf("! Error: type=%d\n", header.type);
            return -1;
        }
        
        if ((header.paddingLen > 0) && (header.len == 0))
        {
            if (read_timeout(fcgi_sock, padd, header.paddingLen, conf->TimeoutCGI) <= 0)
                return -1;
        }
    }
    
    int ret = chunk.end();
    req->resp.respContentLength = chunk.all();
    if (ret < 0)
    {
        print_err(req, "<%s:%d> Error chunk.end(): %d\n", __func__, __LINE__, ret);
        return -1;
    }
    
    if (chunk_mode == NO_SEND)
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
int fcgi_send_param(Connect *req, int fcgi_sock)
{
//print_err("<%s:%d> -------------------------\n", __func__, __LINE__);
    FCGI_class Fcgi(fcgi_sock);
    if (req->resp.scriptType == php_fpm)
        Fcgi.add("REDIRECT_STATUS", "true");
    Fcgi.add("PATH", "/bin:/usr/bin:/usr/local/bin");
    Fcgi.add("SERVER_SOFTWARE", conf->ServerSoftware.str());
    Fcgi.add("GATEWAY_INTERFACE", "CGI/1.1"); 
    Fcgi.add("DOCUMENT_ROOT", conf->rootDir.str());
    Fcgi.add("REMOTE_ADDR", req->remoteAddr);
    Fcgi.add("REMOTE_PORT", req->remotePort);
    Fcgi.add("REQUEST_URI", req->uri);

    if (req->reqMethod == M_HEAD)
        Fcgi.add("REQUEST_METHOD", get_str_method(M_GET));
    else
        Fcgi.add("REQUEST_METHOD", get_str_method(req->reqMethod));
    
    Fcgi.add("SERVER_PROTOCOL", get_str_http_prot(req->httpProt));
    
    if(req->req_hdrs.iHost >= 0)
        Fcgi.add("HTTP_HOST", req->req_hdrs.Value[req->req_hdrs.iHost]);
    
    if(req->req_hdrs.iReferer >= 0)
        Fcgi.add("HTTP_REFERER", req->req_hdrs.Value[req->req_hdrs.iReferer]);
    
    if(req->req_hdrs.iUserAgent >= 0)
        Fcgi.add("HTTP_USER_AGENT", req->req_hdrs.Value[req->req_hdrs.iUserAgent]);

    Fcgi.add("SCRIPT_NAME", req->decodeUri);
    
    if (req->resp.scriptType == php_fpm)
    {
        String s = conf->rootDir;
        s << req->resp.scriptName;
        Fcgi.add("SCRIPT_FILENAME", s.str());
    }

    if(req->reqMethod == M_POST)
    {
        if(req->req_hdrs.iReqContentType >= 0)
        {
            Fcgi.add("CONTENT_TYPE", req->req_hdrs.Value[req->req_hdrs.iReqContentType]);
        }
        
        if(req->req_hdrs.iReqContentLength >= 0)
        {
            Fcgi.add("CONTENT_LENGTH", req->req_hdrs.Value[req->req_hdrs.iReqContentLength]);
        }
    }
    
    Fcgi.add("QUERY_STRING", req->sReqParam);
    
    Fcgi.add(NULL, 0); // End FCGI_PARAMS
    if (Fcgi.error())
    {
        print_err(req, "<%s:%d> Error send_param()\n", __func__, __LINE__);
        return -RS500;
    }
    
    int ret = fcgi_(req, fcgi_sock);
    
    return ret;
}
//======================================================================
int fcgi(Connect *req)
{
    int  sock_fcgi, ret = 0;

    if (req->reqMethod == M_POST)
    {
        if (req->req_hdrs.iReqContentType < 0)
        {
            print_err(req, "<%s:%d> Content-Type \?\n", __func__, __LINE__);
            return -RS400;
        }

        if (req->req_hdrs.reqContentLength < 0)
        {
            print_err(req, "<%s:%d> 411 Length Required\n", __func__, __LINE__);
            return -RS411;
        }

        if (req->req_hdrs.reqContentLength > conf->ClientMaxBodySize)
        {
            print_err(req, "<%s:%d> 413 Request entity too large: %lld\n", __func__, __LINE__, req->req_hdrs.reqContentLength);
            if (req->req_hdrs.reqContentLength < 50000000)
            {
                if (req->tail)
                    req->req_hdrs.reqContentLength -= req->lenTail;
                client_to_cosmos(req, &req->req_hdrs.reqContentLength);
                if (req->req_hdrs.reqContentLength == 0)
                    return -RS413;
            }
            return -1;
        }
    }
    
    if (timedwait_close_cgi(req->numChld, conf->MaxChldsCgi))
    {
        return -1;
    }
    
    if (req->resp.scriptType == php_fpm)
    {
        sock_fcgi = create_fcgi_socket(conf->PathPHP.str());
    }
    else if (req->resp.scriptType == fast_cgi)
    {
        sock_fcgi = get_sock_fcgi(req, req->resp.scriptName);
    }
    else
    {
        print_err(req, "<%s:%d> req->scriptType ?\n", __func__, __LINE__);
        ret = -RS500;
        goto err_exit;
    }
    
    if (sock_fcgi <= 0)
    {
        print_err(req, "<%s:%d> Error connect to fcgi\n", __func__, __LINE__);
        if (sock_fcgi == 0)
            ret = -RS400;
        else
            ret = -RS502;
        goto err_exit;
    }
    
    ret = fcgi_send_param(req, sock_fcgi);
    
    close(sock_fcgi);
    
err_exit:
    cgi_dec();
    if (ret < 0)
        req->connKeepAlive = 0;
    return ret;
}
