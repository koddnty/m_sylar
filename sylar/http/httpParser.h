#pragma once
#include <cstdint>
#include <memory>
#include "http.h"
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace m_sylar
{   
namespace http
{

class HttpRequestParser
{
public:
    enum Error : uint32_t
    {
        OK = 0x00000000,
        INVALID_METHOD = (1 << 1),
        INVALID_VERSION = (1 << 2),
        INVALID_HEADER = (1 << 3)
    };

public:
    using ptr = std::shared_ptr<HttpRequestParser>;

    HttpRequestParser();

    // http request paresr call back functions
    static void on_http_field(void *data, const char *field, size_t flen, const char *value, size_t vlen);
    static void on_request_method(void *data, const char *at, size_t length);
    static void on_request_uri(void *data, const char *at, size_t length);
    static void on_fragment(void *data, const char *at, size_t length);
    static void on_request_path(void *data, const char *at, size_t length);
    static void on_query_string(void *data, const char *at, size_t length);
    static void on_http_version(void *data, const char *at, size_t length);
    static void on_header_done(void *data, const char *at, size_t length);

    size_t execute(char *data, size_t len);
    int isError();
    int isFinished(http_parser *parser);

    HttpRequest::ptr getData() const { return m_requset;}

    void setError(Error e) {m_error = (Error)(m_error | e); }
    void cancelError(Error e) {m_error = (Error)(m_error & ~(e)); }
    Error gerError() {return m_error; }
private:
    http_parser* m_parser;
    HttpRequest::ptr m_requset;
    Error m_error = Error::OK;
};



class HttpResponseParser
{
public:
    enum Error : uint32_t
    {
        OK = 0x00000000,
        // INVALID_METHOD = (1 << 1),
        INVALID_VERSION = (1 << 2),
        INVALID_HEADER = (1 << 3)
    };

public:
    using ptr = std::shared_ptr<HttpResponseParser>;
    HttpResponseParser();

    // http request paresr callback functions
    static void on_field_cb(void *data, const char *field, size_t flen, const char *value, size_t vlen);
    static void on_reason_phrase(void *data, const char *at, size_t length);
    static void on_status_code(void *data, const char *at, size_t length);
    static void on_chunk_size(void *data, const char *at, size_t length);
    static void on_http_version(void *data, const char *at, size_t length);
    static void on_header_done(void *data, const char *at, size_t length);
    static void on_last_chunk(void *data, const char *at, size_t length);

    size_t execute(char *data, size_t len);
    int isError();
    int isFinished();

    HttpResponse::ptr getData() const { return m_response;}

    void setError(Error e) {m_error = (Error)(m_error | e); }
    void cancelError(Error e) {m_error = (Error)(m_error & ~(e)); }
    Error gerError() {return m_error; }

private:
    httpclient_parser* m_parser;
    HttpResponse::ptr m_response;
    Error m_error = Error::OK;
};


}
}