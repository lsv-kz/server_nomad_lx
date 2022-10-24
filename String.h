#ifndef CLASS_STRING_H_
#define CLASS_STRING_H_

#include <iostream>
#include <cstring>
//#include <string>
//======================================================================
class BaseHex {};
class BaseDec {};

#define Hex BaseHex()
#define Dec BaseDec()
//======================================================================
class String
{
protected:
    const int add = 32;
    unsigned int lenBuf = 0;
    unsigned int sizeBuf = 0;
    int err = 0;
    int base_ = 10;
    unsigned int p_ = 0;

    char *ptr = NULL;

    void append(const String& s)
    {
        if (err || (s.lenBuf == 0)) return;
        if ((lenBuf + s.lenBuf) >= sizeBuf)
        {
            if (reserve(lenBuf + s.lenBuf + add))
                return;
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
            if (reserve(lenBuf + len + add))
                return;
        }
        memcpy(ptr + lenBuf, &ch, len);
        lenBuf += len;
    }
    //[[gnu::noinline]]
    void append(const char *s)
    {
        if (!s || err) return;
        unsigned long len = strlen(s);
        if (len == 0) return;
        if ((lenBuf + len) >= sizeBuf)
        {
            if (reserve(lenBuf + len + add))
                return;
        }
        memcpy(ptr + lenBuf, s, len);
        lenBuf += len;
    }

    void append(const char *src, unsigned int n, unsigned int len_src)
    {
        if (!src || err) return;
        if (n > len_src) n = len_src;
        if ((lenBuf + n) >= sizeBuf)
        {
            if (reserve(lenBuf + n + add))
                return;
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
    //------------------------------------------------------------------
    int get_delimiter()
    {
        if ((ptr == NULL) || (lenBuf == 0))
            return -1;

        for ( ; p_ < lenBuf; ++p_)
        {
            switch (ptr[p_])
            {
                case ' ':
                case '\t':
                case '\r':
                case '\n':
                    continue;
            }
            break;
        }

        unsigned int i = p_;
        for ( ; i < lenBuf; ++i)
        {
            switch (ptr[i])
            {
                case ' ':
                case '\t':
                case '\r':
                case '\n':
                    break;
                default:
                    continue;
            }
            break;
        }
        return i;
    }

public:
    String() {}
    explicit String(int n) { if (n == 0) return; p_ = 0; reserve(n); }
    String(const char *s) { p_ = 0; append(s); }
    String(char *s) { p_ = 0; append(s); }
    String(const String& b) { p_ = 0; append(b); }
    
    String& operator << (BaseHex b)
    {
        base_ = 16;
        return *this;
    }

    String& operator << (BaseDec b)
    {
        base_ = 10;
        return *this;
    }

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
    int reserve(unsigned int n)
    {
        if (err) return err;
        if (n <= sizeBuf)
        {
            if (n == 0)
                destroy();
            return 0;
        }

        char *newBuf = new(std::nothrow) char [n];
        if (!newBuf)
            return (err = 1);

        if (ptr)
        {
            memcpy(newBuf, ptr, lenBuf);
            delete [] ptr;
        }

        sizeBuf = n;
        ptr = newBuf;
        *(ptr + lenBuf) = 0;
        return 0;
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
    //----------------------------- += ---------------------------------
    String & operator += (const String& b) { if (err) return *this; append(b); return *this; }
    String & operator += (const char ch) { if (err) return *this; append(ch); return *this; }
    String & operator += (const char *s) { if (err) return *this; append(s); return *this; }
    //----------------------------- << ---------------------------------
    String & operator << (const String& b) { if (err) return *this; append(b); return *this; }
    String & operator << (const char ch) { if (err) return *this; append(ch); return *this; }
    String & operator << (const char *s) { if (err) return *this; append(s); return *this; }
    String & operator << (char *s) { if (err) return *this; append(s); return *this; }

    String & operator << (double f)
    {
        char s[32];
        snprintf(s, sizeof(s), "%.02f", f);
        append(s);
        return *this;
    }

    template <typename T>
    String & operator << (T t)
    {
        if (err) return *this;
        const unsigned long size_ = 21;
        char s[size_];
        int cnt, minus = (t < 0) ? 1 : 0;
        const char *get_char = "FEDCBA9876543210123456789ABCDEF";
        if (base_ == 16)
            cnt = sizeof(t)*2;
        else
            cnt = 20;

        s[cnt] = 0;
        while (cnt > 0)
        {
            --cnt;
            if (base_ == 10)
            {
                s[cnt] = get_char[15 + (t % 10)];
                t /= 10;
            }
            else
            {
                s[cnt] = get_char[15 + (t & 0x0f)];
                t = t>>4;
            }
            if (t == 0)
                break;
        }

        if (base_ == 10)
            if (minus)
                s[--cnt] = '-';
        append(s + cnt);
        return *this;
    }
    //------------------------------------------------------------------
    void append(const char *s, unsigned int n)
    {
        if (!s || err) return;
        if ((lenBuf + n) >= sizeBuf)
        {
            if (reserve(lenBuf + n + add))
                return;
        }

        memcpy(ptr + lenBuf, s, n);
        lenBuf += n;
    }
    //------------------------------------------------------------------
    const char *c_str() const
    { 
        if (err || (!ptr)) return ""; 
        *(ptr + lenBuf) = 0; 
        return ptr; 
    }

    void clear() { err = lenBuf = p_ = 0; }
    int error() const { return err; }
    unsigned int size() const { if (err) return 0; return lenBuf; }
    unsigned int capacity() const { return sizeBuf; }
    void resize(unsigned int n) { if (err || (n > lenBuf)) return; lenBuf = n; }
    //------------------------------------------------------------------
    String & operator >> (String & s)
    {
        if (err || (this == &s))
            return *this;
        s.clear();
        int next = get_delimiter();
        if (next > 0)
        {
            s.append(ptr + p_, (next - p_), lenBuf);
            p_ = next;
        }

        return *this;
    }

    String & operator >> (char& ch)
    {
        if (err)
            return *this;
        get_delimiter();
        ch = *(ptr + p_);
        if (p_ < lenBuf)
            p_ += 1;
        return *this;
    }

    String & operator >> (double& d) = delete;

    String & operator >> (char *ch) =  delete;

    template <typename T>
    String & operator >> (T &t)
    {
        if (err || (p_ == lenBuf))
            return *this;

        if (get_delimiter() < 0)
            return *this;
        char *pp = ptr + p_;
        t = (T)strtoll(ptr + p_, &pp, base_);
        if ((ptr + p_) != pp)
            p_ += (pp - (ptr + p_));
        else
            t = 0;

        return *this;
    }
    //------------------------------------------------------------------
    friend const bool operator == (const String & s1, const String & s2)
    {
        if (s1.lenBuf != s2.lenBuf)
            return false;

        if (strncmp(s1.c_str(), s2.c_str(), s1.lenBuf))
            return false;
        else
            return true;
    }
    //------------------------------------------------------------------
    friend bool operator == (const String & s1, const char *s2)
    {
        unsigned int len = strlen(s2);
        if (s1.lenBuf != len)
            return false;

        if (strncmp(s1.c_str(), s2, len))
            return false;
        else
            return true;
    }
    //------------------------------------------------------------------
    friend bool operator == (const char *s1, const String & s2)
    {
        unsigned int len = strlen(s1);
        if (s2.lenBuf != len)
            return false;

        if (strncmp(s2.c_str(), s1, len))
            return false;
        else
            return true;
    }
    //------------------------------------------------------------------
    friend bool operator != (const String & s1, const char *s2)
    {
        unsigned long len = strlen(s2);
        if (s1.lenBuf != len)
            return true;

        if (strncmp(s1.c_str(), s2, len))
            return true;
        else
            return false;
    }
    //------------------------------------------------------------------
    const char operator[] (unsigned int n) const
    {
        if (err || (n >= lenBuf)) return -1;
        return *(ptr + n);
    }
};

#endif
