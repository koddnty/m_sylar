#include "http.h"
#include <cstring>

namespace m_sylar
{
namespace http
{


// http codes cast
HttpMethod StringToHttpMethod(const std::string& code)
{
#define XX(num, name, string)                               \
    if (strcmp(#string, code.c_str()) == 0)                 \
    {                                                       \
        return HttpMethod::name;                             \
    }                                                       
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::HTTP_INVALID_METHOD;
}
     
HttpMethod StringToHttpMethod(const char* code)
{
#define XX(num, name, string)                               \
    if (strcmp(#string, code) == 0)                         \
    {                                                       \
        return HttpMethod::name;                             \
    }                                                       
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::HTTP_INVALID_METHOD;
}

const char* HttpMethodToString(const HttpMethod& method)
{
#define XX(num, name, string)           \
    if(HttpMethod::name == method)      \
    {                                   \
        return #string;                  \
    }                                   
    HTTP_METHOD_MAP(XX)
#undef XX
    return "Http invalid method";
}

const char* HttpStatusToString(const HttpStatus& status)
{
#define XX(num, name, string)           \
    if(HttpStatus::name == status)      \
    {                                   \
        return #string;                  \
    }                                   
    HTTP_STATUS_MAP(XX)
#undef XX
    return "Http invalid method";
}



bool CaseInsensitive::operator()(const std::string& lhs, const std::string& rhs) const 
{
    return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}



// http request class 
HttpRequest::HttpRequest(uint8_t version, bool close)
    : m_method(HttpMethod::GET), m_version(version)
    , m_close(close)
{}

std::string HttpRequest::getHeader(std::string& key, std::string val)
{
    auto it = m_headers.find(key);
    if(it != m_headers.end())
    {
        return it->second;
    }
    return val;
}

std::string HttpRequest::getParam(std::string& key, std::string val)
{
    auto it = m_params.find(key);
    if(it != m_params.end())
    {
        return it->second;
    }
    return val;
}

std::string HttpRequest::getCookie(std::string& key, std::string val)
{
    auto it = m_cookies.find(key);
    if(it != m_cookies.end())
    {
        return it->second;
    }
    return val;
}


void HttpRequest::setHeader(std::string& key, const std::string& val)
{
    m_headers[key] = val;
}

void HttpRequest::setParam(std::string& key, const std::string& val)
{
    m_params[key] = val;
}

void HttpRequest::setCookie(std::string& key, const std::string& val)
{
    m_cookies[key] = val;
}


void HttpRequest::delHeader(std::string& key)
{
    m_headers.erase(key);
}

void HttpRequest::delParam(std::string& key)
{
    m_params.erase(key);
}

void HttpRequest::delCookie(std::string& key)
{
    m_cookies.erase(key);
}


bool HttpRequest::hasHeader(std::string& key, std::string* val)
{
    auto it = m_headers.find(key);
    if(it != m_headers.end())
    {
        val = &(it->second);
        return true;
    }
    val = nullptr;
    return false;
}
       
bool HttpRequest::hasParam(std::string& key, std::string* val)
{
    auto it = m_params.find(key);
    if(it != m_params.end())
    {
        val = &(it->second);
        return true;
    }
    val = nullptr;
    return false;
}

bool HttpRequest::hasCookie(std::string& key, std::string* val)
{
    auto it = m_cookies.find(key);
    if(it != m_cookies.end())
    {
        val = &(it->second);
        return true;
    }
    val = nullptr;
    return false;
}

std::ostream& HttpRequest::dump(std::ostream& os)
{
    return os;
}



}
}