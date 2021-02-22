#include "classes.h"

    //==================================================================
    void Connect::init()
    {
        err = 0;
        bufReq[0] = '\0';
        decodeUri[0] = '\0';
        sRange = NULL;
        //------------------------------------
        uri = NULL;
        tail = NULL;
        req_hdrs.Name[0] = NULL;
        req_hdrs.Value[0] = NULL;
        //------------------------------------
        reqMethod = 0;
        httpProt = 0;
        connKeepAlive = 0;
        
        req_hdrs = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, -1LL};
        
        resp.respStatus = 0;
        resp.sLogTime = "";
        resp.respContentType = NULL;
        resp.scriptType = 0;
        
        resp.countRespHeaders = 0;
        resp.send_bytes = 0LL;
        resp.numPart = 0;
        resp.scriptName = NULL;
        
        resp.fd = -1;
        resp.offset = 0;
        resp.respContentLength = -1LL;
        resp.fileSize = 0;
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
        
        const char *p = ss.str();
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
