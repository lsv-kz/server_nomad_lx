#include "main.h"

using namespace std;

const char *status_resp(int st);
//======================================================================
int send_response_headers(Connect *req, const String *hdrs)
{
    String resp(512);
    if (resp.error())
    {
        print_err(req, "<%s:%d> Error create String object\n", __func__, __LINE__);
        return -1;
    }

    resp << get_str_http_prot(req->httpProt) << " " << status_resp(req->resp.respStatus) << "\r\n"
        << "Date: " << req->resp.sLogTime << "\r\n"
        << "Server: " << conf->ServerSoftware << "\r\n";

    if (req->reqMethod == M_OPTIONS)
        resp << "Allow: OPTIONS, GET, HEAD, POST\r\n";

    if (req->resp.numPart == 1)
    {
        if (req->resp.respContentType)
            resp << "Content-Type: " << req->resp.respContentType << "\r\n";
        resp << "Content-Length: " << req->resp.respContentLength << "\r\n";
        
        resp << "Content-Range: bytes " << req->resp.offset << "-" 
                                        << (req->resp.offset + req->resp.respContentLength - 1) 
                                        << "/" << req->resp.fileSize << "\r\n";
    }
    else if (req->resp.numPart == 0)
    {
        if (req->resp.respContentType)
            resp << "Content-Type: " << req->resp.respContentType << "\r\n";
        if (req->resp.respContentLength >= 0)
        {
            resp << "Content-Length: " << req->resp.respContentLength << "\r\n";
            if (req->resp.respStatus == RS200)
                resp << "Accept-Ranges: bytes\r\n";
        }
        
        if (req->resp.respStatus == RS416)
            resp << "Content-Range: bytes */" << req->resp.fileSize << "\r\n";
    }

    if (req->resp.respStatus == RS101)
    {
        resp << "Upgrade: HTTP/1.1\r\n"
            << "Connection: Upgrade\r\n";
    }
    else
        resp << "Connection: " << (req->connKeepAlive == 0 ? "close" : "keep-alive") << "\r\n";

    if (hdrs)
        resp << hdrs->c_str();

    resp << "\r\n";

    if (resp.error())
    {
        print_err(req, "<%s:%d> Error create response headers\n", __func__, __LINE__);
        req->req_hd.iReferer = MAX_HEADERS - 1;
        req->reqHdValue[req->req_hd.iReferer] = "Error create response headers";
        return -1;
    }

    int n = write_to_client(req, resp.c_str(), resp.size(), conf->TimeOut);
    if (n <= 0)
    {
        print_err(req, "<%s:%d> Sent to client response error; (%d)\n", __func__, __LINE__, n);
        req->req_hd.iReferer = MAX_HEADERS - 1;
        req->reqHdValue[req->req_hd.iReferer] = "Error send response headers";
        return -1;
    }

    return 0;
}
//======================================================================
void send_message(Connect *req, const char *msg, const String *hdrs)
{
    String html(256);
    if ((req->resp.respStatus != RS204) && (req->reqMethod != M_HEAD))
    {
        const char *title = status_resp(req->resp.respStatus);
        html << "<html>\r\n"
                "<head>\r\n"
                "<title>" << title << "</title>\r\n"
                "<meta charset=\"utf-8\">\r\n"
                "</head>\r\n"
                "<body>\r\n"
                "<h3>" << title << "</h3>\r\n"
                "<p>" << (msg ? msg : "") <<  "</p>\r\n"
                "<hr>\r\n" << req->resp.sLogTime << "\r\n"
                "</body>\r\n"
                "</html>\r\n";
        
        req->resp.respContentType = "text/html";
        req->resp.respContentLength = html.size();
    }

    if (req->resp.respStatus == RS204)
    {
        req->resp.respContentLength = 0;
        req->resp.respContentType = NULL;
    }
    
    req->connKeepAlive = 0;

    if ((req->httpProt != HTTP09) && send_response_headers(req, hdrs))
        return;

    if ((req->reqMethod == M_HEAD) || (req->resp.respStatus == RS204))
        return;

    if (req->resp.respContentLength > 0)
    {
        req->resp.send_bytes = write_to_client(req, html.c_str(), req->resp.respContentLength, conf->TimeOut);
        if (req->resp.send_bytes <= 0)
        {
            print_err(req, "<%s:%d> Error write_timeout()\n", __func__, __LINE__);
        }
    }
}
//======================================================================
const char *status_resp(int st)
{
    switch (st)
    {
        case 0:
            return "";
        case RS101:
            return "101 Switching Protocols";
        case RS200:
            return "200 OK";
        case RS204:
            return "204 No Content";
        case RS206:
            return "206 Partial Content";
        case RS301:
            return "301 Moved Permanently";
        case RS302:
            return "302 Moved Temporarily";
        case RS400:
            return "400 Bad Request";
        case RS401:
            return "401 Unauthorized";
        case RS402:
            return "402 Payment Required";
        case RS403:
            return "403 Forbidden";
        case RS404:
            return "404 Not Found";
        case RS405:
            return "405 Method Not Allowed";
        case RS406:
            return "406 Not Acceptable";
        case RS407:
            return "407 Proxy Authentication Required";
        case RS408:
            return "408 Request Timeout";
        case RS411:
            return "411 Length Required";
        case RS413:
            return "413 Request entity too large";
        case RS414:
            return "414 Request-URI Too Large";
        case RS416:
            return "416 Range Not Satisfiable";
        case RS500:
            return "500 Internal Server Error";
        case RS501:
            return "501 Not Implemented";
        case RS502:
            return "502 Bad Gateway";
        case RS503:
            return "503 Service Unavailable";
        case RS504:
            return "504 Gateway Time-out";
        case RS505:
            return "505 HTTP Version not supported";
        default:
            return "500 Internal Server Error";
    }
    return "";
}
