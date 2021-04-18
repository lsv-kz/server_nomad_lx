#include "main.h"
#include <sys/sendfile.h>

using namespace std;

static Connect *list_start = NULL;
static Connect *list_end = NULL;

static Connect *list_new_start = NULL;
static Connect *list_new_end = NULL;

static int max_resp = 100;

static mutex mtx_send;
static condition_variable cond_add, cond_dec;

static int close_thr = 0;
//======================================================================
int send_part_file(Connect *req, char *buf, int size_buf)
{
    int rd, wr, len;
    errno = 0;
    
    if (req->resp.respContentLength == 0)
        return 0;
    
    if (req->resp.respContentLength >= size_buf)
        len = size_buf;
    else
        len = req->resp.respContentLength;
    
    if (conf->SEND_FILE == 'y')
    {
   //     len = req->resp.respContentLength;
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
  /*      if (req->resp.respContentLength == 0)
            return 0;
    
        if (req->resp.respContentLength >= size_buf)
            len = size_buf;
        else
            len = req->resp.respContentLength;
  */      
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
static void del_from_list(Connect *r)
{
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
    end_response(r);
}
//======================================================================
int set_list(struct pollfd *fdwr)
{
mtx_send.lock();
    if (list_new_start)
    {
        if (list_end)
            list_end->next = list_new_start;
        else
            list_start = list_new_start;
        
        list_new_start->prev = list_end;
        list_end = list_new_end;
        list_new_start = list_new_end = NULL;
    }
mtx_send.unlock();
    
    int i = 0;
    time_t t = time(NULL);
    Connect *r = list_start, *next;
    
    for ( ; r; r = next)
    {
        next = r->next;
        
        if (((t - r->sock_timer) >= conf->TimeOut) && (r->sock_timer != 0))
        {
            r->err = -1;
            print_err(r, "<%s:%d> Timeout = %ld\n", __func__, __LINE__, t - r->sock_timer);
            r->req_hdrs.iReferer = MAX_HEADERS - 1;
            r->req_hdrs.Value[r->req_hdrs.iReferer] = "Timeout";
            del_from_list(r);
        }
        else
        {
            if (r->sock_timer == 0)
                r->sock_timer = t;
            
            fdwr[i].fd = r->clientSocket;
            fdwr[i].events = POLLOUT;
            ++i;
        }

        if (i >= max_resp)
            break;
    }

    return i;
}
//======================================================================
void send_files(int num_chld)
{
    int count_resp = 0;
    int ret = 0;
    int timeout = 100;
    int size_buf;
    char *rd_buf = NULL;
    
    if (conf->SEND_FILE != 'y')
    {
        size_buf = conf->WR_BUFSIZE;
        rd_buf = new (nothrow) char [size_buf];
        if (!rd_buf)
        {
            print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
            exit(1);
        }
    }
    else
        size_buf = conf->WR_BUFSIZE;
    
    struct pollfd *fdwr = new(nothrow) struct pollfd [conf->MAX_REQUESTS];
    if (!fdwr)
    {
        print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
        exit(1);
    }

    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_send);
            while ((list_start == NULL) && (list_new_start == NULL) && (!close_thr))
            {
                cond_add.wait(lk);
            }

            if (close_thr)
                break;
        }
        
        count_resp = set_list(fdwr);
        
        ret = poll(fdwr, count_resp, timeout);
        if (ret == -1)
        {
            print_err("[%d]<%s:%d> Error poll(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
            break;
        }
        else if (ret == 0)
        {
            continue;
        }
        
        if (count_resp < 100) size_buf = conf->WR_BUFSIZE;
        Connect *r = list_start, *next;
        for (int i = 0; (i < count_resp) && (ret > 0) && r; r = next, ++i)
        {
            next = r->next;
            if (fdwr[i].revents == POLLOUT)
            {
                --ret;
                int wr = send_part_file(r, rd_buf, size_buf);
                if (wr == 0)
                {
                    r->err = 0;
                    del_from_list(r);
                }
                else if (wr == -1)
                {
                    r->err = wr;
                    r->req_hdrs.iReferer = MAX_HEADERS - 1;
                    r->req_hdrs.Value[r->req_hdrs.iReferer] = "Connection reset by peer";
                    del_from_list(r);
                }
                else if (wr > 0) 
                {
                    r->sock_timer = 0;
                }
                else if (wr == -EAGAIN)
                {
                    if (size_buf > 8192) size_buf = size_buf/2;
                    r->sock_timer = 0;
                }
            }
            else if (fdwr[i].revents != 0)
            {
                --ret;
                r->err = -1;
                print_err("<%s:%d> Error: fdwr.revents != 0\n", __func__, __LINE__);
                del_from_list(r);
            }
        }
    }
//    print_err("[%d]<%s:%d> *** Exit send_files() ***\n", num_chld, __func__, __LINE__);
    delete [] fdwr;
    delete [] rd_buf;
}
//======================================================================
void push_resp_queue1(Connect *req)
{
    lseek(req->resp.fd, req->resp.offset, SEEK_SET);
    req->sock_timer = 0;
    req->next = NULL;
mtx_send.lock();
    req->prev = list_new_end;
    if (list_new_start)
    {
        list_new_end->next = req;
        list_new_end = req;
    }
    else
        list_new_start = list_new_end = req;
mtx_send.unlock();
    cond_add.notify_one();
}
//======================================================================
void close_queue1(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
