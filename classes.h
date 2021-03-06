#ifndef CLASSES_H_
#define CLASSES_H_

#include "main.h"

//======================================================================
struct Range {
    long long start;
    long long end;
    long long part_len;
};
//----------------------------------------------------------------------
class ArrayRanges
{
protected:
    const int ADDITION = 8;
    Range *range = NULL;
    unsigned int sizeBuf = 0;
    unsigned int lenBuf = 0;
    int numPart;
    long long sizeFile;
    int err = 0;
    
    int check_ranges();
    int parse_ranges(char *sRange, String& ss);
    
public:
    ArrayRanges(const ArrayRanges&) = delete;
    ArrayRanges() { }
    ~ArrayRanges() { if (range) { delete [] range; } }
    
    void reserve(unsigned int n)
    {
        if (n <= lenBuf)
        {
            err = 1;
            return;
        }
        Range *tmp = new(std::nothrow) Range [n];
        if (!tmp)
        {
            err = ENOMEM;
            return;
        }
        for (unsigned int i = 0; i < lenBuf; ++i)
            tmp[i] = range[i];
        if (range)
            delete [] range;
        range = tmp;
        sizeBuf = n;
    }

    ArrayRanges & operator << (const Range& val)
    {
        if (err) return *this;
        if (lenBuf >= sizeBuf)
        {
            reserve(sizeBuf + ADDITION);
            if (err) return *this;
        }
        range[lenBuf++] = val;
        return *this;
    }
    
    Range *get(unsigned int i)
    {
        if (err) return NULL;
        if (i < lenBuf)
            return range + i;
        else
            return NULL;
    }
    
    int len() { if (err) return 0; return lenBuf; }
    int capacity() { if (err) return 0; return sizeBuf; }
    int error() { return err; }
    
    int get_ranges(char *s, long long sz);
};
//======================================================================
const int CHUNK_SIZE_BUF = 4096;
const int MAX_LEN_SIZE_CHUNK = 6;
enum mode_chunk {NO_SEND = 0, SEND_NO_CHUNK, SEND_CHUNK};
//----------------------------------------------------------------------
class ClChunked
{
    int i, mode, allSend;
    int err = 0;
    Connect *req;
    char buf[CHUNK_SIZE_BUF + MAX_LEN_SIZE_CHUNK + 10];
    ClChunked() {};
    //------------------------------------------------------------------
    int send_chunk(int size)
    {
        if (err) return -1;
        const char *p;
        int len;
        if (mode == SEND_CHUNK)
        {
            String ss(16);
            ss << Hex << size << "\r\n" << Dec;
            len = ss.len();
            int n = MAX_LEN_SIZE_CHUNK - len;
            if (n < 0)
                return -1;
            memcpy(buf + n, ss.str(), len);
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, "\r\n", 2);
            i += 2;
            p = buf + n;
            len += i;
        }
        else
        {
            p = buf + MAX_LEN_SIZE_CHUNK;
            len = i;
        }

        int ret = write_to_client(req, p, len, conf->TimeOut);

        i = 0;
        if (ret > 0)
            allSend += ret;
        return ret;
    }
public://---------------------------------------------------------------
    ClChunked(Connect *rq, int m){req = rq; mode = m; i = allSend = err = 0;}
    //------------------------------------------------------------------
    ClChunked & operator << (const long long ll)
    {
        if (err) return *this;
        String ss(32);
        ss << ll;
        *this << ss;
/*
        int n = 0, len = ss.len();
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }
        
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, ss.str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }

        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, ss.str() + n, len);
        i += len;*/
        return *this;
    }
    //------------------------------------------------------------------
    ClChunked & operator << (const char *s)
    {
        if (err) return *this;
        if (!s)
        {
            err = 1;
            return *this;
        }
        int n = 0, len = strlen(s);
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }
        
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }
        
        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, len);
        i += len;
        return *this;
    }
    //------------------------------------------------------------------
    ClChunked & operator << (const String& s)
    {
        if (err) return *this;
        *this << s.str();
    /*
        int n = 0, len = s.len();
        if (len == 0) return *this;
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }
        
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }
        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.str() + n, len);
        i += len;*/
        return *this;
    }
    //------------------------------------------------------------------
/*    ClChunked & operator << (const std::string& s)
    {
        if (err) return *this;
        int n = 0, len = s.size();
        if (len == 0) return *this;
        if (mode == NO_SEND)
        {
            allSend += len;
            return *this;
        }
        
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.c_str() + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return *this;
            }
        }
        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s.c_str() + n, len);
        i += len;
        return *this;
    }*/
    //------------------------------------------------------------------
    int add_arr(const char *s, int len)
    {
        if (err) return -1;
        if (!s) return -1;
        if (mode == NO_SEND)
        {
            allSend += len;
            return 0;
        }
        
        int n = 0;
        while (CHUNK_SIZE_BUF < (i + len))
        {
            int l = CHUNK_SIZE_BUF - i;
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, l);
            i += l;
            len -= l;
            n += l;
            int ret = send_chunk(i);
            if (ret < 0)
            {
                err = 1;
                return ret;
            }
        }

        memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s + n, len);
        i += len;
        return 0;
    }
    //------------------------------------------------------------------
    int cgi_to_client(int fdPipe)
    {
        if (err) return -1;
        int all_rd = 0;
        while (1)
        {
            if (CHUNK_SIZE_BUF <= i)
            {
                int ret = send_chunk(i);
                if (ret < 0)
                {
                    err = 1;
                    return ret;
                }
            }
            
            int rd = CHUNK_SIZE_BUF - i;
            int ret = read_timeout(fdPipe, buf + MAX_LEN_SIZE_CHUNK + i, rd, conf->TimeoutCGI);
            if (ret == 0)
            {
       //         print_err("<%s:%d> ret=%d, all_rd=%d\n", __func__, __LINE__, ret, all_rd);
                break;
            }
            else if (ret < 0)
            {
                i = 0;
                return ret;
            }

            i += ret;
            all_rd += ret;
        }

        return all_rd;
    }
    //------------------------------------------------------------------
    int fcgi_to_client(int fcgi_sock, int len)
    {
        if (err) return -1;
        if (mode == NO_SEND)
        {
            allSend += len;
            fcgi_to_cosmos(fcgi_sock, len, conf->TimeoutCGI);
            return 0;
        }
        
        while (len > 0)
        {
            if (CHUNK_SIZE_BUF <= i)
            {
                int ret = send_chunk(i);
                if (ret < 0)
                    return ret;
            }
            
            int rd = (len < (CHUNK_SIZE_BUF - i)) ? len : (CHUNK_SIZE_BUF - i);
            int ret = read_timeout(fcgi_sock, buf + MAX_LEN_SIZE_CHUNK + i, rd, conf->TimeoutCGI);
            if (ret <= 0)
            {
                print_err("<%s:%d> ret=%d\n", __func__, __LINE__, ret);
                i = 0;
                return -1;
            }
            else if (ret != rd)
            {
                print_err("<%s:%d> ret != rd\n", __func__, __LINE__);
                i = 0;
                return -1;
            }
            
            i += ret;
            len -= ret;
        }
        
        return 0;
    }
    //------------------------------------------------------------------
    int end()
    {
        if (err) return -1;
        if (mode == SEND_CHUNK)
        {
            int n = i;
            const char *s = "\r\n0\r\n";
            int len = strlen(s);
            memcpy(buf + MAX_LEN_SIZE_CHUNK + i, s, len);
            i += len;
            return send_chunk(n);
        }
        else if (mode == SEND_NO_CHUNK)
            return send_chunk(0);
        else
            return 0;
    }
    //------------------------------------------------------------------
    int all(){return allSend;}
    int error() { return err; }
};

#endif
