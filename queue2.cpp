#include "main.h"

using namespace std;

static Connect *list_start = NULL;
static Connect *list_end = NULL;

static mutex mtx_;
static condition_variable cond_add;

static int close_thr = 0;
static int size_list = 0;

struct pollfd *fdrd;
//======================================================================
void del_from_list2(Connect *r, RequestManager *ReqMan, int i)
{
mtx_.lock();
    
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
    if (r->clientSocket != fdrd[i].fd)
    {
        print_err(r, "<%s:%d> clSock = %d, .fd = %d\n", __func__, __LINE__, 
                    r->clientSocket, fdrd[i].fd);
        exit(8);
    }
*/
    for (; (i + 1) < size_list; ++i)
    {
        fdrd[i] = fdrd[i + 1];
    }
    --size_list;
mtx_.unlock();

    if (r->err == -1)
        ReqMan->end_response(r);
    else
        ReqMan->push_req(r, 0);
}
//======================================================================
void queue2(RequestManager *ReqMan)
{
    int count_resp = 0;
    int ret = 0;
    int timeout = 5; // conf->TimeoutKeepAlive * 1000;
    int num_chld = ReqMan->get_num_chld();
    
    fdrd = new(nothrow) struct pollfd [conf->MAX_REQUESTS];
    if (!fdrd)
    {
        print_err("[%d]<%s:%d> Error malloc(): %s\n", num_chld, __func__, __LINE__, strerror(errno));
        exit(1);
    }
    
    while (1)
    {
        {
            unique_lock<mutex> lk(mtx_);
            while ((list_start == NULL) && (!close_thr))
            {
                cond_add.wait(lk);
            }
            count_resp = size_list;
            if (close_thr)
                break;
        }

        ret = poll(fdrd, count_resp, timeout); 
        if (ret == -1)
        {
            print_err("<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            break;
        }

        Connect *r = list_start, *next;
        time_t t = time(NULL);
        for (int i = 0; (i < count_resp) && r; r = next)
        {
            next = r->next;

            if (fdrd[i].revents & POLLIN)
            {
                del_from_list2(r, ReqMan, i);
                count_resp--;
            }
            else if (fdrd[i].revents)
            {
                r->err = -1;
                del_from_list2(r, ReqMan, i);
                count_resp--;
            }
            else
            {
                if (((t - r->sock_timeout) > conf->TimeoutKeepAlive) && (r->sock_timeout != 0))
                {
                    r->err = -1;
                    print_err(r, "<%s:%d> Timeout = %ld\n", __func__, __LINE__, t - r->sock_timeout);
                    del_from_list2(r, ReqMan, i);
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

    print_err("[%d]<%s:%d> *** Exit queue2() ***\n", num_chld, __func__, __LINE__);
    delete [] fdrd;
}
//======================================================================
void push_resp_queue2(Connect *req)
{
    req->sock_timeout = 0;
    req->next = NULL;
mtx_.lock();
    req->prev = list_end;
    if (list_start)
    {
        list_end->next = req;
        list_end = req;
    }
    else
        list_start = list_end = req;

    fdrd[size_list].fd = req->clientSocket;
    fdrd[size_list].events = POLLIN;
    ++size_list;
mtx_.unlock();
    cond_add.notify_one();
}
//======================================================================
void close_queue2(void)
{
    close_thr = 1;
    cond_add.notify_one();
}
