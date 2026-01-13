#pragma once
#include <boost/lexical_cast.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <iostream>
#include <map>
#include <ostream>
#include <string>
#include <yaml-cpp/node/type.h>




namespace m_sylar{
namespace http{
/* Request Methods */
#define HTTP_METHOD_MAP(XX)             \
    XX(0,  DELETE,      DELETE)         \
    XX(1,  GET,         GET)            \
    XX(2,  HEAD,        HEAD)           \
    XX(3,  POST,        POST)           \
    XX(4,  PUT,         PUT)            \
    /* pathological */                  \
    XX(5,  CONNECT,     CONNECT)        \
    XX(6,  OPTIONS,     OPTIONS)        \
    XX(7,  TRACE,       TRACE)          \
    /* WebDAV */                        \
    XX(8,  COPY,        COPY)           \
    XX(9,  LOCK,        LOCK)           \
    XX(10, MKCOL,       MKCOL)          \
    XX(11, MOVE,        MOVE)           \
    XX(12, PROPFIND,    PROPFIND)       \
    XX(13, PROPPATCH,   PROPPATCH)      \
    XX(14, SEARCH,      SEARCH)         \
    XX(15, UNLOCK,      UNLOCK)         \
    XX(16, BIND,        BIND)           \
    XX(17, REBIND,      REBIND)         \
    XX(18, UNBIND,      UNBIND)         \
    XX(19, ACL,         ACL)            \
    /* subversion */                    \
    XX(20, REPORT,      REPORT)         \
    XX(21, MKACTIVITY,  MKACTIVITY)     \
    XX(22, CHECKOUT,    CHECKOUT)       \
    XX(23, MERGE,       MERGE)          \
    /* upnp */                          \
    XX(24, MSEARCH,     M-SEARCH)       \
    XX(25, NOTIFY,      NOTIFY)         \
    XX(26, SUBSCRIBE,   SUBSCRIBE)      \
    XX(27, UNSUBSCRIBE, UNSUBSCRIBE)    \
    /* RFC-5789 */                      \
    XX(28, PATCH,       PATCH)          \
    XX(29, PURGE,       PURGE)          \
    /* CalDAV */                        \
    XX(30, MKCALENDAR,  MKCALENDAR)     \
    /* RFC-2068, section 19.6.1.2 */    \
    XX(31, LINK,        LINK)           \
    XX(32, UNLINK,      UNLINK)         \
    /* icecast */                       \
    XX(33, SOURCE,      SOURCE)         \

/* Status Codes */
#define HTTP_STATUS_MAP(XX)                                                     \
    XX(100, CONTINUE,                        Continue)                          \
    XX(101, SWITCHING_PROTOCOLS,             Switching Protocols)               \
    XX(102, PROCESSING,                      Processing)                        \
    XX(200, OK,                              OK)                                \
    XX(201, CREATED,                         Created)                           \
    XX(202, ACCEPTED,                        Accepted)                          \
    XX(203, NON_AUTHORITATIVE_INFORMATION,   Non-Authoritative Information)     \
    XX(204, NO_CONTENT,                      No Content)                        \
    XX(205, RESET_CONTENT,                   Reset Content)                     \
    XX(206, PARTIAL_CONTENT,                 Partial Content)                   \
    XX(207, MULTI_STATUS,                    Multi-Status)                      \
    XX(208, ALREADY_REPORTED,                Already Reported)                  \
    XX(226, IM_USED,                         IM Used)                           \
    XX(300, MULTIPLE_CHOICES,                Multiple Choices)                  \
    XX(301, MOVED_PERMANENTLY,               Moved Permanently)                 \
    XX(302, FOUND,                           Found)                             \
    XX(303, SEE_OTHER,                       See Other)                         \
    XX(304, NOT_MODIFIED,                    Not Modified)                      \
    XX(305, USE_PROXY,                       Use Proxy)                         \
    XX(307, TEMPORARY_REDIRECT,              Temporary Redirect)                \
    XX(308, PERMANENT_REDIRECT,              Permanent Redirect)                \
    XX(400, BAD_REQUEST,                     Bad Request)                       \
    XX(401, UNAUTHORIZED,                    Unauthorized)                      \
    XX(402, PAYMENT_REQUIRED,                Payment Required)                  \
    XX(403, FORBIDDEN,                       Forbidden)                         \
    XX(404, NOT_FOUND,                       Not Found)                         \
    XX(405, METHOD_NOT_ALLOWED,              Method Not Allowed)                \
    XX(406, NOT_ACCEPTABLE,                  Not Acceptable)                    \
    XX(407, PROXY_AUTHENTICATION_REQUIRED,   Proxy Authentication Required)     \
    XX(408, REQUEST_TIMEOUT,                 Request Timeout)                   \
    XX(409, CONFLICT,                        Conflict)                          \
    XX(410, GONE,                            Gone)                              \
    XX(411, LENGTH_REQUIRED,                 Length Required)                   \
    XX(412, PRECONDITION_FAILED,             Precondition Failed)               \
    XX(413, PAYLOAD_TOO_LARGE,               Payload Too Large)                 \
    XX(414, URI_TOO_LONG,                    URI Too Long)                      \
    XX(415, UNSUPPORTED_MEDIA_TYPE,          Unsupported Media Type)            \
    XX(416, RANGE_NOT_SATISFIABLE,           Range Not Satisfiable)             \
    XX(417, EXPECTATION_FAILED,              Expectation Failed)                \
    XX(421, MISDIRECTED_REQUEST,             Misdirected Request)               \
    XX(422, UNPROCESSABLE_ENTITY,            Unprocessable Entity)              \
    XX(423, LOCKED,                          Locked)                            \
    XX(424, FAILED_DEPENDENCY,               Failed Dependency)                 \
    XX(426, UPGRADE_REQUIRED,                Upgrade Required)                  \
    XX(428, PRECONDITION_REQUIRED,           Precondition Required)             \
    XX(429, TOO_MANY_REQUESTS,               Too Many Requests)                 \
    XX(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large)   \
    XX(451, UNAVAILABLE_FOR_LEGAL_REASONS,   Unavailable For Legal Reasons)     \
    XX(500, INTERNAL_SERVER_ERROR,           Internal Server Error)             \
    XX(501, NOT_IMPLEMENTED,                 Not Implemented)                   \
    XX(502, BAD_GATEWAY,                     Bad Gateway)                       \
    XX(503, SERVICE_UNAVAILABLE,             Service Unavailable)               \
    XX(504, GATEWAY_TIMEOUT,                 Gateway Timeout)                   \
    XX(505, HTTP_VERSION_NOT_SUPPORTED,      HTTP Version Not Supported)        \
    XX(506, VARIANT_ALSO_NEGOTIATES,         Variant Also Negotiates)           \
    XX(507, INSUFFICIENT_STORAGE,            Insufficient Storage)              \
    XX(508, LOOP_DETECTED,                   Loop Detected)                     \
    XX(510, NOT_EXTENDED,                    Not Extended)                      \
    XX(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required)   \


enum HttpMethod
{
#define XX(num, name, string) name = num ,
    HTTP_METHOD_MAP(XX) 
#undef XX 
    HTTP_INVALID_METHOD = 255
};


enum HttpStatus
{
#define XX(code, name, desc) name = code,
    HTTP_STATUS_MAP(XX)
#undef XX
};


HttpMethod StringToHttpMethod(const std::string& code);     
HttpMethod StringToHttpMethod(const char* code);
const char* HttpMethodToString(const HttpMethod& method);
const char* HttpStatusToString(const HttpStatus& status);


struct CaseInsensitive  
{
    bool operator()(const std::string& lhs, const std::string& rhs) const ;
};




class HTTP
{
public:
    using ptr = std::shared_ptr<HTTP>;
    using MapType = std::map<std::string, std::string, CaseInsensitive>;

    HTTP(uint8_t version = 0x11, bool close = true);

    uint8_t getVersion() const {return m_version; }
    HttpStatus getStatus() const {return HttpStatus::HTTP_VERSION_NOT_SUPPORTED; }
    const std::string& getBody() const {return m_body; }
    MapType& getHeaders()  {return m_headers; }
    std::string getHeader(std::string& key, std::string val = "");

    // void setStatus(HttpStatus)
    void setVersion(uint8_t version) {m_version = version; }
    void setBody (std::string body) { m_body = body;}
    void setHeaders (MapType& header) {m_headers = header; }
    void setHeader(const std::string& key, const std::string& val);

    void delHeader(std::string& key);

    bool hasHeader(std::string& key, std::string* val = nullptr);       // val will be set to value if value exists

    std::ostream& updateAndDump(std::ostream& os) {updateHeader(); return dump(os); }         // updte http header and dump
    virtual bool updateHeader() = 0;
    virtual std::ostream& dump(std::ostream& os) const = 0;       // it may generate some headers like content-Length

    // check and return by value if data exists
    template<class T>   
    bool checkGetHeaderAs(const std::string& key, T& value, const T& def = T())   
    {
        return checkGetAs(m_headers, key, value, def);
    }

    // return directly if data exists, or return default value T()
    template<class T>
    T GetHeaderAs(const MapType& m, const std::string& key, const T& def = T())
    {
        return getAs(m, key, def);
    }

protected: 
template<class T>
    bool checkGetAs(const MapType& m, const std::string& key, T& value, const T& def = T())
    {
        auto it = m.find(key);
        if(it == m.end())
        {
            value = def;
            return false;
        }

        try 
        {
            value = boost::lexical_cast<T>(it->second);
            return true;
        }
        catch (...)
        {
            value = def;
        }
        return false;
    }

template<class T>
    T getAs(const MapType& m, const std::string& key, const T& def = T())
    {  
        auto it = m.find(key);
        if(it == m.end())
        {
            return def;
        }
        try
        {
            return boost::lexical_cast<T>(it->second);
        }
        catch(...)
        {

        }
        return def;
    }

protected:
    bool m_close;
    uint8_t m_version;
    MapType m_headers;
    std::string m_body;
};



class HttpRequest : public HTTP
{
public:
    using ptr = std::shared_ptr<HttpRequest>;

    HttpRequest(uint8_t version = 0x11, bool close = true, HttpMethod method = HttpMethod::GET);
    HttpMethod getMethod() const {return m_method; }

    void setMethod(HttpMethod method) {m_method = method; }
    void setParams (MapType& params) {m_params = params; }
    void setCookies (MapType& cookie) {m_cookies = cookie; }
    void setParam(const std::string& key, const std::string& val);
    void setCookie(const std::string& key, const std::string& val);

    void setPath (std::string path) { m_path = path;}
    void setQuery (std::string query) { m_query = query;}
    void setFragment (std::string fragment) { m_fragment = fragment;}

    MapType& getParams() {return m_params; }
    MapType& getCookies() {return m_cookies; }
    const std::string& getPath() const {return m_path; }
    const std::string& getQuery() const {return m_query; }
    std::string getParam(std::string& key, std::string val = "");
    std::string getCookie(std::string& key, std::string val = "");

    void delParam(std::string& key);
    void delCookie(std::string& key);

    bool hasParam(std::string& key, std::string* val = nullptr);
    bool hasCookie(std::string& key, std::string* val = nullptr);

    bool updateHeader() override;
    std::ostream& dump(std::ostream& os) const override;

    template<class T>               // check and return by value if data exists
    bool checkGetParamAs(const std::string& key, T& value, const T& def = T())
    {
        return checkGetAs(m_params, key, value, def);
    }

    template<class T> 
    bool checkGetCookieAs(const std::string& key, T& value, const T& def = T())
    {
        return checkGetAs(m_cookies, key, value, def);
    }

    template<class T>               // return directly if data exists, or return default value T()
    T GetParamAs(const std::string& key, const T& def = T())
    {
        return getAs(m_params, key, def);
    }

    template<class T>
    T GetCookieAs(const std::string& key, const T& def = T())
    {
        return getAs(m_cookies, key, def);
    }

private:
    HttpMethod m_method;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    MapType m_params;
    MapType m_cookies;
};



class HttpResponse : public HTTP
{
public:
    using ptr = std::shared_ptr<HttpResponse>;

    HttpResponse(uint8_t version = 0x11, bool close = true);

    HttpStatus getStatus() const {return m_status; }
    std::string getReason() const {return m_reason; }

    void setStatus(HttpStatus status);
    void setReason(HttpStatus status);

    bool updateHeader() override;
    std::ostream& dump(std::ostream& os) const override;

private:
    HttpStatus m_status;
    std::string m_reason;   
};

}
}