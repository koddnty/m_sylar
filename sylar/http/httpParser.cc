#include "basic/log.h"
#include "basic/config.h"
#include "httpParser.h"
#include "http/http.h"
#include "http/http11_parser.h"
#include "http/httpclient_parser.h"
#include <cstdint>
#include <cstring>
#include <string>


namespace m_sylar
{   
namespace http
{

static Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");
static ConfigVar<uint64_t>::ptr g_http_request_buffer_size = 
    m_sylar::configManager::Lookup("http.request.buffer_size", (uint64_t)1024 * 5, "size of a buffer");

static ConfigVar<uint64_t>::ptr g_http_request_max_size = 
    m_sylar::configManager::Lookup("http.request.max_message_size", (uint64_t)1024 * 1024 * 5, "max size of a request");

static uint64_t s_http_request_buffer_size = 0;
static uint64_t s_http_request_max_size = 0;

// update config values
struct _configInitilizer
{
    _configInitilizer()
    {
        s_http_request_buffer_size = g_http_request_buffer_size->getValue();
        s_http_request_max_size = g_http_request_max_size->getValue();
        g_http_request_buffer_size->addListener(1234 ,
            [](const uint64_t& ov, const uint64_t& nv){
                s_http_request_buffer_size = nv;
            }
        );
        g_http_request_max_size->addListener(1235 ,
            [](const uint64_t& ov, const uint64_t& nv){
                s_http_request_max_size = nv;
            }
        );
    }
};
static _configInitilizer __initier;



HttpRequestParser::HttpRequestParser()
{
    m_parser = new http_parser;
    m_requset.reset(new HttpRequest());
    http_parser_init(m_parser);
    m_parser->http_field = on_http_field;
    m_parser->request_method = on_request_method;
    m_parser->request_uri = on_request_uri;
    m_parser->fragment = on_fragment;
    m_parser->request_path = on_request_path;
    m_parser->query_string = on_query_string;
    m_parser->http_version = on_http_version;
    m_parser->header_done = on_header_done;

    m_parser->data = this;
}

void HttpRequestParser::on_http_field(void *data, const char *field, 
                size_t flen, const char *value, size_t vlen)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    if(flen <= 0)
    {
        M_SYLAR_LOG_WARN(g_logger) << "invalid header, field length is 0";
        parser->setError(Error::INVALID_HEADER); 
    }
    parser->getData()->setHeader(std::string(field, flen), std::string(value, vlen));
}

void HttpRequestParser::on_request_method(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    HttpMethod method = StringToHttpMethod(at);

    if(method == HttpMethod::HTTP_INVALID_METHOD)
    {
        M_SYLAR_LOG_WARN(g_logger) << "http request invalid_method" << std::string(at, length);
        parser->setError(Error::INVALID_METHOD);
    }
    
    parser->getData()->setMethod(method);
}

void HttpRequestParser::on_request_uri(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setFragment(std::string(at, length));
}

void HttpRequestParser::on_fragment(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setFragment(std::string(at, length));
}

void HttpRequestParser::on_request_path(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setPath(std::string(at, length));

}

void HttpRequestParser::on_query_string(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    parser->getData()->setQuery(std::string(at, length));
}

void HttpRequestParser::on_http_version(void *data, const char *at, size_t length)
{
    HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
    uint8_t v = 0;
    if(strncmp(at, "HTTP/1.1", length) == 0)
    {
        v = 0x11;
    }
    else if(strncmp(at, "HTTP/1.0", length) == 0) 
    {
        v = 0x10;
    }
    else if(strncmp(at, "HTTP/2.0", length) == 0)
    {
        v = 0x20;
    }
    else
    {
        M_SYLAR_LOG_WARN(g_logger) << "invalid http version" << std::string(at, length);
        parser->setError(Error::INVALID_VERSION);
        v = 0x00;
    }
    parser->getData()->setVersion(v);
}

void HttpRequestParser::on_header_done(void *data, const char *at, size_t length)
{
    //HttpRequestParser* parser = static_cast<HttpRequestParser*>(data);
}

size_t HttpRequestParser::execute(char *data, size_t len)
{
    size_t offset = http_parser_execute(m_parser, data, len, 0);
    memmove(data, data + offset, (len - offset));
    return offset;
}

int HttpRequestParser::isError()
{
    return m_error || http_parser_has_error(m_parser);
}

int HttpRequestParser::isFinished(http_parser *parser)
{
    return http_parser_is_finished(parser);
}



    
HttpResponseParser::HttpResponseParser()
{
    m_parser = new httpclient_parser;
    m_response.reset(new HttpResponse());
    httpclient_parser_init(m_parser);
    m_parser->reason_phrase = on_reason_phrase;
    m_parser->status_code = on_status_code;
    m_parser->chunk_size = on_chunk_size;
    m_parser->http_version = on_http_version;
    m_parser->header_done = on_header_done;
    m_parser->last_chunk = on_last_chunk;

    m_parser->data = this;
}

void HttpResponseParser::on_reason_phrase(void *data, const char *at, size_t length)
{

}

void HttpResponseParser::on_status_code(void *data, const char *at, size_t length)
{

}

void HttpResponseParser::on_chunk_size(void *data, const char *at, size_t length)
{

}

void HttpResponseParser::on_http_version(void *data, const char *at, size_t length)
{

}

void HttpResponseParser::on_header_done(void *data, const char *at, size_t length)
{

}

void HttpResponseParser::on_last_chunk(void *data, const char *at, size_t length)
{

}

size_t HttpResponseParser::execute(const char *data, size_t len, size_t off)
{

}

int HttpResponseParser::isError()
{

}

int HttpResponseParser::isFinished(http_parser *parser)
{

}


}
}