#include "main.h"

using namespace std;

static Connect *list_start = NULL;
static Connect *list_end = NULL;

static Connect *list_new_start = NULL;
static Connect *list_new_end = NULL;

static mutex mtx_req;
static condition_variable cond_add;

static int close_thr = 0;
//======================================================================
static void del_from_list(Connect *r)
{
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
void close_conn(Connect *r)
{
    r->err = -1;
    del_from_list(r);
    end_response(r);
}
//======================================================================
void push_req_queue(Connect *r, RequestManager *ReqMan)
{
    del_from_list(r);
    ReqMan->push_req(r);
}
//======================================================================
static int set_list(struct pollfd *fdrd)
{
mtx_req.lock();
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
mtx_req.unlock();
    
    int i = 0;
    time_t t = time(NULL);
    Connect *r = list_start, *next;
    
    for ( ; r; r = next)
    {
        next = r->next;
        
        if (((t - r->sock_timer) >= r->timeout) && (r->sock_timer != 0))
        {
            print_err(r, "<%s:%d> Timeout = %ld\n", __func__, __LINE__, t - r->sock_timer);
            close_conn(r);
        }
        else
        {
            if (r->sock_timer == 0)
                r->sock_timer = t;
            
            fdrd[i].fd = r->clientSocket;
            fdrd[i].events = POLLIN;
            ++i;
        }
    }

    return i;
}
//======================================================================
void req_handler(RequestManager *ReqMan)
{
    int count_resp = 0;
    int ret = 0;
    int timeout = 1;
    int num_chld = ReqMan->get_num_chld();
    
    struct pollfd *fdrd = new(nothrow) struct pollfd [conf->MAX_REQUESTS];
    if (!fdrd)
    {
        print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
        exit(1);
    }
    
    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_req);
            while ((list_start == NULL) && (list_new_start == NULL) && (!close_thr))
            {
                cond_add.wait(lk);
            }

            if (close_thr)
                break;
        }
        
        count_resp = set_list(fdrd);
        if (count_resp == 0)
        {
            continue;
        }

        ret = poll(fdrd, count_resp, timeout); 
        if (ret == -1)
        {
            print_err("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            break;
        }
        else if (ret == 0)
        {
            continue;
        }

        Connect *req = list_start, *next;
        for (int i = 0; (i < count_resp) && (ret > 0) && req; req = next, ++i)
        {
            next = req->next;
            if (fdrd[i].revents == POLLIN)
            {
                --ret;
                int ret = req->hd_read();
                if (ret < 0)
                    close_conn(req);
                else if (ret > 0)
                    push_req_queue(req, ReqMan);
                else
                    req->sock_timer = 0;
            }
            else if (fdrd[i].revents)
            {
                --ret;
                print_err("[%d]<%s:%d> Error: fdwr.revents != 0\n", num_chld, __func__, __LINE__);
                close_conn(req);
            }
        }
    }
    
//    print_err("[%d]<%s:%d> *** Exit req_handler() ***\n", num_chld, __func__, __LINE__);
    delete [] fdrd;
}
//======================================================================
void push_req_list(Connect *req)
{
    req->init();
    get_time(req->resp.sLogTime);
    req->sock_timer = 0;
    req->next = NULL;
mtx_req.lock();
    req->prev = list_new_end;
    if (list_new_start)
    {
        list_new_end->next = req;
        list_new_end = req;
    }
    else
        list_new_start = list_new_end = req;
mtx_req.unlock();
    cond_add.notify_one();
}
//======================================================================
void close_req_list(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
