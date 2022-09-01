#include "main.h"

#if defined(LINUX_)
    #include <sys/sendfile.h>
#elif defined(FREEBSD_)
    #include <sys/uio.h>
#endif

using namespace std;

static Connect *list_start = NULL;
static Connect *list_end = NULL;

static Connect *list_new_start = NULL;
static Connect *list_new_end = NULL;

static Connect **array_conn = NULL;
static struct pollfd *arr_pollfd;

static mutex mtx_;
static condition_variable cond_;

static int close_thr = 0;
static int num_proc_;

int size_buf;
char *snd_buf;
//======================================================================
int send_part_file(Connect *req)
{
    int rd, wr, len;
    errno = 0;
    
    if (req->resp.respContentLength == 0)
        return 0;
#if defined(SEND_FILE_) && (defined(LINUX_) || defined(FREEBSD_))
    if (conf->SEND_FILE == 'y')
    {
        if (req->resp.respContentLength >= size_buf)
            len = size_buf;
        else
            len = req->resp.respContentLength;
    #if defined(LINUX_)
        wr = sendfile(req->clientSocket, req->resp.fd, &req->resp.offset, len);
        if (wr == -1)
        {
            if (errno == EAGAIN)
                return -EAGAIN;
            print_err(req, "<%s:%d> Error sendfile(); %s\n", __func__, __LINE__, strerror(errno));
            return wr;
        }
    #elif defined(FREEBSD_)
        off_t wr_bytes;
        int ret = sendfile(req->resp.fd, req->clientSocket, req->resp.offset, len, NULL, &wr_bytes, 0);// SF_NODISKIO SF_NOCACHE
        if (ret == -1)
        {
            if (errno == EAGAIN)
            {
                if (wr_bytes == 0)
                    return -EAGAIN;
                req->resp.offset += wr_bytes;
                wr = wr_bytes;
            }
            else
            {
                print_err("<%s:%d> Error sendfile(); %s\n", __func__, __LINE__, strerror(errno));
                return -1;
            }
        }
        else if (ret == 0)
        {
            req->resp.offset += wr_bytes;
            wr = wr_bytes;
        }
        else
        {
            print_err("<%s:%d> Error sendfile()=%d, wr_bytes=%ld\n", __func__, __LINE__, ret, wr_bytes);
            return -1;
        }
    #endif
    }
    else
#endif
    {
        if (req->resp.respContentLength >= size_buf)
            len = size_buf;
        else
            len = req->resp.respContentLength;

        rd = read(req->resp.fd, snd_buf, len);
        if (rd <= 0)
        {
            if (rd == -1)
                print_err(req, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
            return rd;
        }

        wr = write(req->clientSocket, snd_buf, rd);
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
            lseek(req->resp.fd, wr - rd, SEEK_CUR);
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
        list_start = list_end = NULL;
}
//======================================================================
int set_list()
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
    Connect *r = list_start, *next = NULL;
    for ( ; r; r = next)
    {
        next = r->next;
        
        if (((t - r->sock_timer) >= r->timeout) && (r->sock_timer != 0))
        {
            if (r->reqMethod)
            {
                r->err = -1;
                print_err(r, "<%s:%d> Timeout = %ld\n", __func__, __LINE__, t - r->sock_timer);
                r->req_hd.iReferer = MAX_HEADERS - 1;
                r->reqHdValue[r->req_hd.iReferer] = "Timeout";
            }
            else
                r->err = NO_PRINT_LOG;
            
            del_from_list(r);
            end_response(r);
        }
        else
        {
            if (r->sock_timer == 0)
                r->sock_timer = t;
            
            arr_pollfd[i].fd = r->clientSocket;
            arr_pollfd[i].events = r->event;
            array_conn[i] = r;
            ++i;
        }
    }

    return i;
}
//======================================================================
int poll_(int num_chld, int i, int nfd, RequestManager *ReqMan)
{
    int ret = poll(arr_pollfd + i, nfd, conf->TIMEOUT_POLL);
    if (ret == -1)
    {
        print_err("[%d]<%s:%d> Error poll(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
        return -1;
    }
    else if (ret == 0)
        return 0;

    Connect *r = NULL;
    for ( ; (i < nfd) && (ret > 0); ++i)
    {
        r = array_conn[i];
        if (arr_pollfd[i].revents == POLLOUT)
        {
            int wr = send_part_file(r);
            if (wr == 0)
            {
                del_from_list(r);
                end_response(r);
            }
            else if (wr == -1)
            {
                r->err = wr;
                r->req_hd.iReferer = MAX_HEADERS - 1;
                r->reqHdValue[r->req_hd.iReferer] = "Connection reset by peer";

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
            --ret;
        }
        else if (arr_pollfd[i].revents == POLLIN)
        {
            int n = r->hd_read();
            if (n == -EAGAIN)
                r->sock_timer = 0;
            else if (n < 0)
            {
                r->err = n;
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
            --ret;
        }
        else if (arr_pollfd[i].revents)
        {
            print_err(r, "<%s:%d> Error: events=0x%x, revents=0x%x\n", __func__, __LINE__, arr_pollfd[i].events, arr_pollfd[i].revents);
            if (r->event == POLLOUT)
            {
                r->req_hd.iReferer = MAX_HEADERS - 1;
                r->reqHdValue[r->req_hd.iReferer] = "Connection reset by peer";
                r->err = -1;
            }
            else
                r->err = NO_PRINT_LOG;

            del_from_list(r);
            end_response(r);
            --ret;
        }
    }

    return i;
}
//======================================================================
void event_handler(RequestManager *ReqMan)
{
    int num_chld = ReqMan->get_num_chld();
    int count_resp = 0;
    size_buf = conf->SEND_FILE_SIZE_PART;
    snd_buf = NULL;
    num_proc_ = num_chld;
    
#if defined(SEND_FILE_) && (defined(LINUX_) || defined(FREEBSD_))
    if (conf->SEND_FILE != 'y')
#endif
    {
        size_buf = conf->SNDBUF_SIZE;
        snd_buf = new (nothrow) char [size_buf];
        if (!snd_buf)
        {
            print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
            exit(1);
        }
    }
    
    arr_pollfd = new(nothrow) struct pollfd [conf->MAX_REQUESTS];
    if (!arr_pollfd)
    {
        print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
        exit(1);
    }

    array_conn = new(nothrow) Connect* [conf->MAX_REQUESTS];
    if (!array_conn)
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

        count_resp = set_list();
        if (count_resp == 0)
            continue;
        
        int nfd;
        for (int i = 0; count_resp > 0; )
        {
            if (count_resp > conf->MAX_EVENT_SOCK)
                nfd = conf->MAX_EVENT_SOCK;
            else
                nfd = count_resp;

            int ret = poll_(num_chld, i, nfd, ReqMan);
            if (ret < 0)
            {
                print_err("[%d]<%s:%d> Error poll_()\n", num_chld, __func__, __LINE__);
                break;
            }
            else if (ret == 0)
                break;

            i += nfd;
            count_resp -= nfd;
        }
    }

    delete [] arr_pollfd;
    delete [] array_conn;
#if defined(SEND_FILE_) && (defined(LINUX_) || defined(FREEBSD_))
    if (conf->SEND_FILE != 'y')
#endif
        if (snd_buf) delete [] snd_buf;
    print_err("*** Exit [%s:proc=%d] ***\n", __func__, num_chld);
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
