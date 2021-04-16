#ifndef CLASS_STRING_H_
#define CLASS_STRING_H_

#include <cstring>
#include <string>
//======================================================================
class BaseString
{
public:
    int b;
    BaseString(int n) { b = n; }
};
//======================================================================
#define Hex BaseString(16)
#define Dec BaseString(10)
//======================================================================
class String
{
protected:
    const int ADDITION = 128;
    int add = ADDITION;
    unsigned int lenBuf = 0;
    unsigned int sizeBuf = 0;
    int err = 0;
    int base_ = 10;
    unsigned int p_ = 0;
    
    char *ptr = NULL;
    
    void append(const String & s)
    {
        if (err) return;
        if (s.lenBuf == 0) return;
        if ((lenBuf + s.lenBuf) >= sizeBuf)
        {
            if (lenBuf)
                reserve(lenBuf + s.lenBuf + 1 + add);
            else
                reserve(s.lenBuf + 1);
            if (err) return;
        }
        memcpy(ptr + lenBuf, s.ptr, s.lenBuf);
        lenBuf += s.lenBuf;
    }
    
    void append(const char ch)
    {
        if (err) return;
        unsigned long len = 1;
        if ((lenBuf + len) >= sizeBuf)
        {
            if (lenBuf)
                reserve(lenBuf + len + 1 + add);
            else
                reserve(len + 1);
            if (err) return;
        }
        memcpy(ptr + lenBuf, &ch, len);
        lenBuf += len;
    }
    
    void append(const char * s)
    {
        if (err) return;
        if (!s) return;
        unsigned long len = strlen(s);
        if (len == 0) return;
        if ((lenBuf + len) >= sizeBuf)
        {
            if (lenBuf)
                reserve(lenBuf + len + 1 + add);
            else
                reserve(len + 1);
            if (err) return;
        }
        memcpy(ptr + lenBuf, s, len);
        lenBuf += len;
    }
    
    void append(const std::string & s)
    {
        if (err) return;
        if (s.size() == 0) return;
        unsigned long len = s.size();
        if (len == 0) return;
        if ((lenBuf + len) >= sizeBuf)
        {
            if (lenBuf)
                reserve(lenBuf + len + 1 + add);
            else
                reserve(len + 1);
            if (err) return;
        }
        memcpy(ptr + lenBuf, s.c_str(), len);
        lenBuf += len;
    }
    
    void append(const char *src, unsigned int n, unsigned int len_src)
    {
        if (err) return;
        if (n > len_src) n = len_src;
        if ((lenBuf + n) >= sizeBuf)
        {
            reserve(lenBuf + n + 1 + add);
            if (err) return;
        }
        
        memcpy(ptr + lenBuf, src, n);
        lenBuf += n;
    }
    
    void destroy()
    {
        if (ptr)
        {
            delete [] ptr;
            ptr = NULL;
            sizeBuf = lenBuf = 0;
        }
        err = p_ = 0;
        base_ = 10;
    }

public:
    String() {}
    explicit String(int n) { if (n == 0) return; reserve(n); }
    String(const char *s) { append(s); }
    String(char *s) { append(s); }
    String(const String& b) { append(b); }
    
    String(String&& b)
    {
        ptr = b.ptr;
        p_ = b.p_;
        lenBuf = b.lenBuf;
        sizeBuf = b.sizeBuf;
            
        b.ptr = NULL;
        b.sizeBuf = b.lenBuf = 0;
    }
    
    ~String() { destroy(); }
    //------------------------------------------------------------------
    void reserve(unsigned int n)
    {
        if (err) return;
        if (n <= sizeBuf)
        {
            if (n == 0)
                destroy();
            return;
        }
        
        char *newBuf = new(std::nothrow) char [n];
        if (!newBuf)
        {
            err = 1;
            return;
        }
        if (ptr)
        {
            memcpy(newBuf, ptr, lenBuf);
            delete [] ptr;
        }

        sizeBuf = n;
        ptr = newBuf;
        *(ptr + lenBuf) = 0;
    }
//----------------------- = ----------------------------
    String & operator = (const String& b)
    {
        if (err) return *this;
        if (this != &b)
        {
            lenBuf = 0;
            append(b);
            p_ = 0;
        }
        return *this;
    }

    template <typename T>
    String & operator = (const T& t)
    {
        if (err) return *this;
        lenBuf = 0;
        append(t);
        p_ = 0;
        return *this;
    }
    //----------------------------- << ---------------------------------
    String & operator << (const String& b) { if (err) return *this; append(b); return *this; }
    String & operator << (const char ch) { if (err) return *this; append(ch); return *this; }
    String & operator << (const char *s) { if (err) return *this; append(s); return *this; }
    String & operator << (char *s) { if (err) return *this; append(s); return *this; }
    String & operator << (const std::string& s) { if (err) return *this; append(s); return *this; }
    
    String & operator << (double f)
    {
        char s[32];
        snprintf(s, sizeof(s), "%.02f", f);
        *this << s;
        return *this;
    }
    
    template <typename T>
    String & operator << (T t)
    {
        if (err) return *this;
        const unsigned long size = 21;
        char s[size];
        int cnt, minus = 0;
        const char *byte_to_char = "FEDCBA9876543210123456789ABCDEF";
        if (base_ == 16)
            cnt = sizeof(t)*2;
        else
        {
            cnt = size - 1;
            if (t < 0) minus = 1;
        }
        s[cnt] = 0;
        while (cnt > 0)
        {
            --cnt;
            if (base_ == 10)
            {
                s[cnt] = byte_to_char[15 + (t % 10)];
                t /= 10;
            }
            else
            {
                s[cnt] = byte_to_char[15 + (t & 0x0f)];
                t = t>>4;
            }
            if (t == 0) break;
        }
        if (base_ == 10)
        {
            if (cnt <= 0)
            {
                err = 1;
                return *this;
            }
            if (minus) s[--cnt] = '-';
        }
        
        append(s + cnt);
        return *this;
    }
    //------------------------------------------------------------------
    void append(const char *s, unsigned int n)
    {
        if (!s || err) return;
        if ((lenBuf + n) >= sizeBuf)
        {
            reserve(lenBuf + n + 1 + add);
            if (err) return;
        }
        
        memcpy(ptr + lenBuf, s, n);
        lenBuf += n;
    }
    //------------------------------------------------------------------
    String  &operator << (BaseString b)
    {
        if (err) return *this;
        base_ = b.b;
        return *this;
    }

    const char *str() const
    { 
        if (err || (!ptr)) return ""; 
        *(ptr + lenBuf) = 0; 
        return ptr; 
    }
    
    const char *get_tail() const
    {
        if (err || (!ptr)) return "";
        *(ptr + lenBuf) = 0;
        return ptr + p_;
    }
    
    void clear() { err = lenBuf = p_ = 0; }
    int error() const { return err; }
    unsigned int len() const { if (err) return 0; return lenBuf; }
    unsigned int capacity() const { return sizeBuf; }
    void resize(unsigned int n) { if (err || (n > lenBuf)) return; lenBuf = n; }
    int base() const { return base_; }
    int get_p() const { return p_; }
    //----------------------------- >> ---------------------------------
    const char *get_delimiter()
    {
        if ((ptr == NULL) || (lenBuf == 0))
        {
            return NULL;
        }
        
        for ( ; (ptr[p_] == ' ') || (ptr[p_] == '\t') || (ptr[p_] == '\r') || (ptr[p_] == '\n'); ++p_)
        {
            if (p_ >= lenBuf) break;
        }
        
        char *p1 = (char*)memchr(ptr + p_, ' ', lenBuf - p_);
        char *p2 = (char*)memchr(ptr + p_, '\t', lenBuf - p_);
        char *p3 = (char*)memchr(ptr + p_, '\r', lenBuf - p_);
        char *p4 = (char*)memchr(ptr + p_, '\n', lenBuf - p_);
        
        char *p5 = ptr + lenBuf;
        
        if (!p1) p1 = p5;
        if (!p2) p2 = p5;
        if (!p3) p3 = p5;
        if (!p4) p4 = p5;
        
        if ((p1 < p2) && (p1 < p3) && (p1 < p4))
            return p1;
        else if ((p2 < p1) && (p2 < p3) && (p2 < p4))
            return p2;
        else if ((p3 < p1) && (p3 < p2) && (p3 < p4))
            return p3;
        else if ((p4 < p1) && (p4 < p2) && (p4 < p3))
            return p4;
        else
            return p5;
    }
    
    String & operator >> (String & s)
    {
        if (err || (this == &s)) return *this;
        s.clear();
        const char *p = get_delimiter();
        if (p)
        {
            s.append(ptr + p_, (p - (ptr + p_)), lenBuf);
            p_ += (p - (ptr + p_));
        }
        
        return *this;
    }
    
    String & operator >> (std::string & s)
    {
        if (err) return *this;
        s.clear();
        const char *p = get_delimiter();
        if (p)
        {
            s.append(ptr + p_, (p - (ptr + p_)));
            p_ += (p - (ptr + p_));
        }
        return *this;
    }
    
    String & operator >> (char& ch)
    {
        if (err) return *this;
        const char *p = get_delimiter();
        if (p)
        {
            ch = *(ptr + p_);
            if (p_ < lenBuf)
                p_ += 1;
        }
        return *this;
    }

    template <typename T>
    String & operator >> (T &t)
    {
        if (err || (p_ == lenBuf)) return *this;
        const char *p = get_delimiter();
        if (!p) return *this;
        char *pp = ptr + p_;
        t = (T)strtoll(str() + p_, &pp, base_);
        if ((ptr + p_) != pp)
            p_ += (pp - (ptr + p_));
        else
        {
            err = 1;
            t = 0;
        }
        return *this;
    }
    //------------------------------------------------------------------
    int get_front(char *s, int size) 
    {
        if (err || (p_ == lenBuf))
        {
            *s = 0;
            return 1;
        }
        
        const char *p = get_delimiter();
        if (p)
        {
            int len = p - (ptr + p_);
            if (size > len)
            {
                memcpy(s, ptr + p_, len);
                *(s + len) = 0;
                p_ += (p - (ptr + p_));
            }
            else
                err = 1;
        }
        return err;
    }
    //--------------------------- == -----------------------------------
    friend const bool operator == (const String & s1, const String & s2)
    {
        if (s1.lenBuf != s2.lenBuf) return false;
        if (strncmp(s1.str(), s2.str(), s1.lenBuf))
            return false;
        else
            return true;
    }
    
    friend bool operator == (const String & s1, const char *s2)
    {
        unsigned int len = strlen(s2);
        if (s1.lenBuf != len) return false;
        if (strncmp(s1.str(), s2, len))
            return false;
        else
            return true;
    }
    
    friend bool operator == (const char *s1, const String & s2)
    {
        unsigned int len = strlen(s1);
        if (s2.lenBuf != len) return false;
        if (strncmp(s2.str(), s1, len))
            return false;
        else
            return true;
    }
    //---------------------------- != ----------------------------------
    friend bool operator != (const String & s1, const char *s2)
    {
        unsigned long len = strlen(s2);
        if (s1.lenBuf != len) return true;
        if (strncmp(s1.str(), s2, len))
            return true;
        else
            return false;
    }
    //------------------------------------------------------------------
    const char operator[] (unsigned int n)
    {
        if (err || (n >= lenBuf)) return -1;
        return *(ptr + n);
    }
};

#endif
