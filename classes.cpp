#include "classes.h"

    //==================================================================
    void Connect::init()
    {
        sRange = NULL;
        //------------------------------------
        uri = NULL;
        p_newline = bufReq;
        tail = NULL;
        //------------------------------------
        err = 0;
        lenTail = 0;
        i_bufReq = 0;
        i_arrHdrs = 0;
        reqMethod = 0;
        httpProt = 0;
        connKeepAlive = 0;
        
        req_hdrs = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1LL,0};
        
        resp.fd = -1;
        resp.respStatus = 0;
        resp.respContentType = NULL;
        
        resp.countRespHeaders = 0;
        resp.send_bytes = 0LL;
        resp.numPart = 0;
        resp.scriptName = NULL;
        
        resp.offset = 0;
        resp.respContentLength = -1LL;
    }
    
    int Connect::hd_read()
    {
        errno = 0;
        if (err) return -1;
        int n = recv(clientSocket, bufReq + i_bufReq, LEN_BUF_REQUEST - i_bufReq - 1, 0);
        if (n < 0)
            return -1;
        else if (n == 0)
            return NO_PRINT_LOG;

        lenTail += n;
        
        i_bufReq += n;
        bufReq[i_bufReq] = 0;

        n = empty_line();
        if (n == 1)
        {
            timeout = conf->TimeOut;
            return i_bufReq;
        }
        else if (n < 0)
            return n;
        
        return 0;
    }
    
    int Connect::empty_line()
    {
        if (err) return -1;
        timeout = conf->TimeOut;
        char *pr, *pn;
        while (lenTail > 0)
        {
            pr = (char*)memchr(p_newline, '\r', lenTail - 1);
            pn = (char*)memchr(p_newline, '\n', lenTail);
            if (pr && pn)
            {
                if ((pn - pr) != 1)
                    return -RS400;
            
                if ((pn - p_newline) == 1)
                {
                    lenTail -= (pn+1 - p_newline);
                    if (lenTail > 0)
                        tail = pn + 1;
                    else
                    {
                        tail = NULL;
                        lenTail = 0;
                    }
                
                    return 1;
                }

                if (i_arrHdrs < MAX_HEADERS)
                {
                    arrHdrs[i_arrHdrs].ptr = p_newline;
                    arrHdrs[i_arrHdrs].len = pn - p_newline + 1;
                    ++i_arrHdrs;
                }
                else
                {
                    return -RS500;
                }

                lenTail -= (pn + 1 - p_newline);
                p_newline = pn + 1;
            }
            else if (pr && (!pn))
                return -RS400;
            else if ((!pr) && pn)
                return -RS400;
            else
                break;
        }

        return 0;
    }
//======================================================================
void ArrayRanges::check_ranges()
{
    if (err) return;
    Range *r = range;

    for ( int n = Len - 1; n > 0; n--)
    {
        for (int i = n - 1; i >= 0; i--)
        {
            if (((r[n].end + 1) >= r[i].start) && ((r[i].end + 1) >= r[n].start))
            {
                if (r[n].start < r[i].start)
                    r[i].start = r[n].start;

                if (r[n].end > r[i].end)
                    r[i].end = r[n].end;

                r[i].len = r[i].end - r[i].start + 1;
                r[n].len = 0;

                n--;
                LenRanges--;
            }
        }
    }
    
    for (int i = 0, j = 0; j < Len; j++)
    {
        if (r[j].len)
        {
            if (i < j)
            {
                r[i].start = r[j].start;
                r[i].end = r[j].end;
                r[i].len = r[j].len;
                r[j].len = 0;
            }
            
            i++;
        }
    }
}
//----------------------------------------------------------------------
void ArrayRanges::parse_ranges(char *sRange)
{
    if (err) return;
    long long start = 0, end = 0, size = sizeFile, ll;
    int i = 0;
    const char *p1;
    char *p2;
    
    if (sRange == NULL)
    {
        err = 1;
        return;
    }
    
    p1 = p2 = sRange;
    
    for ( ; Len < SizeArray; )
    {
        if (err) return;
        if ((*p1 >= '0') && (*p1 <= '9'))
        {
            ll = strtoll(p1, &p2, 10);
            if (p1 < p2)
            {
                if (i == 0)
                    start = ll;
                else if (i == 2)
                    end = ll;
                else
                {
                    err = 416;
                    return;
                }
                            
                i++;
                p1 = p2;
            }
        }
        else if (*p1 == ' ')
            p1++;
        else if (*p1 == '-')
        {
            if (i == 0)
            {
                ll = strtoll(p1, &p2, 10);
                if (ll < 0)
                {
                    start = size + ll;
                    end = size - 1;
                    i = 3;
                    p1 = p2;
                }
                else
                {
                    err = 416;
                    return;
                }
            }
            else if (i == 2)
            {
                err = 416;
                return;
            }
            else
            {
                p1++;
                i++;
            }
        }
        else if (*p1 == ',')
        {
            if (i == 2)
                end = size - 1;
            else if (i != 3)
            {
                err = 416;
                return;
            }
            
            if (end >= size)
                end = size - 1;
            
            if (start <= end)
                (*this) << Range{start, end, end - start + 1};
            
            start = end = 0;
            p1++;
            i = 0;
        }
        else if (*p1 == 0)
        {
            if (i == 2)
                end = size - 1;
            else if (i != 3)
            {
                err = 416;
                return;
            }
            
            if (end >= size)
                end = size - 1;
            
            if (start <= end)
                (*this) << Range{start, end, end - start + 1};
            
            start = end = 0;
            break;
        }
        else
        {
            err = 416;
            return;
        }
    }
}
//----------------------------------------------------------------------
ArrayRanges::ArrayRanges(char *s, long long sz)
{
    if (!s)
    {
        err = RS500;
        return;
    }
    
    if (conf->MaxRanges == 0)
    {
        err = RS403;
        return;
    }

    for ( char *p = s; *p; ++p)
    {
        if (*p == ',')
            SizeArray++;
    }
    
    SizeArray++;
    
    if (SizeArray > conf->MaxRanges)
        SizeArray = conf->MaxRanges;
    reserve();
    sizeFile = sz;
    parse_ranges(s);
    LenRanges = Len;
    if ((Len == 0) && (err == 0))
        err = 416;
    //else if (Len > 1)
    //    check_ranges();
}
