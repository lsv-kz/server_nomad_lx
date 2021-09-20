#include "main.h"
#include <sys/sendfile.h>

using namespace std;

static Connect *list_start = NULL;
static Connect *list_end = NULL;

static Connect *list_new_start = NULL;
static Connect *list_new_end = NULL;

static mutex mtx_;
static condition_variable cond_;

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
            {
                lseek(req->resp.fd, -rd, SEEK_CUR);
                return -EAGAIN;
            }
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
    if (r->event == POLLOUT)
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
}
//======================================================================
int set_list(struct pollfd *fdwr)
{
mtx_.lock();
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
mtx_.unlock();
    
    int i = 0;
    time_t t = time(NULL);
    Connect *r = list_start, *next;
    
    for ( ; r; r = next)
    {
        next = r->next;
        
        if (((t - r->sock_timer) >= r->timeout) && (r->sock_timer != 0))
        {
            r->err = -1;
            print_err(r, "<%s:%d> Timeout = %ld\n", __func__, __LINE__, t - r->sock_timer);
            r->req_hdrs.iReferer = MAX_HEADERS - 1;
            r->req_hdrs.Value[r->req_hdrs.iReferer] = "Timeout";
            
            del_from_list(r);
            end_response(r);
        }
        else
        {
            if (r->sock_timer == 0)
                r->sock_timer = t;
            
            fdwr[i].fd = r->clientSocket;
            fdwr[i].events = r->event;
            ++i;
        }
    }

    return i;
}
//======================================================================
int ff(Connect *r)
{
    if (r->connKeepAlive == 0 || r->err < 0)
        return 1;
    else
        return 0;
}
//======================================================================
void event_handler(RequestManager *ReqMan)
{
    int num_chld = ReqMan->get_num_chld();
    int count_resp = 0;
    int ret = 1, n, wr;
    int size_buf = conf->WR_BUFSIZE;
    char *rd_buf = NULL;
    
    if (conf->SEND_FILE != 'y')
    {
        rd_buf = new (nothrow) char [size_buf];
        if (!rd_buf)
        {
            print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
            exit(1);
        }
    }
    
    struct pollfd *fdwr = new(nothrow) struct pollfd [conf->MAX_REQUESTS];
    if (!fdwr)
    {
        print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
        exit(1);
    }

    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_);
            
            while ((!list_start) && (!list_new_start) && (!close_thr))
            {
                cond_.wait(lk);
            }

            if (close_thr)
                break;
        }
        

        count_resp = set_list(fdwr);
        if (count_resp == 0)
            continue;
        
        ret = poll(fdwr, count_resp, conf->TIMEOUT_POLL);
        if (ret == -1)
        {
            print_err("[%d]<%s:%d> Error poll(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
            break;
        }
        else if (ret == 0)
        {
            //print_err("[%d]<%s:%d> count_resp=%d\n", num_chld, __func__, __LINE__, count_resp);
            continue;
        }
        
        Connect *r = list_start, *next;
        for (int i = 0; (i < count_resp) && (ret > 0) && r; r = next, ++i)
        {
            next = r->next;

            if (fdwr[i].revents == POLLOUT)
            {
                --ret;
                wr = send_part_file(r, rd_buf, size_buf);
                if (wr == 0)
                {
                    del_from_list(r);
                    end_response(r);
                }
                else if (wr == -1)
                {
                    r->err = wr;
                    r->req_hdrs.iReferer = MAX_HEADERS - 1;
                    r->req_hdrs.Value[r->req_hdrs.iReferer] = "Connection reset by peer";
                        
                    del_from_list(r);
                    end_response(r);
                }
                else if (wr > 0) 
                    r->sock_timer = 0;
                else if (wr == -EAGAIN)
                {
                    r->sock_timer = 0;
                    print_err(r, "<%s:%d> Error: EAGAIN\n", __func__, __LINE__);
                }
            }
            else if (fdwr[i].revents == POLLIN)
            {
                --ret;
                n = r->hd_read();
                if (n < 0)
                {
                    r->err = -1;
                    del_from_list(r);
                    end_response(r);
                }
                else if (n > 0)
                {
                    del_from_list(r);
                    push_resp_list(r, ReqMan);
                }
                else
                    r->sock_timer = 0;
            }
            else if (fdwr[i].revents)
            {
                --ret;
                print_err(r, "<%s:%d> Error: revents=0x%x\n", __func__, __LINE__, fdwr[i].revents);
                r->err = -1;
                del_from_list(r);
                end_response(r);
            }
        }
    }
//    print_err("[%d]<%s:%d> *** Exit send_files() ***\n", num_chld, __func__, __LINE__);
    delete [] fdwr;
    if (conf->SEND_FILE != 'y')
        delete [] rd_buf;
}
//======================================================================
void push_pollout_list(Connect *req)
{
    req->event = POLLOUT;
    lseek(req->resp.fd, req->resp.offset, SEEK_SET);
    req->sock_timer = 0;
    req->next = NULL;
mtx_.lock();
    req->prev = list_new_end;
    if (list_new_start)
    {
        list_new_end->next = req;
        list_new_end = req;
    }
    else
        list_new_start = list_new_end = req;
mtx_.unlock();
    cond_.notify_one();
}
//======================================================================
void push_pollin_list(Connect *req)
{
    req->init();
    get_time(req->resp.sLogTime);
    req->event = POLLIN;
    req->sock_timer = 0;
    req->next = NULL;
mtx_.lock();
    req->prev = list_new_end;
    if (list_new_start)
    {
        list_new_end->next = req;
        list_new_end = req;
    }
    else
        list_new_start = list_new_end = req;
    
mtx_.unlock();
    cond_.notify_one();
}
//======================================================================
void close_event_handler(void)
{
    close_thr = 1;
    cond_.notify_one();
}
