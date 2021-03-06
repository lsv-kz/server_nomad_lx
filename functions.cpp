#include "main.h"

using namespace std;

//======================================================================
String get_time()
{
    struct tm t;
    char s[40];
    time_t now = time(NULL);

    gmtime_r(&now, &t);

    strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S GMT", &t);
    return s;
}
//======================================================================
void get_time(String& str)
{
    struct tm t;
    char s[40];
    time_t now = time(NULL);

    gmtime_r(&now, &t);

    strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S GMT", &t);
    str = s;
}
/*====================================================================*/
const char *strstr_case(const char *str1, const char *str2)
{
    const char *s1, *p1, *s2, *p2;
    char c1, c2;
    
    s1 = str1;
    s2 = str2;
    
    if (!s1 || !s2) return NULL;
    if (*s2 == 0) return s1;

    int diff = ('a' - 'A');
    
    for (; ; ++s1)
    {
        c1 = *s1;
        if (!c1) break;
        c2 = *s2;
        c1 += (c1 >= 'A') && (c1 <= 'Z') ? diff : 0;
        c2 += (c2 >= 'A') && (c2 <= 'Z') ? diff : 0;
        if (c1 == c2)
        {
            p1 = s1;
            p2 = s2;
            ++s1;
            ++p2;

            for (; ; ++s1, ++p2)
            {
                c2 = *p2;
                if (!c2) return p1;
                
                c1 = *s1;
                if (!c1) return NULL;

                c1 += (c1 >= 'A') && (c1 <= 'Z') ? diff : 0;
                c2 += (c2 >= 'A') && (c2 <= 'Z') ? diff : 0;
                if (c1 != c2)
                    break;
            }
        }
    }

    return NULL;
}
/*====================================================================*/
int strlcmp_case(const char *s1, const char *s2, int len)
{
    char c1, c2;
    
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    int diff = ('a' - 'A');
    
    for (; len > 0; --len, ++s1, ++s2)
    {
        c1 = *s1;
        c2 = *s2;
        if (!c1 && !c2) return 0;
        
        c1 += (c1 >= 'A') && (c1 <= 'Z') ? diff : 0;
        c2 += (c2 >= 'A') && (c2 <= 'Z') ? diff : 0;
        
        if (c1 != c2) return (c1 - c2);
    }

    return 0;
}
/*====================================================================*/
int get_int_method(char *s)
{
    if (!memcmp(s, "GET", 3))
        return M_GET;
    else if (!memcmp(s, "POST", 4))
        return M_POST;
    else if (!memcmp(s, "HEAD", 4))
        return M_HEAD;
    else if (!memcmp(s, "OPTIONS", 7))
        return M_OPTIONS;
    else if (!memcmp(s, "CONNECT", 7))
        return M_CONNECT;
    else
        return 0;
}
/*====================================================================*/
const char *get_str_method(int i)
{
    if (i == M_GET)
        return "GET";
    else if (i == M_POST)
        return "POST";
    else if (i == M_HEAD)
        return "HEAD";
    else if (i == M_OPTIONS)
        return "OPTIONS";
    else if (i == M_CONNECT)
        return "CONNECT";
    return "";
}
/*====================================================================*/
int get_int_http_prot(char *s)
{
    if (!memcmp(s, "HTTP/1.1", 8))
        return HTTP11;
    else if (!memcmp(s, "HTTP/1.0", 8))
        return HTTP10;
    else if (!memcmp(s, "HTTP/0.9", 8))
        return HTTP09;
    else if (!memcmp(s, "HTTP/2", 6))
        return HTTP2;
    else
        return 0;
}
/*====================================================================*/
const char *get_str_http_prot(int i)
{

    if (i == HTTP11)
        return "HTTP/1.1";
    else if (i == HTTP10)
            return "HTTP/1.0";
    else if (i == HTTP09)
            return "HTTP/0.9";
    else if (i == HTTP2)
            return "HTTP/2";
    return "";
}
/*====================================================================*/
const char *istextfile(const char *path)
{
    FILE *f;
    int cnt, i;
    int c;
    char s[128];
    char chr_txt[] = "`~!@#$%^&*()-_=+\\|[]{};:'\",<.>/?"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789"
                    "\x09\x20\x0a\x0d";

    f = fopen(path, "r");
    if(f == NULL)
    {
        printf("error fopen\n");
        return "";
    }

    for(cnt = 0; ((c = fgetc(f)) >= 0) && (cnt < 128); cnt++)
    {
        if((c < ' ') && (c != '\t') && (c != '\r') && (c != '\n'))
        {
            fclose(f);
            return "";
        }
    }

    fseek(f, 0, SEEK_SET);
    fgets(s, sizeof(s), f);
    if(strstr(s, "html>") || strstr(s, "HTML>") || strstr(s, "<html") || strstr(s, "<HTML"))
    {
        fclose(f);
        return "text/html";
    }

    fseek(f, 0, SEEK_SET);
    for(cnt = 0; ((c = fgetc(f)) >= 0) && (cnt < 32); cnt++)
    {
        if((c < ' ') && (c != '\t') && (c != '\r') && (c != '\n'))
        {
            fclose(f);
            return "";
        }

        if(c < 0x7f)
        {
            if(!strchr(chr_txt, c))
            {
                fclose(f);
                return "";
            }
            continue;
        }
        else if((c >= 0xc0) && (c <= 0xdf))
        {
            for(i = 1; i < 2; i++)
            {
                c = fgetc(f);
                if(!((c >= 0x80) && (c <= 0xbf)))
                {
                    fclose(f);
                    goto end;
                    return "";
                }
            }
            continue;
        }
        else if((c >= 0xe0) && (c <= 0xef))
        {
            for(i = 1; i < 3; i++)
            {
                c = fgetc(f);
                if(!((c >= 0x80) && (c <= 0xbf)))
                {
                    fclose(f);
                    goto end;
                    return "";
                }
            }
            continue;
        }
        else if((c >= 0xf0) && (c <= 0xf7))
        {
            for(i = 1; i < 4; i++)
            {
                c = fgetc(f);
                if(!((c >= 0x80) && (c <= 0xbf)))
                {
                    fclose(f);
                    goto end;
                    return "";
                }
            }
            continue;
        }
        else if((c >= 0xf8) && (c <= 0xfb))
        {
            for(i = 1; i < 5; i++)
            {
                c = fgetc(f);
                if(!((c >= 0x80) && (c <= 0xbf)))
                {
                    fclose(f);
                    goto end;
                    return "";
                }
            }
            continue;
        }
        else if((c >= 0xfc) && (c <= 0xfd))
        {
            for(i = 1; i < 6; i++)
            {
                c = fgetc(f);
                if(!((c >= 0x80) && (c <= 0xbf)))
                {
                    fclose(f);
                    goto end;
                    return "";
                }
            }
            continue;
        }
        else
        {
            fclose(f);
            goto end;
            return "";
        }
        fclose(f);
        goto end;
        return "";
    }
    fclose(f);
    return "text/plain; charset=UTF-8";
end:
    return "text/plain; charset=cp1251";
}
/*====================================================================*/
const char *ismediafile(const char *path)
{
    FILE *f;
    int size = 0;
    char s[64];

    f = fopen(path, "r");
    if(f == NULL)
    {
        printf("error fopen\n");
        return "";
    }
    size = fread(s, 1, 63, f);
    fclose(f);
    if(size <= 0)
        return "";

    if(!memcmp(s, "\x30\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C", 16))
    {
        return "video/x-ms-wmv";
    }

    if(s[0] == 'C' || s[0] == 'F')
    {
        if(!memcmp(s + 1, "WS", 2))
        {
            if(s[3] >= 0x02 && s[3] <= 0x15)
                return "application/x-shockwave-flash";
        }
    }

    if(!memcmp(s, "RIFF", 4))                              // avi, wav
    {
        if(!memcmp(s + 8, "AVI LIST", 8)) return "video/x-msvideo";
        else if(!memcmp(s + 8, "WAVE", 4)) return "audio/x-wav";
        else return "";
    }

    if((!memcmp(s, "\xff\xf1", 2)) || (!memcmp(s, "\xff\xf9", 2))) return "audio/aac";
    if(!memcmp(s + 8, "AIFF", 4)) return "audio/aiff";
    if(!memcmp(s, "fLaC", 4)) return "audio/flac";
    if(!memcmp(s, "#!AMR", 4)) return "audio/amr";
    if(!memcmp(s, "ID3", 3)) return "audio/mpeg";          // mp3
    if(!memcmp(s, "MThd", 4)) return "audio/midi";
    if(!memcmp(s, "OggS", 4)) //return "audio/ogg";
    {
        if(!memcmp(s + 28, "\x01""vorbis", 7) || !memcmp(s + 28, "\x7f""FLAC", 5))
            return "audio/ogg";
    }

    if(*s == '\xff')
    {
        if(memchr("\xE2\xE3\xF2\xF3\xFA\xFB", s[1], 6))
        {
            if (((s[2] & 0xF0) != 0xF0) && ((s[2] & 0xF0) != 0x00) && ((s[2] & 0x0F) < 0x0C))
                return "audio/mpeg";
        }
    }
//----------------------------------------------------------------------
    if(!memcmp(s, "FLV", 3)) return "video/x-flv";            // flv
    if(!memcmp(s + 4, "ftyp3gp", 6)) return "video/3gpp"; // 3gp
    if(!memcmp(s + 4, "ftypqt", 6)) return "video/quicktime"; // mov
    if(!memcmp(s + 4, "ftyp", 4)) return "video/mp4";         // mp4
    if(!memcmp(s, "\x1A\x45\xDF\xA3", 4))    // \x93\x42\x82\x88
        return "video/x-matroska";                            // mkv
    if(!memcmp(s, "OggS", 4)) return "video/ogg";
    if(!memcmp(s + 4, "moov", 4)) return "video/quicktime";
    if(!memcmp(s, "\x00\x00\x01\xBA", 4)) return "video/mpeg";
    return "";
}
/*=========================== file_extens ============================*/
const char *content_type(const char *s)
{
    const char *p = strrchr(s, '.');

    if(!p)
    {
        goto end;
    }

    //       video
    if(!strlcmp_case(p, ".ogv", 4)) return "video/ogg";
    else if(!strlcmp_case(p, ".mp4", 4)) return "video/mp4";
    else if(!strlcmp_case(p, ".avi", 4)) return "video/x-msvideo";
    else if(!strlcmp_case(p, ".mov", 4)) return "video/quicktime";
    else if(!strlcmp_case(p, ".mkv", 4)) return "video/x-matroska";
    else if(!strlcmp_case(p, ".flv", 4)) return "video/x-flv";
    else if(!strlcmp_case(p, ".mpeg", 5) || !strlcmp_case(p, ".mpg", 4)) return "video/mpeg";
    else if(!strlcmp_case(p, ".asf", 4)) return "video/x-ms-asf";
    else if(!strlcmp_case(p, ".wmv", 4)) return "video/x-ms-wmv";
    else if(!strlcmp_case(p, ".swf", 4)) return "application/x-shockwave-flash";
    else if(!strlcmp_case(p, ".3gp", 4)) return "video/video/3gpp";

    //       sound
    else if(!strlcmp_case(p, ".mp3", 4)) return "audio/mpeg";
    else if(!strlcmp_case(p, ".wav", 4)) return "audio/x-wav";
    else if(!strlcmp_case(p, ".ogg", 4)) return "audio/ogg";
    else if(!strlcmp_case(p, ".pls", 4)) return "audio/x-scpls";
    else if(!strlcmp_case(p, ".aac", 4)) return "audio/aac";
    else if(!strlcmp_case(p, ".aif", 4)) return "audio/x-aiff";
    else if(!strlcmp_case(p, ".ac3", 4)) return "audio/ac3";
    else if(!strlcmp_case(p, ".voc", 4)) return "audio/x-voc";
    else if(!strlcmp_case(p, ".flac", 5)) return "audio/flac";
    else if(!strlcmp_case(p, ".amr", 4)) return "audio/amr";
    else if(!strlcmp_case(p, ".au", 3)) return "audio/basic";

    //       image
    else if(!strlcmp_case(p, ".gif", 4)) return "image/gif";
    else if(!strlcmp_case(p, ".svg", 4) || !strlcmp_case(p, ".svgz", 5)) return "image/svg+xml";
    else if(!strlcmp_case(p, ".png", 4)) return "image/png";
    else if(!strlcmp_case(p, ".ico", 4)) return "image/vnd.microsoft.icon";
    else if(!strlcmp_case(p, ".jpeg", 5) || !strlcmp_case(p, ".jpg", 4)) return "image/jpeg";
    else if(!strlcmp_case(p, ".djvu", 5) || !strlcmp_case(p, ".djv", 4)) return "image/vnd.djvu";
    else if(!strlcmp_case(p, ".tiff", 5)) return "image/tiff";
    //       text
    else if(!strlcmp_case(p, ".txt", 4)) return istextfile(s);
    else if(!strlcmp_case(p, ".html", 5) || !strlcmp_case(p, ".htm", 4) || !strlcmp_case(p, ".shtml", 6)) return "text/html";
    else if(!strlcmp_case(p, ".css", 4)) return "text/css";

    //       application
    else if(!strlcmp_case(p, ".pdf", 4)) return "application/pdf";
    else if(!strlcmp_case(p, ".gz", 3)) return "application/gzip";
end:
    p = ismediafile(s);
    if(p)
        if(strlen(p)) return p;

    p = istextfile(s);
    if(p)
        if(strlen(p)) return p;

    return "";
}
/*====================================================================*/
int clean_path(char *path)
{
    int slash = 0, comma = 0, cnt = 0;
    char *p = path, ch;

    while((ch = *path))
    {
        if(ch == '/')
        {
            if (!slash)
            {
                *p++ = *path++;
                slash = 1;
                ++cnt;
            }
            else path++;
            comma = 0;
        }
        else if (ch == '.')
        {
            if (comma && slash)
            {
                *p = 0;
                return 0;
            }

            comma = 1;
            
            if (slash) path++;
            else
            {
                *p++ = *path++;
                ++cnt;
            }
        }
        else
        {
            if (comma && slash)
            {
                *p = 0;
                return 0;
            }
            *p++ = *path++;
            slash = comma = 0;
            ++cnt;
        }
    }
    *p = 0;
    return cnt;
}
//======================================================================
const char *base_name(const char *path)
{
    const char *p;
    
    if(!path)
        return NULL;

    p = strrchr(path, '/');
    if(p)
    {
        return p + 1;
    }

    return path;
}
//======================================================================
int parse_startline_request(Connect *req, char *s, int len)
{
    char *p, tmp[16];
    //----------------------------- method -----------------------------
    p = tmp;
    int i = 0, n = 0;
    while ((i < len) && (n < (int)sizeof(tmp)))
    {
        char ch = s[i++];
        if ((ch != '\x20') && (ch != '\r') && (ch != '\n'))
            p[n++] = ch;
        else
            break;
    }
    p[n] = 0;

    req->reqMethod = get_int_method(tmp);
    if (!req->reqMethod) return -RS400;
    //------------------------------- uri ------------------------------
    char ch = s[i];
    if ((ch == '\x20') || (ch == '\r') || (ch == '\n'))
    {
        return -RS400;
    }

    req->uri = s + i;
    while (i < len)
    {
        char ch = s[i];
        if ((ch == '\x20') || (ch == '\r') || (ch == '\n') || (ch == 0))
            break;
        ++i;
    }

    if (s[i] == '\r')// HTTP/0.9
    {
        req->httpProt = HTTP09;
        s[i] = 0;
        return 0;
    }
    
    if (s[i] == '\x20')
        s[i++] = 0;
    else
        return -RS400;
    //------------------------------ version ---------------------------
    ch = s[i];
    if ((ch == '\x20') || (ch == '\r') || (ch == '\n'))
        return -RS400;

    p = tmp;
    n = 0;
    while ((i < len) && (n < (int)sizeof(tmp)))
    {
        char ch = s[i++];
        if ((ch != '\x20') && (ch != '\r') && (ch != '\n'))
            p[n++] = ch;
        else
            break;
    }
    p[n] = 0;
    
    if(!(req->httpProt = get_int_http_prot(tmp)))
    {
        print_err(req, "<%s:%d> Error version protocol\n", __func__, __LINE__);
        req->httpProt = HTTP11;
        return -RS400;
    }

    return 0;
}
/*====================================================================*/
int parse_headers(Connect *req, char *s, int len)
{
    int n;
    char *pName = s, *pVal, *p;

    p = (char*)memchr(pName, '\r', len);
    if (!p) return -1;
    *p = 0;
    
    if(!(p = (char*)memchr(pName, ':', len)))
    {
        print_err(req, "<%s:%d> Error: ':' not found\n", __func__, __LINE__);
        return -RS400;
    }
    *p = 0;

    n = strspn(p + 1, "\x20");
    pVal = p + 1 + n;
    
    if(!strlcmp_case(pName, "connection", 32))
    {
        req->req_hdrs.iConnection = req->req_hdrs.countReqHeaders;
        if(strstr_case(pVal, "keep-alive"))
            req->connKeepAlive = 1;
        else
            req->connKeepAlive = 0;
    }
    else if(!strlcmp_case(pName, "host", 32))
    {
        req->req_hdrs.iHost = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "range", 32))
    {
        char *p = strchr(pVal, '=');
        if (p)
            req->sRange = p + 1;
        else
            req->sRange = NULL;
        
        req->req_hdrs.iRange = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "If-Range", 32))
    {
        req->req_hdrs.iIfRange = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "referer", 32))
    {
        req->req_hdrs.iReferer = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "user-agent", 32))
    {
        req->req_hdrs.iUserAgent = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "upgrade", 32))
    {
        req->req_hdrs.iUpgrade = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "content-length", 32))
    {
        req->req_hdrs.reqContentLength = atoll(pVal);
        req->req_hdrs.iReqContentLength = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "content-type", 32))
    {
        req->req_hdrs.iReqContentType = req->req_hdrs.countReqHeaders;
    }
    else if(!strlcmp_case(pName, "accept-encoding", 32))
    {
        req->req_hdrs.iAcceptEncoding = req->req_hdrs.countReqHeaders;
    }

    req->req_hdrs.Name[req->req_hdrs.countReqHeaders] = pName;
    req->req_hdrs.Value[req->req_hdrs.countReqHeaders++] = pVal;

    req->req_hdrs.Name[req->req_hdrs.countReqHeaders] = NULL;
    req->req_hdrs.Value[req->req_hdrs.countReqHeaders] = NULL;

    return 0;
}
/*====================================================================*/
const char *str_err(int i)
{
    switch(i)
    {
        case 0:
            return "Success";
        case EPERM: // 1
            return "Operation not permitted";
        case ENOENT: // 2
            return "No such file or directory";
        case EINTR: // 4
            return "Interrupted system call";
        case EIO: // 5
            return "I/O error";
        case ENXIO: // 6
            return "No such device or address";
        case E2BIG: // 7
            return "Argument list too long";
        case ENOEXEC: // 8
            return "Exec format error";
        case EBADF: // 9
            return "Bad file number";
        case ECHILD: // 10
            return "No child processes";
        case EAGAIN: // 11
            return "Try again";
        case ENOMEM: // 12
            return "Out of memory";
        case EACCES: // 13
            return "Permission denied";
        case EFAULT: // 14
            return "Bad address";
        case ENOTBLK: // 15
            return "Block device required";
        case EBUSY: // 16
            return "Device or resource busy";
        case EEXIST: // 17
            return "File exists";
        case EXDEV: // 18
            return "Cross-device link";
        case ENODEV: // 19
            return "No such device";
        case ENOTDIR: // 20
            return "Not a directory";
        case EISDIR: // 21
            return "Is a directory";
        case EINVAL: // 22
            return "Invalid argument";
        case ENFILE: // 23
            return "File table overflow";
        case EMFILE: // 24
            return "Too many open files";
        case ENOTTY: // 25
            return "Not a typewriter";
        case ETXTBSY: // 26
            return "Text file busy";
        case EFBIG: // 27
            return "File too large";
        case ENOSPC: // 28
            return "No space left on device";
        case ESPIPE: // 29
            return "Illegal seek";
        case EROFS: // 30
            return "Read-only file system";
        case EMLINK: // 31
            return "Too many links";
        case EPIPE: // 32
            return "Broken pipe";
        case ENAMETOOLONG: // 36
            return "File name too long";
        case ECONNABORTED: // 53 Ctrl+C OpenBSD 
            return "Software caused connection abort";
        case EILSEQ: // 84
            return "Illegal byte sequence";
        case ENOTSOCK: // 88
            return "Socket operation on non-socket";
        case EDESTADDRREQ: // 89
            return "Destination address required";
        case EMSGSIZE: // 90
            return "Message too long";
        case EPROTOTYPE: // 91
            return "Protocol wrong type for socket";
        case ENOPROTOOPT: // 92
            return "Protocol not available";
        case EPROTONOSUPPORT: // 93
            return "Protocol not supported";
        case ESOCKTNOSUPPORT: // 94
            return "Socket type not supported";
        case EOPNOTSUPP: // 95
            return "Operation not supported on transport endpoint";
        case EPFNOSUPPORT: // 96
            return "Protocol family not supported";
        case EAFNOSUPPORT: // 97
            return "Address family not supported by protocol";
        case EADDRINUSE: // 98
            return "Address already in use";
        case EADDRNOTAVAIL: // 99
            return "Cannot assign requested address";
        case ENETDOWN: // 100
            return "Network is down";
        case ENETUNREACH: // 101
            return "Network is unreachable";
        case ECONNRESET: // 104
            return "Connection reset by peer";
        case ENOBUFS: // 105
            return "No buffer space available";
        case ENOTCONN: // 107
            return "Transport endpoint is not connected";
        case ETIMEDOUT: // 110
            return "Connection timed out";
        case ECONNREFUSED: // 111
            return "Connection refused";
        
        default:
            return "?";
    }
    return "";
}
