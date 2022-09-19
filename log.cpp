#include "main.h"
#include <stdarg.h>

using namespace std;

int flog, flog_err;
mutex mtxLog;
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
    fileName.reserve(log_dir.size() + strlen(buf) + ServerSoftware.size() + 16);
    fileName << log_dir << '/' << buf << '-' << ServerSoftware << ".log";

    flog = open(fileName.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(flog == -1)
    {
        cerr << "  Error create log: " << fileName.c_str() << "\n";
        exit(1);
    }
/*
    struct flock flck;
    flck.l_type = F_WRLCK;
    flck.l_whence = SEEK_SET;
    flck.l_start = 0;
    flck.l_len = 0;
    fcntl(flog, F_SETLK, &flck);

    flock(flog, LOCK_SH); //   LOCK_EX
*/  
//   lockf(flog, F_LOCK, 0);
    //------------------------------------------------------------------
    fileName.clear();
    fileName << log_dir << "/" << ServerSoftware << "-error.log";
    
    flog_err = open(fileName.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); //   O_APPEND 
    if(flog_err == -1)
    {
        cerr << "  Error create log_err: " << fileName.c_str() << "\n";
        exit(1);
    }
/*
    flck.l_type = F_WRLCK;
    flck.l_whence = SEEK_SET;
    flck.l_start = 0;
    flck.l_len = 0;
    fcntl(flog_err, F_SETLK, &flck);

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
    char buf[128];

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    String ss(256);
    ss << "[" << get_time() << "] - " << buf;
mtxLog.lock();
    write(flog_err, ss.c_str(), ss.size());
mtxLog.unlock();
}
//======================================================================
void print_err(Connect *req, const char *format, ...)
{
    va_list ap;
    char buf[128];

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    String ss(256);
    ss << "[" << get_time() << "]-[" << req->numProc << "/" << req->numConn << "/" << req->numReq << "] " << buf;
    
mtxLog.lock();
    write(flog_err, ss.c_str(), ss.size());
mtxLog.unlock();
}
//======================================================================
void print_log(Connect *req)
{
    String ss(320);
    if (req->reqMethod <= 0)
        return;
    ss  << req->numProc << "/" << req->numConn << "/" << req->numReq << " - " << req->remoteAddr
        << " - [" << req->sLogTime << "] - \"" << get_str_method(req->reqMethod) << " " << req->decodeUri
        << ((req->sReqParam) ? "?" : "") << ((req->sReqParam) ? req->sReqParam : "") << " "
        << get_str_http_prot(req->httpProt) << "\" "
        << req->respStatus << " " << req->send_bytes << " "
        << "\"" << ((req->req_hd.iReferer >= 0) ? req->reqHdValue[req->req_hd.iReferer] : "-") << "\" "
        << "\"" << ((req->req_hd.iUserAgent >= 0) ? req->reqHdValue[req->req_hd.iUserAgent] : "-") << "\"\n";
//mtxLog.lock();
    write(flog, ss.c_str(), ss.size());
//mtxLog.unlock();
}
