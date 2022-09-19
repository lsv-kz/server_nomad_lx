#include "classes.h"

//======================================================================
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
    countReqHeaders = 0;
    reqMethod = 0;
    httpProt = 0;
    connKeepAlive = 0;

    req_hd = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1LL};

    fd = -1;
    respStatus = 0;
    respContentType = NULL;

    send_bytes = 0LL;
    numPart = 0;
    scriptName = NULL;

    offset = 0;
    respContentLength = -1LL;
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
    char *pr, *pn, ch;
    while (lenTail > 0)
    {
        int i = 0, len_line = 0;
        pr = pn = NULL;
        while (i < lenTail)
        {
            ch = *(p_newline + i);
            if (ch == '\r')
            {
                if (i == (lenTail - 1))
                    return 0;
                pr = p_newline + i;
            }
            else if (ch == '\n')
            {
                pn = p_newline + i;
                if ((pr) && ((pn - pr) != 1))
                    return -RS400;
                i++;
                break;
            }
            else
                len_line++;
            i++;
        }

        if (pn)
        {
            if (pr == NULL)
                *pn = 0;
            else
                *pr = 0;

            if (len_line == 0)
            {
                if (countReqHeaders == 0)
                {
                    if ((pn - bufReq + 1) > 4)
                        return -RS400;
                    lenTail -= i;
                    p_newline = pn + 1;
                    continue;
                }

                if (lenTail > 0)
                {
                    tail = pn + 1;
                    lenTail -= i;
                }
                else 
                    tail = NULL;
                return 1;
            }

            if (countReqHeaders < MAX_HEADERS)
            {
                reqHdName[countReqHeaders] = p_newline;
                countReqHeaders++;
            }
            else
                return -RS500;

            lenTail -= i;
            p_newline = pn + 1;
        }
        else if (pr && (!pn))
            return -RS400;
        else
            break;
    }

    return 0;
}
//======================================================================
/*void ArrayRanges::check_ranges()
{
    if (err) return;
    Range *r = range;

    for ( int n = nRanges - 1; n > 0; n--)
    {
        for (int i = n - 1; i >= 0; i--)
        {
            if (((r[n].end + 1) >= r[i].start) && ((r[i].end + 1) >= r[n].start))
            {
                nRanges--;
                if (r[n].start < r[i].start)
                    r[i].start = r[n].start;

                if (r[n].end > r[i].end)
                    r[i].end = r[n].end;

                r[i].len = r[i].end - r[i].start + 1;
                r[n].len = 0;
                
                if (((nRanges) > n) && (r[nRanges].len > 0))
                {
                    r[n].start = r[nRanges].start;
                    r[n].end = r[nRanges].end;
                    r[n].len = r[nRanges].len;
                    r[nRanges].len = 0;
                }

                n--;
            }
        }
    }
}*/
//----------------------------------------------------------------------
void ArrayRanges::check_ranges()
{
    if (err) return;
    int num = nRanges;
    Range *r = range;

    for ( int n = num - 1; n > 0; n--)
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
                nRanges--;
            }
        }
    }
    
    for (int i = 0, j = 0; j < num; j++)
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

    p1 = p2 = sRange;
    
    for ( ; nRanges < SizeArray; )
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
                    err = RS416;
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
                    err = RS416;
                    return;
                }
            }
            else if (i == 2)
            {
                err = RS416;
                return;
            }
            else
            {
                p1++;
                i++;
            }
        }
        else if ((*p1 == ',') || (*p1 == 0))
        {
            if (i == 2)
                end = size - 1;
            else if (i != 3)
            {
                err = RS416;
                return;
            }
            
            if (end >= size)
                end = size - 1;
            
            if (start <= end)
            {
                (*this) << Range{start, end, end - start + 1};
                if (*p1 == 0)
                    break;
                start = end = 0;
                p1++;
                i = 0;
            }
            else
            {
                err =  RS416;
                return;
            }
        }
        else
        {
            err = RS416;
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
    if ((nRanges == 0) && (err == 0))
        err = RS416;
    //else if (nRanges > 1)
    //    check_ranges();
}
