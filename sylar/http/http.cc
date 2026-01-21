#include "http.h"
#include "basic/log.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

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



// http basic class 
HTTP::HTTP(uint8_t version, bool close)
    : m_close(close)
    , m_version(version)
{}

std::string HTTP::getHeader(const std::string& key, const std::string& val)
{
    auto it = m_headers.find(key);
    if(it != m_headers.end())
    {
        return it->second;
    }
    return val;
}

void HTTP::setHeader(const std::string& key, const std::string& val)
{
    m_headers[key] = val;
}

void HTTP::delHeader(std::string& key)
{
    m_headers.erase(key);
}

bool HTTP::hasHeader(std::string& key, std::string* val)
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






// http request class 
HttpRequest::HttpRequest(uint8_t version, bool close, HttpMethod method)
    : HTTP(version, close),
      m_method(method)
{
    m_path = "/";
    m_fragment = "";
    m_path = "";
    m_query = "";
}

void HttpRequest::setParam(const std::string& key, const std::string& val)
{
    m_params[key] = val;
}

void HttpRequest::setCookie(const std::string& key, const std::string& val)
{
    m_cookies[key] = val;
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

void HttpRequest::delParam(std::string& key)
{
    m_params.erase(key);
}

void HttpRequest::delCookie(std::string& key)
{
    m_cookies.erase(key);
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

bool HttpRequest::updateHeader()
{
    setHeader("connection", (m_close ? "close" : "keep-alive"));
    if(!m_body.empty()) {setHeader("Content-Length", std::to_string(m_body.size())); }
    return true; 
}

std::ostream& HttpRequest::dump(std::ostream& os) const
{
    // request 
    os << HttpMethodToString(m_method) << " " << m_path
       << (m_query.empty() ? "" : "?") << m_query 
       << (m_fragment.empty() ? "" : "#") << m_fragment
       << " HTTP/" 
       << (uint32_t)(m_version >> 4) << "." << (uint32_t)(m_version & 0x0F)
       << "\r\n";
    // header
    for(auto& i : m_headers)
    {
        os << i.first << ": " << i.second << "\r\n";
    }
    os << "\r\n";
    // body
    os << m_body;
    return os;
}



// http response class
HttpResponse::HttpResponse(uint8_t version, bool close)
    : HTTP(version, close)
{
    setStatus(HttpStatus::OK);
}

void HttpResponse::setStatus(HttpStatus status)
{
    m_status = status;
    m_reason = HttpStatusToString(status);
}

void HttpResponse::setReason(HttpStatus status)
{
    setStatus(status);
}

bool HttpResponse::updateHeader()
{
    setHeader("connection", (m_close ? "close" : "keep-alive"));
    if(!m_body.empty()) {
        // M_SYLAR_LOG_DEBUG(g_logger) << 
        // std::cout << "content-Length:" << std::to_string(m_body.size());
        setHeader("Content-Length", std::to_string(m_body.size())); 
    }
    if(!m_reason.empty()) {setReason(m_status);}
    return true; 
}

std::ostream& HttpResponse::dump(std::ostream& os) const
{
    // request 
    os  << "HTTP/" << (uint32_t)(m_version >> 4) << "." << (uint32_t)(m_version & 0x0F) << " "
        << m_status << " "
        << (m_reason.empty() ? HttpStatusToString(m_status) : m_reason)
        << "\r\n"; 
    // header
    for(auto& i : m_headers)
    {
        os << i.first << ": " << i.second << "\r\n";
    }
    os << "\r\n";
    // body
    os << m_body;
    return os;
}

}
}