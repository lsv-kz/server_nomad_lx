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
        if (err) return -1;
        int n = recv(clientSocket, bufReq + i_bufReq, LEN_BUF_REQUEST - i_bufReq - 1, 0);
        if (n <= 0)
            return -1;

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
int ArrayRanges::check_ranges()
{
    int numPart, maxIndex, n;
    Range *r = range;
    numPart = lenBuf;
    maxIndex = n = numPart - 1;

    while (n > 0)
    {
        for (int i = n - 1; i >= 0; )
        {
            if (((r[n].end + 1) >= r[i].start) && ((r[i].end + 1) >= r[n].start))
            {
                if (r[n].start < r[i].start)
                    r[i].start = r[n].start;

                if (r[n].end > r[i].end)
                    r[i].end = r[n].end;

                r[i].part_len = r[i].end - r[i].start + 1;
                r[n].part_len = 0;

                if ((maxIndex > n) && (r[maxIndex].part_len > 0))
                {
                    r[n].start = r[maxIndex].start;
                    r[n].end = r[maxIndex].end;
                    r[n].part_len = r[maxIndex].part_len;
                    r[maxIndex].part_len = 0;
                    maxIndex--;
                    n = maxIndex - 1;
                }
                else
                {
                    maxIndex--;
                    n = maxIndex;
                }
                numPart--;
                i = n - 1;
            }
            else
                i--;
        }
        n--;
    }

    return numPart;
}
//----------------------------------------------------------------------
int ArrayRanges::parse_ranges(char *sRange, String& ss)
{
    char *p0 = sRange, *p;

    if (ss.error())
    {
        err = 1;
        return 0;
    }
    long long size = sizeFile;
    int numPart = 0;
    
    for (; *p0; p0++)
    {
        if ((*p0 != ' ') && (*p0 != '\t'))
            break;
    }

    for (p = p0; *p; )
    {
        long long start = 0, end = 0;
        char ch = *p;
        if ((ch >= '0') && (ch <= '9'))
        {
            start = strtoll(p, &p, 10);
            if (*p == '-')
            {
                ch = *(++p);
                if ((ch >= '0') && (ch <= '9'))// [10-50]
                {
                    end = strtoll(p, &p, 10);
                    if ((*p != ',') && (*p != 0))
                        break;
                }
                else if ((ch == ',') || (ch == 0))// [10-]
                    end = size - 1;
                else
                    break;
            }
            else
            {
                return 0;
            }
        }
        else if (ch == '-')
        {
            if ((*(p+1) >= '0') && (*(p+1) <= '9'))// [-50]
            {
                end = strtoll(p, &p, 10);
                if ((*p != ',') && (*p != 0))
                    break;
                start = size + end;
                end = size - 1;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            break;
        }
        
        if (end >= size)
            end = size - 1;
        
        if ((start < size) && (end >= start) && (start >= 0))
        {
            ss << start << "-" << end;
            if (*p == ',')
                ss << ",";
            numPart++;
            if (*p == 0)
                break;
        }
        p++;
    }

    return numPart;
}
//----------------------------------------------------------------------
int ArrayRanges::get_ranges(char *s, long long sz)
{
    String ss(128);
    sizeFile = sz;
    numPart = parse_ranges(s, ss);
    if (numPart > 0)
    {
        reserve(numPart);
        if (err)
        {
            numPart = 0;
            return 0;
        }
        
        const char *p = ss.c_str();
        char *pp;
        for (int i = 0; i < numPart; ++i)
        {
            long long start, end;
            start = strtoll(p, &pp, 10);
            pp++;
            p = pp;
            
            end = strtoll(p, &pp, 10);
            pp++;
            p = pp;
            
            (*this) << Range{start, end, end - start + 1};
            if (err) return 0;
        }
        
        if (numPart > 1)
            numPart = check_ranges();
    }
    return numPart;
}
