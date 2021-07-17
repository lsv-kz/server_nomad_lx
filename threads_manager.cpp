#include "main.h"

using namespace std;

mutex mtx_conn;
condition_variable cond_close_conn;

static int count_conn = 0;
//======================================================================
RequestManager::RequestManager(int n)
{
    list_start = list_end = NULL;
    size_list = stop_manager = all_thr = 0;
    count_thr = num_wait_thr = 0;
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
lock_guard<std::mutex> lg(mtx_thr);
    return count_thr;
}
//----------------------------------------------------------------------
int RequestManager::get_all_thr(void)
{
    return all_thr;
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
void RequestManager::push_req(Connect *req)
{
mtx_thr.lock();
    req->next = NULL;
    req->prev = list_end;
    if (list_start)
    {
        list_end->next = req;
        list_end = req;
    }
    else
        list_start = list_end = req;

    ++size_list;
mtx_thr.unlock();
    cond_list.notify_one();
}
//----------------------------------------------------------------------
Connect *RequestManager::pop_req()
{
unique_lock<mutex> lk(mtx_thr);
    ++num_wait_thr;
    while (list_start == NULL)
    {
        cond_list.wait(lk);
    }
    --num_wait_thr;
    Connect *req = list_start;
    if (list_start->next)
    {
        list_start->next->prev = NULL;
        list_start = list_start->next;
    }
    else
        list_start = list_end = NULL;
    
    --size_list;
    if (num_wait_thr <= 1)
        cond_new_thr.notify_one();

    return req;
}
//----------------------------------------------------------------------
int RequestManager::wait_create_thr(int *n)
{
unique_lock<mutex> lk(mtx_thr);
    while (((num_wait_thr >= 1) || (count_thr >= conf->MaxThreads) || !count_conn) && !stop_manager)
    {
        cond_new_thr.wait(lk);
    }

    *n = count_thr;
    return stop_manager;
}
//----------------------------------------------------------------------
int RequestManager::end_thr(int ret)
{
mtx_thr.lock();
    
    if (((count_thr > conf->MinThreads) && (size_list <= num_wait_thr)) || ret)
    {
        --count_thr;
        ret = EXIT_THR;
    }
    
mtx_thr.unlock();
    if (ret)
    {
        cond_exit_thr.notify_all();
        cond_new_thr.notify_all();
    }
    return ret;
}
//--------------------------------------------
void RequestManager::close_manager()
{
    stop_manager = 1;
    cond_new_thr.notify_one();
    cond_exit_thr.notify_one();
}
//======================================================================
int get_num_conn(void)
{
lock_guard<std::mutex> lg(mtx_conn);
    return count_conn;
}
//======================================================================
void start_conn(void)
{
mtx_conn.lock();
    ++count_conn;
mtx_conn.unlock();
}
//======================================================================
int check_num_conn()
{
unique_lock<mutex> lk(mtx_conn);
    while (count_conn >= conf->MAX_REQUESTS)
    {
        cond_close_conn.wait(lk);
    }
    return 0;
}
//======================================================================
static int fd_close_conn;
static int nChld;
//----------------------------------------------------------------------
void end_response(Connect *req)
{
    if (req->connKeepAlive == 0 || req->err < 0)
    { // ----- Close connect -----
        if (req->err == 0)
            print_log(req);
        shutdown(req->clientSocket, SHUT_RDWR);
        close(req->clientSocket);
        
        delete req;
    mtx_conn.lock();
        --count_conn;
    mtx_conn.unlock();
        char ch = nChld;
        if (write(fd_close_conn, &ch, 1) <= 0)
        {
            print_err("<%s:%d> Error write(): %s\n", __func__, __LINE__, strerror(errno));
            exit(1);
        }
        
        cond_close_conn.notify_all();
    }
    else
    { // ----- KeepAlive -----
        if (conf->tcp_cork == 'y')
        {
            int optval = 0;
            setsockopt(req->clientSocket, SOL_TCP, TCP_CORK, &optval, sizeof(optval));
        }
        
        print_log(req);
        req->timeout = conf->TimeoutKeepAlive;
        ++req->numReq;
        push_req_list(req);
    }
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
            thr = thread(response1, ReqMan);
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
//    print_err("[%d] <%s:%d> *** Exit thread_req_manager() ***\n", numProc, __func__, __LINE__);
}
//======================================================================
int servSock;
RequestManager *RM;
unsigned long allConn = 0;
//======================================================================
void RequestManager::print_intr()
{
mtx_thr.lock();
    print_err("[%d]<%s:%d> thr=%d, open_conn=%d, qu=%d, num_wait_thr=%d\n", numChld, __func__, __LINE__, 
                                count_thr, count_conn, size_list, num_wait_thr);
mtx_thr.unlock();
}
//======================================================================
static void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
  //      RM->print_intr();
        print_err("[%d] <%s:%d> ### SIGINT ### all_conn=%d\n", nChld, __func__, __LINE__, allConn);
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
void manager(int sockServer, int numChld, int pfd)
{
    String nameFifo;
    nameFifo << "./fifo_" << numChld;
    int fifoFD;
    if ((fifoFD = open(nameFifo.str(), O_RDONLY)) == -1)
    {
        printf("[%u]<%s:%d> Error open: %s\n", numChld, __func__, __LINE__, strerror(errno));
        exit(1);
    }
    
    fd_close_conn = pfd;
    nChld = numChld;
    servSock = sockServer;
    
    RequestManager *ReqMan = new(nothrow) RequestManager(numChld);
    if (!ReqMan)
    {
        print_err("<%s:%d> *********** Exit child %d ***********\n", __func__, __LINE__, numChld);
        close_logs();
        exit(1);
    }
    
    RM = ReqMan;

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
        SendFile = thread(send_files, numChld);
    }
    catch (...)
    {
        print_err("[%d] <%s:%d> Error create thread(send_file_): errno=%d \n", numChld, __func__, __LINE__, errno);
        exit(errno);
    }
    //------------------------------------------------------------------
    thread thrRequest;
    try
    {
        thrRequest = thread(req_handler, ReqMan);
    }
    catch (...)
    {
        print_err("[%d] <%s:%d> Error create thread(req_handler): errno=%d \n", numChld, __func__, __LINE__, errno);
        exit(errno);
    }
    //------------------------------------------------------------------
    if (chdir(conf->rootDir.str()))
    {
        print_err("[%d] <%s:%d> Error chdir(%s): %s\n", numChld, __func__, __LINE__, conf->rootDir.str(), strerror(errno));
        exit(1);
    }
    //------------------------------------------------------------------
    int n = 0;
    while (n < conf->MinThreads)
    {
        thread thr;
        try
        {
            thr = thread(response1, ReqMan);
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
        int clientSocket;
        socklen_t addrSize;
        struct sockaddr_storage clientAddr;
       
        check_num_conn();

        char ch;
        int ret = read(fifoFD, &ch, 1);
        if ((ret != 1) || (ch != nChld))
        {
            print_err("<%s:%d> ret = %d\n", __func__, __LINE__, ret);
            break;
        }

        while (1)
        {
            addrSize = sizeof(struct sockaddr_storage);
            clientSocket = accept(sockServer, (struct sockaddr *)&clientAddr, &addrSize);
            if (clientSocket == -1)
            {
                print_err("[%d] <%s:%d>  Error accept(): %s\n", numChld, __func__, __LINE__, strerror(errno));
                if ((errno == EINTR) || (errno == EMFILE) || (errno == EAGAIN))
                    continue;
            }
            break;
        }
        
        if (clientSocket == -1)
            break;
        ++allConn;

        char chwr = numChld | 0x80;
        if (write(pfd, &chwr, 1) < 1)
        {
            print_err("[%d] <%s:%d>  Error write(): %s\n", numChld, __func__, __LINE__, strerror(errno));
            break;
        }

        Connect *req;
        req = create_req();
        if (!req)
        {
            shutdown(clientSocket, SHUT_RDWR);
            close(clientSocket);
            continue;
        }
/*
        int flags = fcntl(clientSocket, F_GETFL);
        if (flags == -1)
            print_err("<%s:%d> Error fcntl(, F_GETFL, ): %s\n", __func__, __LINE__, strerror(errno));
        else
        {
            if (fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1)
                print_err("<%s:%d> Error fcntl(, F_SETFL, ): %s\n", __func__, __LINE__, strerror(errno));
        }
*/
        int opt = 1;
        ioctl(clientSocket, FIONBIO, &opt);

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

        req->numChld = numChld;
        req->numConn = allConn;
        req->numReq = 0;
        req->clientSocket = clientSocket;
        req->timeout = conf->TimeOut;
        getnameinfo((struct sockaddr *)&clientAddr, 
                    addrSize, 
                    req->remoteAddr, 
                    sizeof(req->remoteAddr), 
                    req->remotePort, 
                    sizeof(req->remotePort), 
                    NI_NUMERICHOST | NI_NUMERICSERV);

        start_conn();
        push_req_list(req);// --- First request ---
    }
    
    ReqMan->close_manager();
    thrReqMan.join();

    n = ReqMan->get_num_thr();
    print_err("[%d] <%s:%d>  numThr=%d; allNumThr=%u; allConn=%u; open_conn=%d\n", numChld, 
                    __func__, __LINE__, n, ReqMan->get_all_thr(), allConn, get_num_conn());

    while (n)
    {
        Connect *req;
        req = create_req();
        if (!req)
        {
            break;
        }
        req->clientSocket = -1;
        ReqMan->push_req(req);
        --n;
    }

    close(fifoFD);
    close(pfd);
    
    close_send_list();
    SendFile.join();

    close_req_list();
    thrRequest.join();

    delete ReqMan;
    sleep(1);
    print_err("[%d] <%s:%d> ***** Exit *****\n", numChld, __func__, __LINE__);
}
//======================================================================
Connect *create_req(void)
{
    Connect *req = new(nothrow) Connect;
    if (!req)
        print_err("<%s:%d> Error malloc(): %s\n", __func__, __LINE__, str_err(errno));
    return req;
}
