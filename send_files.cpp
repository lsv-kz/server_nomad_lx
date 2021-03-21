#include "main.h"
#include <sys/sendfile.h>

using namespace std;

static Connect *list_start = NULL;
static Connect *list_end = NULL;

static mutex mtx_send;
static condition_variable cond_add;

static int close_thr = 0;
static int size_list = 0;

struct pollfd *fdwr;
//======================================================================
int send_part_file(Connect *req, char *buf, int size_buf)
{
    int rd, wr, len;
    errno = 0;
    
    if (req->resp.respContentLength >= size_buf)
        len = size_buf;
    else // (int) (long long)
        len = req->resp.respContentLength;
        
    if (len == 0)
        return 0;
    
    if (conf->SEND_FILE == 'y')
    {
        wr = sendfile(req->clientSocket, req->resp.fd, &req->resp.offset, len);
        if (wr == -1)
        {
            if (errno == EAGAIN)
                return -EAGAIN;
            print_err(req, "<%s:%d> Error sendfile(); %s\n", __func__, __LINE__, strerror(errno));
            return wr;
        }
    }
    else
    {
        rd = read(req->resp.fd, buf, len);
        if (rd <= 0)
        {
            if (rd == -1)
                print_err(req, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
            return rd;
        }

        wr = write(req->clientSocket, buf, rd);
        if (wr == -1)
        {
            if (errno == EAGAIN)
                return -EAGAIN;
            print_err(req, "<%s:%d> Error write(); %s\n", __func__, __LINE__, strerror(errno));
            return wr;
        }
        else if (rd != wr)
        {
            lseek(req->resp.fd, wr - rd, SEEK_CUR);
        }
    }
    
    req->resp.send_bytes += wr;
    req->resp.respContentLength -= wr;
    if (req->resp.respContentLength == 0)
        wr = 0;

    return wr;
}
//======================================================================
void del_from_list(Connect *r, RequestManager *ReqMan, int i)
{
mtx_send.lock();
    close(r->resp.fd);
    if (r->prev && r->next)
    {
        r->prev->next = r->next;
        r->next->prev = r->prev;
    }
    else if (r->prev && !r->next)
    {
        r->prev->next = r->next;
        list_end = r->prev;
    }
    else if (!r->prev && r->next)
    {
        r->next->prev = r->prev;
        list_start = r->next;
    }
    else if (!r->prev && !r->next)
    {
        list_start = list_end = NULL;
    }
/*
    if (r->clientSocket != fdwr[i].fd)
    {
        print_err(r, "<%s:%d> clSock = %d, .fd = %d\n", __func__, __LINE__, 
                    r->clientSocket, fdwr[i].fd);
        exit(8);
    }
*/
    for (; (i + 1) < size_list; ++i)
    {
        fdwr[i] = fdwr[i + 1];
    }
    --size_list;
mtx_send.unlock();
    ReqMan->end_response(r);
}
//======================================================================
void send_files(RequestManager *ReqMan)
{
    int count_resp = 0;
    int ret = 0;
    int timeout = 100;
    int size_buf = conf->WR_BUFSIZE;
    int num_chld = ReqMan->get_num_chld();
    
    char *rd_buf = new (nothrow) char [size_buf];
    fdwr = new(nothrow) struct pollfd [conf->MAX_REQUESTS];
    if (!rd_buf || !fdwr)
    {
        print_err("[%u]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
        exit(1);
    }

    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_send);
            while ((list_start == NULL) && (!close_thr))
            {
                cond_add.wait(lk);
            }
            count_resp = size_list;
            if (close_thr)
                break;
        }

        ret = poll(fdwr, count_resp, timeout);
        if (ret == -1)
        {
            print_err("[%u] <%s:%d> Error poll(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
            break;
        }

        if (count_resp < 50) size_buf = conf->WR_BUFSIZE;
        Connect *r = list_start, *next;
        time_t t = time(NULL);
        for (int i = 0; (i < count_resp) && r; r = next)
        {
            next = r->next;
            if (fdwr[i].revents == POLLOUT)
            {
                int wr = send_part_file(r, rd_buf, size_buf);
                if (wr == 0)
                {
                    r->err = 0;
                    del_from_list(r, ReqMan, i);
                    count_resp--;
                }
                else if (wr == -1)
                {
            //        r->err = -1;
                    r->connKeepAlive = 0;
                    r->req_hdrs.iReferer = NUM_HEADERS - 1;
                    r->req_hdrs.Value[r->req_hdrs.iReferer] = "Connection reset by peer";
                    del_from_list(r, ReqMan, i);
                    count_resp--;
                }
                else if (wr > 0) 
                {
                    r->sock_timeout = t;
                    ++i;
                }
                else if (wr == -EAGAIN)
                {
                    if (size_buf > 8192) size_buf = size_buf/2;
                    r->sock_timeout = t;
                    ++i;
                }
            }
            else if (fdwr[i].revents != 0)
            {
                print_err(r, "<%s:%d> revents=0x%x\n", __func__, __LINE__, fdwr[i].revents);
         //       r->err = -1;
                r->connKeepAlive = 0;
                r->req_hdrs.iReferer = NUM_HEADERS - 1;
                r->req_hdrs.Value[r->req_hdrs.iReferer] = "Connection reset by peer";
                del_from_list(r, ReqMan, i);
                count_resp--;
            }
            else
            {
                if (((t - r->sock_timeout) >= conf->TimeOut) && (r->sock_timeout != 0))
                {
            //        r->err = -1;
                    r->connKeepAlive = 0;
                    print_err(r, "<%s:%d> Timeout = %ld\n", __func__, __LINE__, t - r->sock_timeout);
                    r->req_hdrs.iReferer = NUM_HEADERS - 1;
                    r->req_hdrs.Value[r->req_hdrs.iReferer] = "Timeout";
                    del_from_list(r, ReqMan, i);
                    count_resp--;
                }
                else
                {
                    if (r->sock_timeout == 0)
                        r->sock_timeout = t;
                    ++i;
                }
            }
        }
    }

    delete [] fdwr;
    delete [] rd_buf;
}
//======================================================================
void push_resp_queue1(Connect *req)
{
    lseek(req->resp.fd, req->resp.offset, SEEK_SET);
    req->sock_timeout = 0;
    req->next = NULL;
mtx_send.lock();
    req->prev = list_end;
    if (list_start)
    {
        list_end->next = req;
        list_end = req;
    }
    else
        list_start = list_end = req;
    
    fdwr[size_list].fd = req->clientSocket;
    fdwr[size_list].events = POLLOUT;
    ++size_list;
mtx_send.unlock();
    cond_add.notify_one();
}
//======================================================================
void close_queue1(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
