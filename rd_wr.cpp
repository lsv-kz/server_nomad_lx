#include "main.h"

using namespace std;

//    #define POLLIN      0x0001    /* Можно считывать данные */
//    #define POLLPRI     0x0002    /* Есть срочные данные */
//    #define POLLOUT     0x0004    /* Запись не будет блокирована */
//    #define POLLERR     0x0008    /* Произошла ошибка */
//    #define POLLHUP     0x0010    /* "Положили трубку" */
//    #define POLLNVAL    0x0020    /* Неверный запрос: fd не открыт */
// 0x1c = 0001 1100 = x10 | x08 | x04
//======================================================================
int wait_read(int fd, int timeout)
{
    struct pollfd fdrd;
    int ret, tm;
    
    if (timeout == -1)
        tm = -1;
    else
        tm = timeout * 1000;

    fdrd.fd = fd;
    fdrd.events = POLLIN;
retry:
    ret = poll(&fdrd, 1, tm);
    if (ret == -1)
    {
        print_err("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
        if (errno == EINTR)
            goto retry;
        return -1;
    }
    else if (!ret)
    {
    //  print_err("<%s:%d> TimeOut read()\n", __func__, __LINE__);
        return -RS408;
    }

    if (fdrd.revents & POLLIN)
        return 1;
    else if (fdrd.revents & POLLHUP)
    {
//      print_err("<%s:%d>***** POLLHUP *****0x%02x\n", __func__, __LINE__, fdrd.revents);
        return 0;
    }
    else if (fdrd.revents & POLLERR)
    {
        print_err("<%s:%d> POLLERR fdrd.revents = 0x%02x\n", __func__, __LINE__, fdrd.revents);
        return -1;
    }

    print_err("<%s:%d> Error fdrd.revents = 0x%02x\n", __func__, __LINE__, fdrd.revents);

    return -1;
}
//======================================================================
int read_timeout(int fd, char *buf, int len, int timeout)
{
    int read_bytes = 0, ret, tm;
    struct pollfd fdrd;
    char *p;
    
    tm = (timeout == -1) ? -1 : (timeout * 1000);

    fdrd.fd = fd;
    fdrd.events = POLLIN;
    p = buf;
    while (len > 0)
    {
        ret = poll(&fdrd, 1, tm);
        if (ret == -1)
        {
            print_err("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (!ret)
            return -RS408;

        if (fdrd.revents & POLLIN)
        {
            ret = read(fd, p, len);
            if (ret == -1)
            {
                print_err("<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
                return -1;
            }
            else if (ret == 0)
            {
 //   print_err("<%s:%d> read()=0\n", __func__, __LINE__);
                break;
            }
            else
            {
                p += ret;
                len -= ret;
                read_bytes += ret;
            }
        }
        else if (fdrd.revents & POLLHUP)
        {
 //   print_err("<%s:%d>***** POLLHUP *****0x%02x\n", __func__, __LINE__, fdrd.revents);
            break;
        }
        else if (fdrd.revents & POLLERR)
        {
            print_err("<%s:%d> POLLERR fdrd.revents = 0x%02x\n", __func__, __LINE__, fdrd.revents);
            return -1;
        }
    }

    return read_bytes;
}
//======================================================================
int write_to_client(Connect *req, const char *buf, int len, int timeout)
{
    int write_bytes = 0, ret;
    struct pollfd fdwr;
    
    fdwr.fd = req->clientSocket;
    fdwr.events = POLLOUT;

    while (len > 0)
    {
        ret = poll(&fdwr, 1, timeout * 1000);
        if (ret == -1)
        {
            print_err(req, "<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (!ret)
        {
            print_err(req, "<%s:%d> TimeOut poll(), tm=%d\n", __func__, __LINE__, timeout);
            return -1;
        }
        
        if (fdwr.revents != POLLOUT)
        {
            print_err(req, "<%s:%d> 0x%02x\n", __func__, __LINE__, fdwr.revents);
            return -1;
        }
        
        ret = write(req->clientSocket, buf, len);
        if (ret == -1)
        {
            print_err(req, "<%s:%d> Error write(): %s\n", __func__, __LINE__, strerror(errno));
            if ((errno == EINTR) || (errno == EAGAIN))
                continue;
            return -1;
        }

        write_bytes += ret;
        len -= ret;
        buf += ret;
    }

    return write_bytes;
}
//======================================================================
int write_to_script(int fd, const char *buf, int len, int timeout)
{
    int write_bytes = 0, ret;
    struct pollfd fdwr;
    
    fdwr.fd = fd;
    fdwr.events = POLLOUT;

    while (len > 0)
    {
        ret = poll(&fdwr, 1, timeout * 1000);
        if (ret == -1)
        {
            print_err("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (!ret)
        {
            print_err("<%s:%d> TimeOut poll(), tm=%d\n", __func__, __LINE__, timeout);
            return -1;
        }
        
        if (fdwr.revents != POLLOUT)
        {
            print_err("<%s:%d> 0x%02x\n", __func__, __LINE__, fdwr.revents);
            return -1;
        }
        
        ret = write(fd, buf, len);
        if (ret == -1)
        {
            print_err("<%s:%d> Error write(): %s\n", __func__, __LINE__, strerror(errno));
            if ((errno == EINTR) || (errno == EAGAIN))
                continue;
            return -1;
        }

        write_bytes += ret;
        len -= ret;
        buf += ret;
    }

    return write_bytes;
}
//======================================================================
int client_to_script(Connect *req, int fd_out, long long *cont_len)
{
    int wr_bytes = 0;
    int rd, wr, ret;
    char buf[512];

    for ( ; *cont_len > 0; )
    {
        rd = (*cont_len > (int)sizeof(buf)) ? (int)sizeof(buf) : *cont_len;
        
        ret = read_timeout(req->clientSocket, buf, rd, conf->TimeOut);
        if (ret == -1)
        {
            return -1;
        }
        else if (ret == 0)
            break;

        *cont_len -= ret;
buf[rd]=0;
//fprintf(stderr, "%s", buf);
        wr = write_to_script(fd_out, buf, ret, conf->TimeoutCGI);
        if (wr <= 0)
            return wr;
        wr_bytes += wr;
    }
//fprintf(stderr, "\n\n");
    return wr_bytes;
}
//======================================================================
void client_to_cosmos(Connect *req, long long *size)
{
    int rd;
    char buf[1024];

    for (; *size > 0; )
    {
        rd = read_timeout(req->clientSocket, buf, (*size > (int)sizeof(buf)) ? (int)sizeof(buf) : *size, conf->TimeOut);
        if (rd == -1)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        else if (rd == 0)
            break;

        *size -= rd;
    }
}
/*====================================================================*/
long cgi_to_cosmos(int fd_in, int timeout)
{
    long wr_bytes = 0;
    long rd;
    char buf[1024];

    for (; ; )
    {
        rd = read_timeout(fd_in, buf, sizeof(buf), timeout);
        if(rd == -1)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if(rd == 0)
            break;
        wr_bytes += rd;
    }

    return wr_bytes;
}
/*====================================================================*/
long fcgi_to_cosmos(int fd_in, unsigned int size, int timeout)
{
    long wr_bytes = 0;
    long rd;
    char buf[1024];

    for (; size > 0; )
    {
        rd = read_timeout(fd_in, buf, (size > sizeof(buf)) ? sizeof(buf) : size, timeout);
        if(rd == -1)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if(rd == 0)
            break;
        
        size -= rd;
        wr_bytes += rd;
    }

    return wr_bytes;
}
//======================================================================
int fcgi_read_padding(int fd_in, unsigned char len, int timeout)
{
    int rd;
    const unsigned int size = 256;
    char buf[size];

    for (; len > 0; )
    {
        rd = read_timeout(fd_in, buf, (len > size) ? size : len, timeout);
        if (rd == -1)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (rd == 0)
        {
            return 0;
        }

        len -= rd;
    }

    return 1;
}
//======================================================================
int script_to_file(int fd_in, int fd_out, int cont_len, int timeout)
{
    int all_wr_bytes = 0;
    int rd, wr;
    char buf[512];

    for ( ; cont_len > 0; )
    {
        rd = read_timeout(fd_in, buf, cont_len > (int)sizeof(buf) ? (int)sizeof(buf) : cont_len, timeout);
        if (rd == -1)
        {
            if (errno == EINTR)
                continue;
            print_err("<%s:%d> Error read_timeout()\n", __func__, __LINE__);
            return -1;
        }
        else if (rd == 0)
        {
        //  print_err("<%s:%d> rd=0; all_wr_bytes=%d\n", __func__, __LINE__, all_wr_bytes);
            break;
        }
        
        cont_len -= rd;

        wr = write(fd_out, buf, rd);
        if (wr != rd)
        {
            return -1;
        }
        
        all_wr_bytes += rd;
    }

    return all_wr_bytes;
}
//======================================================================
int fcgi_read_stderr(int fd_in, int cont_len, int timeout)
{
    int wr_bytes = 0;
    int rd;
    char buf[512];

    for ( ; cont_len > 0; )
    {
        rd = read_timeout(fd_in, buf, cont_len > (int)sizeof(buf) ? (int)sizeof(buf) : cont_len, conf->TimeOut);
        if (rd == -1)
        {
            print_err("<%s:%d> Error read_timeout()=%d\n", __func__, __LINE__, rd);
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (rd == 0)
            break;

        cont_len -= rd;

        write(STDERR_FILENO, buf, rd);
        wr_bytes += rd;
    }
write(STDERR_FILENO, "\n", 1);
    return wr_bytes;
}
//======================================================================
int send_largefile(Connect *req, char *buf, int size, off_t offset, long long *cont_len)
{
    int rd, wr;
    
    lseek(req->resp.fd, offset, SEEK_SET);

    for ( ; *cont_len > 0; )
    {
        if (*cont_len < size)
            rd = read(req->resp.fd, buf, *cont_len);
        else
            rd = read(req->resp.fd, buf, size);
        
        if (rd == -1)
        {
            print_err(req, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (rd == 0)
            break;

        wr = write_to_client(req, buf, rd, conf->TimeOut);
        if (wr != rd)
        {
            print_err(req, "<%s:%d> Error write_to_sock()=%d, %d\n", __func__, __LINE__, wr, rd);
            return -1;
        }
        
        *cont_len -= wr;
    }

    return 0;
}
//======================================================================
int check_req(Connect *req, char *s, char **p_newline, int *len, int *start)
{
    char *pr, *pn;
    
    while (*len > 0)
    {
        pr = (char*)memchr(*p_newline, '\r', *len - 1);
        pn = (char*)memchr(*p_newline, '\n', *len);
        if (pr && pn)
        {
            if ((pn - pr) != 1)
                return -RS400;
            
            if ((pn - *p_newline) == 1)
            {
                *len -= (pn+1 - *p_newline);
                if (*len > 0)
                {
                    req->tail = pn + 1;
                    req->lenTail = *len;
                }
                else
                    req->tail = NULL;
                
                return 1;
            }
            
            int n = pn - *p_newline + 1;
            if (!(*start))
            {
//hex_dump_stderr(req->resp.sLogTime.c_str(), req->numConn, *p_newline, n);
                int ret = parse_startline_request(req, *p_newline, n);
                if (ret)
                {
                    print_err(req, "<%s:%d>  Error parse_startline_request(): %d\n", __func__, __LINE__, ret);
                    return ret;
                }
                *start = 1;
            }
            else
            {
                int ret = parse_headers(req, *p_newline, n);
                if (ret < 0)
                {
                    print_err(req, "<%s:%d>  Error parse_headers(): %d\n", __func__, __LINE__, ret);
                    return ret;
                }
            }
            
            *len -= (pn+1 - *p_newline);
            *p_newline = pn + 1;
        }
        else if (pr && (!pn))
        {
            return -RS400;
        }
        else if ((!pr) && pn)
        {
            return -RS400;
        }
        else
        {
            break;
        }
    }

    return 0;
}
//======================================================================
int read_headers(Connect *req, int timeout1, int timeout2)
{
    int all_rd = 0, ret = -1;
    int startline = 0, len_tail = 0;
    int len_buf = sizeof(req->bufReq) - 1;
    char *p, *p_newline;
    struct pollfd fdrd;
    int timeout = timeout1;
    
    get_time(req->resp.sLogTime);
    
    p = p_newline = req->bufReq;
    
    fdrd.fd = req->clientSocket;
    fdrd.events = POLLIN;
    while (1)
    {
        ret = poll(&fdrd, 1, timeout * 1000);
        if (ret == -1)
        {
            print_err(req, "<%s:%d> Error poll()=-1: %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            break;
        }
        else if (!ret)
        {
            print_err(req, "<%s:%d> Timeout=%d\n", __func__, __LINE__, timeout);
            return -1000;
        }
        timeout = timeout2;
        
        if (!(fdrd.revents & POLLIN))
        {
            print_err(req, "<%s:%d> fdrd.revents != POLLIN: 0x%02x\n", __func__, __LINE__, fdrd.revents);
            return -1;
        }

        ret = recv(req->clientSocket, p, len_buf, 0);
        if (ret < 0)
        {
            if (errno == EAGAIN)
                continue;
            print_err(req, "<%s:%d> Error recv() = %d: %s\n", __func__, __LINE__, ret, strerror(errno));
            return -1;
        }
        else if (ret == 0)
        {
    //        print_err(req, "<%s:%d> Error recv()=0; %d; %s\n", __func__, __LINE__, len_buf, req->bufReq);
            return ret;
        }

        all_rd += ret;
        *(p + ret) = 0;
//hex_dump_stderr(__func__, __LINE__, req->bufReq, all_rd);
        len_tail += ret;
        int n = check_req(req, p, &p_newline, &len_tail, &startline);
        if (n == 1)
        {
            return all_rd;
        }
        else if (n < 0)
        {
            return n;
        }
        
        p += ret;
        len_buf -= ret;
        if (len_buf <= 0)
        {
            print_err(req, "<%s:%d> Error len_buf=0\n", __func__, __LINE__);
            return -RS414;
        }
    }

    return ret;
}
