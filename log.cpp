#include "main.h"
#include <stdarg.h>

using namespace std;

int flog, flog_err;
mutex mtxLog;
int get_len_queue(int *num_thr);
//======================================================================
void create_logfiles(const String & log_dir, const String& ServerSoftware)
{
    char buf[256];
    struct tm tm1;
    time_t t1;

    time(&t1);
    tm1 = *localtime(&t1);
    strftime(buf, sizeof(buf), "%Y-m%m-%d_%Hh%Mm%Ss", &tm1);

    String fileName;
    fileName.reserve(log_dir.len() + strlen(buf) + ServerSoftware.len() + 16);
    fileName << log_dir;
    fileName << '/';
    fileName << buf;
    fileName << '-';
    fileName << ServerSoftware;
    fileName << ".log";

    flog = open(fileName.str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // fopen(logfile, "w");
    if(flog == -1)
    {
        cerr << "  Error create log: " << fileName.str() << "\n";
        exit(1);
    }
    
    struct flock flck;
    flck.l_type = F_WRLCK;
    flck.l_whence = SEEK_SET;
    flck.l_start = 0;
    flck.l_len = 0;
    fcntl(flog, F_SETLK, &flck);
/*
    flock(flog, LOCK_SH); //   LOCK_EX
*/  
//   lockf(flog, F_LOCK, 0);
    //------------------------------------------------------------------
    fileName.clear();
    fileName << log_dir;
    fileName << "/";
    fileName << ServerSoftware;
    fileName << "-error.log";
    
    flog_err = open(fileName.str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // fopen(logfile, "w");
    if(flog_err == -1)
    {
        cerr << "  Error create log_err: " << fileName.str() << "\n";
        exit(1);
    }
    
    flck.l_type = F_WRLCK;
    flck.l_whence = SEEK_SET;
    flck.l_start = 0;
    flck.l_len = 0;
    fcntl(flog_err, F_SETLK, &flck);
/*
    flock(flog_err, LOCK_SH); //   LOCK_EX
*/  
//   lockf(flog_err, F_LOCK, 0);
    dup2(flog_err, STDERR_FILENO);
}
//======================================================================
void close_logs(void)
{
    close(flog);
    close(flog_err);
}
//======================================================================
void print_err(const char *format, ...)
{
    va_list ap;
    char buf[MAX_PATH * 2];

    va_start(ap, format);
    vsnprintf(buf, MAX_PATH * 2, format, ap);
    va_end(ap);
    String ss(256);
    ss << "[" << get_time() << "] - " << buf;
mtxLog.lock();
    write(flog_err, ss.str(), ss.len());
mtxLog.unlock();
}
//======================================================================
void print_err(Connect *req, const char *format, ...)
{
    va_list ap;
    char buf[MAX_PATH * 2];

    va_start(ap, format);
    vsnprintf(buf, MAX_PATH * 2, format, ap);
    va_end(ap);
    
    String ss(256);
    ss << "[" << get_time() << "]-[" << req->numChld << "/" << req->numConn << "/" << req->numReq << "] " << buf;
    
mtxLog.lock();
    write(flog_err, ss.str(), ss.len());
mtxLog.unlock();
}
//======================================================================
void print_log(Connect *req)
{
    String ss(256);
        
    ss << req->numChld << "/" << req->numConn << "/" << req->numReq << " - " << req->remoteAddr << ":" << req->remotePort
            << " - [" << req->resp.sLogTime << "] - ";
    if (req->reqMethod > 0)
            ss << "\"" << get_str_method(req->reqMethod) << " " << req->uri
               << " " << get_str_http_prot(req->httpProt) << "\" ";
    else
            ss << "\"-\" ";
        
    ss << req->resp.respStatus << " " << req->resp.send_bytes << " "
            << "\"" << ((req->req_hdrs.iReferer >= 0) ? req->req_hdrs.Value[req->req_hdrs.iReferer] : "-") << "\" "
            << "\"" << ((req->req_hdrs.iUserAgent >= 0) ? req->req_hdrs.Value[req->req_hdrs.iUserAgent] : "-")
            << "\""
            << " " << req->connKeepAlive << "\n";
mtxLog.lock();
    write(flog, ss.str(), ss.len());
mtxLog.unlock();
}
