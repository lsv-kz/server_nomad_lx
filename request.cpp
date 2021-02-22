#include "main.h"

using namespace std;

/*====================================================================*/
void get_request(RequestManager *ReqMan)
{
    int readFromClient;
    const char *p;
    Connect *req;

    while(1)
    {
        req = ReqMan->pop_req();
        if (!req)
        {
            print_err("[%d] <%s:%d>  req = NULL\n", ReqMan->get_num_chld(), __func__, __LINE__);
            ReqMan->end_thr(1);
            return;
        }
        else if (req->clientSocket < 0)
        {
            ReqMan->end_thr(1);
            delete req;
            return;
        }
        
        req->init();
        /*--------------------- read_request ---------------------*/
        readFromClient = read_headers(req, req->timeout, conf->TimeOut);
        req->timeout = conf->TimeoutKeepAlive;
        if (readFromClient <= 0)
        {
            if (readFromClient == 0)
            {
                req->req_hdrs.iReferer = NUM_HEADERS - 1;
                req->req_hdrs.Value[req->req_hdrs.iReferer] = "Connection reset by peer";
                req->err = -1;
            }
            else if (readFromClient == -1000)
            {
                req->req_hdrs.iReferer = NUM_HEADERS - 1;
                req->req_hdrs.Value[req->req_hdrs.iReferer] = "Timeout";
                req->err = -1;
            }
            else
                req->err = readFromClient;

            goto end;
        }
        /*--------------------------------------------------------*/
        if ((req->httpProt != HTTP10) && (req->httpProt != HTTP11))
        {
            req->httpProt = HTTP11;
            req->connKeepAlive = 0;
            req->err = -RS505;
            goto end;
        }

        if (req->numReq >= (unsigned int)conf->MaxRequestsPerThr || (conf->KeepAlive == 'n') || (req->httpProt == HTTP10))
            req->connKeepAlive = 0;
        else if (req->req_hdrs.iConnection == -1)
            req->connKeepAlive = 1;

        if ((p = strchr(req->uri, '?')))
        {
            req->uriLen = p - req->uri;
            req->sReqParam = req->uri + req->uriLen + 1;
        }
        else
        {
            if ((p = strstr_case(req->uri, "%3F")))
            {
                req->uriLen = p - req->uri;
                req->sReqParam = req->uri + req->uriLen + 3;
            }
            else
            {
                req->sReqParam = NULL;
                req->uriLen = strlen(req->uri);
            }
        }

        decode(req->uri, req->uriLen, req->decodeUri, sizeof(req->decodeUri) - 1);
        clean_path(req->decodeUri);
        req->lenDecodeUri = strlen(req->decodeUri);

        if (strstr(req->uri, ".php") && (conf->UsePHP != "php-cgi") && (conf->UsePHP != "php-fpm"))
        {
            print_err(req, "<%s:%d> Error UsePHP=%s\n", __func__, __LINE__, conf->UsePHP.str());
            req->err = -RS404;
            goto end;
        }

        if (req->req_hdrs.iUpgrade >= 0)
        {
            print_err(req, "<%s:%d> req->upgrade: %s\n", __func__, __LINE__, req->req_hdrs.Value[req->req_hdrs.iUpgrade]);
            req->connKeepAlive = 0;
            req->err = -RS505;
            goto end;
        }
        //--------------------------------------------------------------
        if ((req->reqMethod == M_GET) || (req->reqMethod == M_HEAD) || (req->reqMethod == M_POST))
        {
            int ret = response(req);
            if (ret == 1)
            {// "req" may be free !!!
                ret = ReqMan->end_thr(0);
                if (ret == EXIT_THR)
                    return;
                else
                    continue;
            }
            
            req->err = ret;
        }
        else if (req->reqMethod == M_OPTIONS)
        {   
            req->err = options(req);
        }
        else
            req->err = -RS501;

    end:

        if (req->err <= -RS101)
        {
            req->resp.respStatus = -req->err;
            send_message(req, "", NULL);

            if ((req->reqMethod == M_POST) || (req->reqMethod == M_PUT))
                req->connKeepAlive = 0;
        }

        ReqMan->end_response(req);
        
        int ret = ReqMan->end_thr(0);
        if (ret)
        {
            return;
        }
    }
}
/*====================================================================*/
int options(Connect *req)
{
    req->resp.respStatus = RS200;
    if (send_response_headers(req, NULL))
    {
        return -1;
    }

    return 0;
}
