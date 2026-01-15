#include "http/http.h"
#include <cstddef>
#include <cstring>
#include <http/httpParser.h>
#include <basic/log.h>
#include <iostream>
#include <ostream>
#include <ranges>
#include <sstream>
#include <string>

static m_sylar::Logger::ptr g_logger = M_SYLAR_LOG_NAME("system");

char test_request[] = "POST / HTTP/1.1\r\n"
                            "Host: api.example.com\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: 29\r\n\r\n"
                            "{\"user\":\"test\",\"pass\":\"123\"}";
                            
char test_response[] = "HTTP/1.1 200 OK\r\n"
                       "Server: test-server\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: 31\r\n\r\n"
                       "{\"code\":200,\"msg\":\"success\",\"data\":{}}";

int testRequestParser()
{ 
    m_sylar::http::HttpRequestParser::ptr req_parser(new m_sylar::http::HttpRequestParser());
    size_t s = req_parser->execute(test_request, sizeof(test_request));

    std::ostringstream os;
    req_parser->getData()->updateAndDump(os);

    M_SYLAR_LOG_INFO(g_logger) << "offset : << " << s << "   request1:\n" << os.str();

    m_sylar::http::HttpRequestParser::ptr req_parser2(new m_sylar::http::HttpRequestParser());
    size_t s2 = req_parser2->execute(test_request, sizeof(test_request));

    std::ostringstream os2;
    req_parser->getData()->updateAndDump(os2);

    M_SYLAR_LOG_INFO(g_logger) << "request2:\n" << os2.str();
    return 1;
}

int test_responseParser()
{
    m_sylar::http::HttpResponseParser::ptr parser(new m_sylar::http::HttpResponseParser());
    int total_len =  sizeof(test_response);
    int single_len = 25;
    char buffer[single_len + 1];
    buffer[single_len] = '\0';
    memcpy(buffer, test_response, single_len);
    int length = parser->execute(buffer, single_len);

    std::stringstream ss;
    parser->getData()->dump(ss);
    std::string message = ss.str();
    M_SYLAR_LOG_INFO(g_logger) << "lenght : " << length <<  "  response : \n" << ss.str(); 

    // std::stringstream ss1;

    // length = parser->execute((char*)message.c_str(), message.size());
    // parser->getData()->updateAndDump(ss1);
    // M_SYLAR_LOG_INFO(g_logger) << "lenght : " << length <<  "  response : \n" << ss1.str(); 
    // message = ss1.str();


    // std::stringstream ss2;
    // length = parser->execute((char*)message.c_str(), message.size());
    // parser->getData()->updateAndDump(ss2);
    // M_SYLAR_LOG_INFO(g_logger) << "lenght : " << length <<  "  response : \n" << ss2.str(); 
    
    return 0;
}

int main(void)
{
    test_responseParser();
    return 0;
}