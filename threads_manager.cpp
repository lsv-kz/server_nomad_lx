#include "main.h"

using namespace std;

//======================================================================
RequestManager::RequestManager(int n)
{
    list_begin = list_end = NULL;
    len_qu = stop_manager = all_thr = need_create_thr = 0;
    count_thr = count_conn = num_wait_thr = 0;
    numChld = n;
}
//----------------------------------------------------------------------
RequestManager::~RequestManager()
{
    free_fcgi_list();
}
//----------------------------------------------------------------------
int RequestManager::get_num_chld(void)
{
    return numChld;
}
//----------------------------------------------------------------------
int RequestManager::get_num_thr(void)
{
mtx_thr.lock();
    int ret = count_thr;
mtx_thr.unlock();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::get_all_thr(void)
{
    return all_thr;
}
//----------------------------------------------------------------------
int RequestManager::get_num_conn(void)
{
//lock_guard<std::mutex> lg(mtx_thr);
mtx_thr.lock();
    int ret = count_conn;
mtx_thr.unlock();
    return ret;
}
//----------------------------------------------------------------------
int RequestManager::start_thr(void)
{
mtx_thr.lock();
    int ret = ++count_thr;
    ++all_thr;
mtx_thr.unlock();
    return ret;
}
//----------------------------------------------------------------------
void RequestManager::wait_exit_thr(int n)
{
    unique_lock<mutex> lk(mtx_thr);
    while (n == count_thr)
    {
        cond_exit_thr.wait(lk);
    }
}
//----------------------------------------------------------------------
int RequestManager::push_req(Connect *req, int inc)
{
    int ret;
mtx_thr.lock();
    req->next = NULL;
    req->prev = list_end;
    if (list_begin)
    {
        list_end->next = req;
        list_end = req;
    }
    else
        list_begin = list_end = req;
        
    if (inc)
        ++count_conn;
    ret = ++len_qu;
    
mtx_thr.unlock();

    cond_push.notify_one();

    return ret;
}
//----------------------------------------------------------------------
Connect *RequestManager::pop_req()
{
    Connect *req;
    
unique_lock<mutex> lk(mtx_thr);
    ++num_wait_thr;
    while (list_begin == NULL)
    {
        cond_push.wait(lk);
    }
    --num_wait_thr;
    req = list_begin;
    if (list_begin->next)
    {
        list_begin->next->prev = NULL;
        list_begin = list_begin->next;
    }
    else
        list_begin = list_end = NULL;
    
    --len_qu;
    if (num_wait_thr <= 1)
    {
        need_create_thr = 1;
        cond_new_thr.notify_one();
    }

    return req;
}
//----------------------------------------------------------------------
int RequestManager::wait_create_thr(int *n)
{
    while (1)
    {
    unique_lock<mutex> lk(mtx_thr);

        while (((need_create_thr == 0) || (num_wait_thr > 0)) && !stop_manager)
        {
            cond_new_thr.wait(lk);
        }
        
        if (stop_manager)
            return num_wait_thr;
        
        while ((count_thr >= conf->MaxThreads) && !stop_manager)
        {
            cond_exit_thr.wait(lk);
        }

        if (stop_manager)
            return num_wait_thr;

        if ((need_create_thr > 0) && (num_wait_thr <= 0))
        {
            need_create_thr = 0;
            *n = count_thr;
            break;
        }
    }

    return stop_manager;
}
//----------------------------------------------------------------------
int RequestManager::end_thr(int ret)
{
mtx_thr.lock();
    
    if (((count_thr > conf->MinThreads) && (len_qu <= num_wait_thr)) || ret)
    {
        --count_thr;
        ret = EXIT_THR;
    }
    
mtx_thr.unlock();
    if (ret)
        cond_exit_thr.notify_all();
    return ret;
}
//--------------------------------------------
void RequestManager::close_manager()
{
    stop_manager = 1;
    cond_new_thr.notify_one();
    cond_exit_thr.notify_one();
}
//----------------------------------------------------------------------
void RequestManager::end_response(Connect *req)
{
    if (conf->tcp_cork == 'y')
    {
        int optval = 0;
        setsockopt(req->clientSocket, SOL_TCP, TCP_CORK, &optval, sizeof(optval));
    }
    
    if (req->connKeepAlive == 0 || req->err < 0)
    {
        if (req->err == 0)
            print_log(req);
        //----------------- close connect ------------------------------
        shutdown(req->clientSocket, SHUT_RDWR);
        close(req->clientSocket);
        delete req;
    mtx_thr.lock();
        --count_conn;
    mtx_thr.unlock();
        cond_close_conn.notify_all();
    }
    else
    {
        print_log(req);
        ++req->numReq;
        push_resp_queue2(req);
    }
}
//----------------------------------------------------------------------
int RequestManager::check_num_conn()
{
unique_lock<mutex> lk(mtx_thr);
    while (count_conn >= conf->MAX_REQUESTS)
    {
        cond_close_conn.wait(lk);
    }
    return 0;
}
//======================================================================
void thr_create_manager(int numProc, RequestManager *ReqMan)
{
    int num_thr;
    thread thr;

    while (1)
    {
        if (ReqMan->wait_create_thr(&num_thr))
            break;
        try
        {
            thr = thread(get_request, ReqMan);
        }
        catch (...)
        {
            print_err("[%d] <%s:%d> Error create thread: num_thr=%d, errno=%d\n", numProc, __func__, __LINE__, num_thr, errno);
            ReqMan->wait_exit_thr(num_thr);
            continue;
        }

        thr.detach();

        ReqMan->start_thr();
    }
}
//======================================================================
static int nChld;
int servSock;
//======================================================================
static void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        print_err("[%d] <%s:%d> ###### SIGINT ######\n", nChld, __func__, __LINE__);
        shutdown(servSock, SHUT_RDWR);
        close(servSock);
        
    }
    else if (sig == SIGSEGV)
    {
        print_err("[%d] <%s:%d> ### SIGSEGV ###\n", nChld, __func__, __LINE__);
        exit(1);
    }
}
//======================================================================
Connect *create_req(void);
static int sys_sndbufsize = 0;
void set_sndbuf(int n); 
//======================================================================
void manager(int sockServer, int numChld)
{
    unsigned long allConn = 0;
    RequestManager *ReqMan = new(nothrow) RequestManager(numChld);
    if (!ReqMan)
    {
        print_err("<%s:%d> *********** Exit child %d ***********\n", __func__, __LINE__, numChld);
        close_logs();
        exit(1);
    }
    
    nChld = numChld;
    servSock = sockServer;

    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        print_err("<%s:%d> Error signal(SIGINT): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (signal(SIGSEGV, signal_handler) == SIG_ERR)
    {
        print_err("<%s:%d> Error signal(SIGSEGV): %s\n", __func__, __LINE__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    //----------------------------------------
    thread SendFile;
    try
    {
        SendFile = thread(send_files, ReqMan);
    }
    catch (...)
    {
        print_err("[%d] <%s:%d> Error create thread(send_file_): errno=%d \n", numChld, __func__, __LINE__, errno);
        exit(errno);
    }
    //------------------------------------------------------------------
    thread thrQueue2;
    try
    {
        thrQueue2 = thread(queue2, ReqMan);
    }
    catch (...)
    {
        print_err("[%d] <%s:%d> Error create thread(Queue2): errno=%d \n", numChld, __func__, __LINE__, errno);
        exit(errno);
    }
    //------------------------------------------------------------------
    int n = 0;
    while (n < conf->MinThreads)
    {
        thread thr;
        try
        {
            thr = thread(get_request, ReqMan);
        }
        catch (...)
        {
            print_err("[%d] <%s:%d> Error create thread: errno=%d\n", numChld, __func__, __LINE__);
            exit(errno);
        }

        ReqMan->start_thr();
        thr.detach();
        ++n;
    }
    //------------------------------------------------------------------
    thread thrReqMan;
    try
    {
        thrReqMan = thread(thr_create_manager, numChld, ReqMan);
    }
    catch (...)
    {
        print_err("<%s:%d> Error create thread %d: errno=%d \n", __func__, 
                __LINE__, ReqMan->get_all_thr(), errno);
        exit(errno);
    }
    //------------------------------------------------------------------    
    printf("[%d:%s:%d] +++++ num threads=%d, pid=%d, uid=%d, gid=%d  +++++\n", numChld, __func__, 
                            __LINE__, ReqMan->get_num_thr(), getpid(), getuid(), getgid());

    while (1)
    {
        socklen_t addrSize;
        struct sockaddr_storage clientAddr;
       
        ReqMan->check_num_conn();

        addrSize = sizeof(struct sockaddr_storage);
        int clientSocket = accept(sockServer, (struct sockaddr *)&clientAddr, &addrSize);
        if (clientSocket == -1)
        {
            if (errno == EINTR)
            {
                print_err("<%s:%d>  Error accept(): %s\n", __func__, __LINE__, strerror(errno));
                continue;
            }
            else if (errno == EMFILE)
            {
                print_err("<%s:%d>  Error accept(): %s; num_req=%d\n", __func__, __LINE__, strerror(errno), ReqMan->get_num_conn());
                continue;
            }
            else if (errno == EAGAIN)
            {
                print_err("<%s:%d>  Error accept(): EAGAIN; %d\n", __func__, __LINE__, ReqMan->get_num_conn());
                continue;
            }
            else
            {
                print_err("[%d] <%s:%d>  Error accept(): %s\n", numChld, __func__, __LINE__, strerror(errno));
                break;
            }
        }

        Connect *req;
        req = create_req();
        if (!req)
        {
            shutdown(clientSocket, SHUT_RDWR);
            close(clientSocket);
            continue;
        }

        int flags = fcntl(clientSocket, F_GETFL);
        if (flags == -1)
            print_err("<%s:%d> Error fcntl(, F_GETFL, ): %s\n", __func__, __LINE__, strerror(errno));
        else
        {
            if (fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1)
                print_err("<%s:%d> Error fcntl(, F_SETFL, ): %s\n", __func__, __LINE__, strerror(errno));
        }
/*
        int opt = 1;
        ioctl(clientSocket, FIONBIO, &opt);
*/
        if (sys_sndbufsize == 0)
        {
            socklen_t optlen = sizeof(sys_sndbufsize);
            if (getsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, (void *)&sys_sndbufsize, &optlen) == 0)
            {
                printf("[%u] <%s:%d> system_bufsize=%d\n", numChld, __func__, __LINE__, sys_sndbufsize);
                if ((sys_sndbufsize/2) < conf->WR_BUFSIZE)
                    set_sndbuf(sys_sndbufsize/2);
            }
            else
                print_err("[%u] <%s:%d> Error getsockopt(SO_SNDBUF): %s\n", numChld, __func__, __LINE__, strerror(errno));
        }
        
        if (conf->tcp_cork == 'y')
        {
            int optval = 1;
            setsockopt(clientSocket, SOL_TCP, TCP_CORK, &optval, sizeof(optval));
        }

        req->numChld = numChld;
        req->numConn = ++allConn;
        req->numReq = 0;
        req->clientSocket = clientSocket;
        req->timeout = conf->TimeOut;
        req->remoteAddr[0] = '\0';
        req->remotePort[0] = '\0';
        getnameinfo((struct sockaddr *)&clientAddr, 
                    addrSize, 
                    req->remoteAddr, 
                    sizeof(req->remoteAddr), 
                    req->remotePort, 
                    sizeof(req->remotePort), 
                    NI_NUMERICHOST | NI_NUMERICSERV);

        ReqMan->push_req(req, 1);
    }

    ReqMan->close_manager();
    thrReqMan.join();

    n = ReqMan->get_num_thr();
    print_err("[%d] <%s:%d>  numThr=%d; allNumThr=%u; allConn=%u; num_req=%d\n", numChld, 
                    __func__, __LINE__, n, ReqMan->get_all_thr(), allConn, ReqMan->get_num_conn());

    while (n)
    {
        Connect *req;
        req = create_req();
        if (!req)
        {
            break;
        }
        req->clientSocket = -1;
        ReqMan->push_req(req, 0);
        --n;
    }
    
    close_queue1();
    SendFile.join();
    
    close_queue2();
    thrQueue2.join();

    delete ReqMan;
}
//======================================================================
Connect *create_req(void)
{
    Connect *req = NULL;

    req = new(nothrow) Connect;
    if (!req)
    {
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, str_err(errno));
    }
    else
    {
        req->init();
    }
    
    return req;
}
