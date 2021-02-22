#include "classes.h"

using namespace std;

#define FCGI_RESPONDER  1
//#define FCGI_AUTHORIZER 2
//#define FCGI_FILTER     3

//#define FCGI_KEEP_CONN  1

#define FCGI_VERSION_1           1
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

typedef struct {
    unsigned char type;
    int len;
    int paddingLen;
} fcgi_header;

const int requestId = 1;
//======================================================================
const int FCGI_SIZE_PAR_BUF = 4088;
const int FCGI_SIZE_HEADER = 8;

class FCGI_params
{
    char buf[FCGI_SIZE_HEADER + FCGI_SIZE_PAR_BUF];
    int lenBuf = 0;
    int err = 0;
    int numPar = 0;
    int fcgi_sock;
    char *ptrPar = buf + FCGI_SIZE_HEADER;
    
    void send_par(int end)
    {
        if (err) return;
        unsigned char padding = 8 - (lenBuf % 8);
        padding = (padding == 8) ? 0 : padding;
        char *p = buf;
        *p++ = FCGI_VERSION_1;
        *p++ = FCGI_PARAMS;
        *p++ = (unsigned char) ((1 >> 8) & 0xff);
        *p++ = (unsigned char) ((1) & 0xff);
    
        *p++ = (unsigned char) ((lenBuf >> 8) & 0xff);
        *p++ = (unsigned char) ((lenBuf) & 0xff);
    
        *p++ = padding;
        *p = 0;

        memset(ptrPar + lenBuf, 0, padding);
        lenBuf += padding;
        
        if (end && ((lenBuf + 8) <= FCGI_SIZE_PAR_BUF))
        {
            char s[8] = {1, 4, 0, 1, 0, 0, 0, 0};
            memcpy(ptrPar + lenBuf, s, 8);
            lenBuf += 8;
            end = 0;
        }
 
        int n = write_to_script(fcgi_sock, buf, 8 + lenBuf, conf->TimeoutCGI);
        if (n == -1)
        {
            err = 1;
            return;
        }
    
        lenBuf = 0;
        if (end)
            send_par(0);
    }
    
    FCGI_params() {}
public:
    FCGI_params(int s) { fcgi_sock = s; }
    
    void add(const char *name, const char *val)
    {
        if (err) return;
        ++numPar;
        if (!name)
        {
            send_par(1);
            return;
        }

        size_t len_name = strlen(name), len_val, len;
        
        if (val)
            len_val = strlen(val);
        else
            len_val = 0;
        
        len = len_name + len_val;
        len += len_name > 127 ? 4 : 1;
        len += len_val > 127 ? 4 : 1;
        
        if ((lenBuf + len + 8) > FCGI_SIZE_PAR_BUF)
        {
            send_par(0);
            if (err)
                return;
            if ((len + 8) > FCGI_SIZE_PAR_BUF)
            {
                cerr << "[" << name << "  " << val << "] len=" << len <<  "\n";
                err = 1;
                return;
            }
        }
        
        char *p = ptrPar + lenBuf;
        if (len_name < 0x80)
            *p++ = (unsigned char)len_name;
        else
        {
            *p++ = (unsigned char)((len_name >> 24) | 0x80);
            *p++ = (unsigned char)(len_name >> 16);
            *p++ = (unsigned char)(len_name >> 8);
            *p++ = (unsigned char)len_name;
        }
    
        if (len_val < 0x80)
            *p++ = (unsigned char)len_val;
        else
        {
            *p++ = (unsigned char)((len_val >> 24) | 0x80);
            *p++ = (unsigned char)(len_val >> 16);
            *p++ = (unsigned char)(len_val >> 8);
            *p++ = (unsigned char)len_val;
        }
        
        memcpy(p, name, len_name);
        if (len_val > 0)
            memcpy(p + len_name, val, len_val);
        
        lenBuf += len;
    }
    
    int error() { return err; }
};
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
void fcgi_set_header(char *header, int type, int id, int len, int padding_len)
{
    char *p = header;
    *p++ = FCGI_VERSION_1;                      // Protocol Version
    *p++ = type;                                // PDU Type
    *p++ = (unsigned char) ((id >> 8) & 0xff);  // Request Id
    *p++ = (unsigned char) ((id) & 0xff);       // Request Id
    
    *p++ = (unsigned char) ((len >> 8) & 0xff); // Content Length
    *p++ = (unsigned char) ((len) & 0xff);      // Content Length
    
    *p++ = padding_len;                         // Padding Length
    *p = 0;                                     // Reserved
}
//======================================================================
int tail_to_fcgi(int fcgi_sock, char *tail, int lenTail)
{
    int rd, wr, all_wr = 0;
    const int size_buf = 8192;
    char buf[16 + size_buf], *p = tail;

    while(lenTail > 0)
    {
        if (lenTail > size_buf)
            rd = size_buf;
        else
            rd = lenTail;
        memcpy(buf + 8, p, rd);
        
        size_t padding = 8 - (rd % 8);
        padding = (padding == 8) ? 0 : padding;
        fcgi_set_header(buf, FCGI_STDIN, requestId, rd, padding);
//hex_dump_stderr(__func__, __LINE__, buf, rd + 8 + padding);
        wr = write_to_script(fcgi_sock, buf, rd + 8 + padding, conf->TimeoutCGI);
        if ((wr == -1) || ((rd + 8 + (int)padding) != wr))
        {
            return -1;
        }
        lenTail -= rd;
        all_wr += rd;
        p += rd;
    }
    return all_wr;
}
//======================================================================
int client_to_fcgi(int sock_cl, int fcgi_sock, int contentLength)
{
    int rd, wr;
    const int size_buf = 8192;
    char buf[16 + size_buf];
    
    while(contentLength > 0)
    {
        if (contentLength > size_buf)
            rd = read_timeout(sock_cl, buf + 8, size_buf, conf->TimeOut);
        else
            rd = read_timeout(sock_cl, buf + 8, contentLength, conf->TimeOut);
        if (rd <= 0)
        {
            return -1;
        }

        size_t padding = 8 - (rd % 8);
        padding = (padding == 8) ? 0 : padding;
        fcgi_set_header(buf, FCGI_STDIN, requestId, rd, padding);
//hex_dump_stderr(__func__, __LINE__, buf, rd + 8 + padding);
        wr = write_to_script(fcgi_sock, buf, rd + 8 + padding, conf->TimeoutCGI);
        if (wr == -1)
        {
            return -1;
        }
        contentLength -= rd;
    }
    return 0;
}
//======================================================================
int fcgi_get_header(int fcgi_sock, fcgi_header *header)
{
    int n;
    char buf[8];
    
    n = read_timeout(fcgi_sock, buf, 8, conf->TimeoutCGI);
    if (n <= 0)
        return n;
//hex_dump_stderr(__func__, __LINE__, buf, 8);
    header->type = (unsigned char)buf[1];
    header->paddingLen = (unsigned char)buf[6];
    header->len = ((unsigned char)buf[4]<<8) | (unsigned char)buf[5];
    
    return n;
}
//======================================================================
int fcgi_chunk(Connect *req, String *hdrs, int fcgi_sock, fcgi_header *header, char *tail_ptr, int tail_len)
{
    int ret;
    int chunk_mode;
    if (req->reqMethod == M_HEAD)
        chunk_mode = NO_SEND;
    else
        chunk_mode = ((req->httpProt == HTTP11) && req->connKeepAlive) ? SEND_CHUNK : SEND_NO_CHUNK;

    ClChunked chunk(req, chunk_mode);
//print_err(req, "<%s:%d> -------------\n", __func__, __LINE__);
    req->resp.numPart = 0;

    if (chunk_mode == SEND_CHUNK)
    {
        (*hdrs) << "Transfer-Encoding: chunked\r\n";
    }

    if (chunk_mode)
    {
        if (send_response_headers(req, hdrs))
        {
            return -1;
        }
        
        if (req->resp.respStatus == RS204)
        {
            return 0;
        }
    }
    //------------------- send entity after headers --------------------
    if ((tail_len > 0) && tail_ptr)
    {
        ret = chunk.add_arr(tail_ptr, tail_len);
        if (ret < 0)
        {
            print_err(req, "<%s:%d> Error chunk_buf.add_arr(): %d\n", __func__, __LINE__, ret);
            return -RS500;
        }
    }

    if (header->len > 0)
    {
        ret = chunk.fcgi_to_client(fcgi_sock, header->len);
        if (ret < 0)
        {
            print_err(req, "<%s:%d> Error chunk.fcgi_to_client()=%d\n", __func__, __LINE__, ret);
            return -1;
        }
    }

    if (header->paddingLen > 0)
    {
        ret = fcgi_read_padding(fcgi_sock, header->paddingLen, conf->TimeoutCGI);
        if (ret <= 0)
        {
            print_err(req, "<%s:%d> Error fcgi_read_padding()=%d\n", __func__, __LINE__, ret);
            return -1;
        }
    }
    //------------------- send entity other parts ----------------------
    while(1)
    {
        ret = fcgi_get_header(fcgi_sock, header);
        if (ret <= 0)
        {
            print_err(req, "<%s:%d> fcgi_get_header()=%d\n", __func__, __LINE__, ret);
            return -RS500;
        }

        if (header->type == FCGI_END_REQUEST)
        {
            char buf[256];
            ret = read_timeout(fcgi_sock, buf, header->len, conf->TimeoutCGI);
            if (ret <= 0)
            {
                print_err(req, "<%s:%d> read_timeout()\n", __func__, __LINE__);
                return -1;
            }
  //  hex_dump_stderr("fcgi_chunk: FCGI_END_REQUEST", __LINE__, buf, header->len);
            break;
        }
        else if (header->type == FCGI_STDERR)
        {
            ret = fcgi_read_stderr(fcgi_sock, header->len, conf->TimeoutCGI);
            if (ret <= 0)
            {
                print_err(req, "<%s:%d> Error fcgi_read_stderr()\n", __func__, __LINE__);
                return -RS500;
            }
    
            header->len -= ret;
        }
        else if (header->type == FCGI_STDOUT)
        {
//print_err("<%s:%d>  header.len=%d\n", __func__, __LINE__, header->len);
            ret = chunk.fcgi_to_client(fcgi_sock, header->len);
            if (ret < 0)
            {
                print_err(req, "<%s:%d> Error chunk.fcgi_to_client()=%d\n", __func__, __LINE__, ret);
                return -1;
            }
        }
        else
        {
            print_err(req, "<%s:%d> Error fcgi: type=%hhu\n", __func__, __LINE__, header->type);
            return -1;
        }
        
        if (header->paddingLen > 0)
        {
            ret = fcgi_read_padding(fcgi_sock, header->paddingLen, conf->TimeoutCGI);
            if (ret <= 0)
            {
                print_err(req, "<%s:%d> Error fcgi_read_padding()=%d\n", __func__, __LINE__, ret);
                return -1;
            }
        }
    }
    //------------------------------------------------------------------
    ret = chunk.end();
    req->resp.respContentLength = chunk.all();
    if (ret < 0)
    {
        print_err(req, "<%s:%d> Error chunk.end(): %d\n", __func__, __LINE__, ret);
    }
    
    if (chunk_mode == NO_SEND)
    {
//print_err("<%s:%d> chunk.all() = %d\n", __func__, __LINE__, chunk.all());
        if (send_response_headers(req, hdrs))
        {
            print_err("<%s:%d> Error send_header_response()\n", __func__, __LINE__);
            return -1;
        }
    }
    else
        req->resp.send_bytes = req->resp.respContentLength;
    /*----------------------- end response -----------------------*/
    return 0;
}
//======================================================================
int read_from_script(int cgi_serv_in, char *buf, int len);
//======================================================================
int fcgi_read_headers(Connect *req, int fcgi_sock)
{
    int n;
    fcgi_header header;
//print_err(req, "<%s:%d> -------------\n", __func__, __LINE__);
    req->resp.respStatus = RS200;
    String hdrs(256);
    if (hdrs.error())
    {
        print_err(req, "<%s:%d> Error create String object\n", __func__, __LINE__);
        return -RS500;
    }
    const char *err_str = NULL;
    while (1)
    {
        n = fcgi_get_header(fcgi_sock, &header);
        if (n <= 0)
        {
print_err(req, "<%s:%d> fcgi_get_header()=%d\n", __func__, __LINE__, n);
            err_str = "Error: fcgi_get_header()";
            break;
        }
        
        if (header.type == FCGI_STDOUT)
            break;
        else if (header.type == FCGI_STDERR)
        {
            n = fcgi_read_stderr(fcgi_sock, header.len, conf->TimeoutCGI);
            if (n <= 0)
            {
                err_str = "Error: fcgi_read_stderr()";
                break;
            }
            
            header.len -= n;
            
            if (header.paddingLen > 0)
            {
                n = fcgi_read_padding(fcgi_sock, header.paddingLen, conf->TimeoutCGI);
                if (n <= 0)
                {
                    err_str = "Error: fcgi_read_padding()";
                    break;
                }
            }
        }
        else
        {
            err_str = "Error header type";
            break;
        }
    }

    if (err_str)
    {
        print_err(req, "<%s:%d> \"%s\"\n", __func__, __LINE__, err_str);
        return -RS500;
    }
    
    if (header.type != FCGI_STDOUT)
    {
        print_err(req, "<%s:%d> Error: %hhu\n", __func__, __LINE__, header.type);
        return -RS500;
    }
    //-------------------------- read headers --------------------------
    const int size = 41;
    char buf[size];
    
    char *start_ptr = buf;
    err_str = "Error: Blank line not found";
    int i = 0;

    while (1)
    {
        int len;
        char *end_ptr, s[64];

        if (i > 0)
            end_ptr = (char*)memchr(start_ptr, '\n', i);
        else
            end_ptr = NULL;
        if(!end_ptr)
        {
            if (header.len == 0)
                break;
                
            if (i > 0)
                memmove(buf, start_ptr, i);
            int rd = size - i;
            if (rd <= 0)
            {
                print_err(req, "<%s:%d> Error: size()=%d, i=%d\n", __func__, __LINE__, size, i);
                err_str = "Error: Buffer for read is small";
                break;
            }
            
            if (header.len < rd)
                rd = header.len;

     //       int ret = read_from_script(fcgi_sock, buf + i, rd);
            int ret = read_timeout(fcgi_sock, buf + i, rd, conf->TimeoutCGI);
            if (ret <= 0)
            {
                print_err(req, "<%s:%d> read_from_script()=%d, read_len=%d\n", __func__, __LINE__, ret, rd);
                err_str = "Error: Read from script";
                break;
            }
//hex_dump_stderr(__func__, __LINE__, buf + i, ret);
            header.len -= ret;
            i = i + ret;
            start_ptr = buf;
            continue;
        }
        
        len = end_ptr - start_ptr;
        i = i - (len + 1);
        
        if (len > 0)
        {
            if (*(end_ptr - 1) == '\r')
                --len;
            memcpy(s, start_ptr, len);
            s[len] = '\0';
        }

        start_ptr = end_ptr + 1;

        if(len == 0)
        {
            err_str = NULL;
            break;
        }
//print_err("<%s:%d> [%s]\n", __func__, __LINE__, s);
        char *p;
        if((p = (char*)memchr(s, ':', len)))
        {
            if(!strlcmp_case(s, "Status", 6))
            {
                req->resp.respStatus = strtol(++p, NULL, 10);
          //      if ((req->resp.respStatus >= RS500) || (req->resp.respStatus == RS404))
                if (req->resp.respStatus >= RS500)
                {
                    send_message(req, NULL, NULL);
                    return 0;
                }

                continue;
            }
            else if(!strlcmp_case(s, "Date", 4) || \
                !strlcmp_case(s, "Server", 6) || \
                !strlcmp_case(s, "Accept-Ranges", 13) || \
                !strlcmp_case(s, "Content-Length", 14) || \
                !strlcmp_case(s, "Connection", 10))
            {
                print_err("<%s:%d> %s\n", __func__, __LINE__, s);
                continue;
            }
            
            hdrs << s << "\r\n";
            if (hdrs.error())
            {
                err_str = "Error: Create header";
                break;
            }
        }
        else
        {
            err_str = "Error: Line not header";
            break;
        }
    }

    if (err_str)
    {
        print_err(req, "<%s:%d> \"%s\" header.len=%d\n", __func__, __LINE__, err_str, header.len);
        return -1;
    }

    return fcgi_chunk(req, &hdrs, fcgi_sock, &header, start_ptr, i);
}
//======================================================================
int send_param(Connect *req, int fcgi_sock)
{
    int n;
    char buf[4096];
    //------------------------- param ----------------------------------
    fcgi_set_header(buf, FCGI_BEGIN_REQUEST, requestId, 8, 0);
    
    buf[8] = (unsigned char) ((FCGI_RESPONDER >> 8) & 0xff);
    buf[9] = (unsigned char) (FCGI_RESPONDER        & 0xff);
    buf[10] = 0;//  FCGI_KEEP_CONN : 0
    buf[11] = 0;
    buf[12] = 0;
    buf[13] = 0;
    buf[14] = 0;
    buf[15] = 0;
    
    n = write_to_script(fcgi_sock, buf, 16, conf->TimeoutCGI);
    if (n == -1)
    {
        print_err(req, "<%s:%d> Error write_timeout(): %s\n", __func__, __LINE__, str_err (errno));
        return -RS500;
    }
//hex_dump_stderr("send_param: FCGI_BEGIN_REQUEST", buf, 16);
    FCGI_params par(fcgi_sock);
    
    if (req->resp.scriptType == php_fpm)
        par.add("REDIRECT_STATUS", "true");
    par.add("PATH", "/bin:/usr/bin:/usr/local/bin");
    par.add("SERVER_SOFTWARE", conf->ServerSoftware.str());
    par.add("GATEWAY_INTERFACE", "CGI/1.1");
    par.add("DOCUMENT_ROOT", conf->rootDir.str());
    par.add("REMOTE_ADDR", req->remoteAddr);
    par.add("REMOTE_PORT", req->remotePort);
    par.add("REQUEST_URI", req->uri);
    
    if (req->reqMethod == M_HEAD)
        par.add("REQUEST_METHOD", get_str_method(M_GET));
    else
        par.add("REQUEST_METHOD", get_str_method(req->reqMethod));
    
    par.add("SERVER_PROTOCOL", get_str_http_prot(req->httpProt));
    
    if(req->req_hdrs.iHost >= 0)
        par.add("HTTP_HOST", req->req_hdrs.Value[req->req_hdrs.iHost]);
    
    if(req->req_hdrs.iReferer >= 0)
        par.add("HTTP_REFERER", req->req_hdrs.Value[req->req_hdrs.iReferer]);
    
    if(req->req_hdrs.iUserAgent >= 0)
        par.add("HTTP_USER_AGENT", req->req_hdrs.Value[req->req_hdrs.iUserAgent]);

    par.add("SCRIPT_NAME", req->decodeUri);
    
    if (req->resp.scriptType == php_fpm)
    {
        String s = conf->rootDir;
        s << req->resp.scriptName;
        par.add("SCRIPT_FILENAME", s.str());
    }

    if(req->reqMethod == M_POST)
    {
        if(req->req_hdrs.iReqContentType >= 0)
        {
            par.add("CONTENT_TYPE", req->req_hdrs.Value[req->req_hdrs.iReqContentType]);
        }
        
        if(req->req_hdrs.iReqContentLength >= 0)
        {
            par.add("CONTENT_LENGTH", req->req_hdrs.Value[req->req_hdrs.iReqContentLength]);
        }
    }
    
    par.add("QUERY_STRING", req->sReqParam);
    par.add(NULL, 0); // End FCGI_PARAMS
    if (par.error())
    {
        print_err(req, "<%s:%d> Error send_param()\n", __func__, __LINE__);
        return -RS500;
    }
    
    if(req->reqMethod == M_POST)
    {
        if (req->tail)
        {
            n = tail_to_fcgi(fcgi_sock, req->tail, req->lenTail);
            if(n < 0)
            {
                print_err(req, "<%s:%d> Error tail to script: %d\n", __func__, __LINE__, n);
                return -RS500;
            }
            req->req_hdrs.reqContentLength -= n;
        }

        n = client_to_fcgi(req->clientSocket, fcgi_sock, req->req_hdrs.reqContentLength);
        if (n == -1)
        {
            print_err(req, "<%s:%d> Error client_to_fcgi()\n", __func__, __LINE__);
            return -RS500;
        }
    }
    
    fcgi_set_header(buf, FCGI_STDIN, requestId, 0, 0); // End FCGI_STDIN
//hex_dump_stderr(__func__, __LINE__, buf, 8);
    n = write_to_script(fcgi_sock, buf, 8, conf->TimeoutCGI);
    if (n < 0)
    {
        print_err(req, "<%s:%d> Error write header [FCGI_STDIN, 0, 0]\n", __func__, __LINE__);
        return -RS500;
    }

    return fcgi_read_headers(req, fcgi_sock);
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
            ret = -RS404;
        goto err_exit;
    }
    
    ret = send_param(req, sock_fcgi);
    
    close(sock_fcgi);
    
err_exit:
    cgi_dec();
    if (ret < 0)
        req->connKeepAlive = 0;
    return ret;
}
