#pragma once
#include "parser.hpp"
#include "request.hpp"
#include "response.hpp"

namespace m_sylar{
namespace http{
using StatusCode = protocol::http::status_code::value;
inline std::string toString(StatusCode code) {
    return protocol::http::status_code::get_string(code);
}



class Request {
public:
    using ptr = std::shared_ptr<Request>;
    Request() = delete;
    Request(protocol::http::parser::request::ptr request) : m_request(request) {
        parserParams();
    }

public:
    void parserParams();
    protocol::http::parser::request::ptr get() {
        return m_request;
    }
public:
    std::string raw() {
        return m_request->raw();
    }
    std::string rawHeader() {
        return m_request->raw_head();
    }

    inline std::string const & getMethod() const {
        return m_request->get_method();
    }

    inline std::string const & getUri() const {
        return m_request->get_uri();
    }

    inline std::string const & getVersion() const {
        return m_request->get_version();
    }

    inline std::string const & getHeader(const std::string &key) const {
        return m_request->get_header(key);
    }

    inline std::string const & getBody() const {
        return m_request->get_body();
    }



private:
    std::string m_path {};
    std::map<std::string, std::string> m_params {};
    protocol::http::parser::request::ptr m_request;
};







class Response{

public:
    using ptr = std::shared_ptr<Response>;
    Response() = delete;
    Response(protocol::http::parser::response::ptr response) : m_response(response) {
    }

public:
    protocol::http::parser::response::ptr get() {
        return m_response;
    }

public:
    std::string raw() {
        return m_response->raw();
    }

    void setStatus(StatusCode code) {
        m_response->set_status(code);
    }
    inline StatusCode getStatus() const {
        return m_response->get_status_code();
    }

    inline std::string const & getVersion() const {
        return m_response->get_version();
    }

    inline std::string const & getHeader(const std::string &key) const {
        return m_response->get_header(key);
    }
    inline void appendHeader(const std::string &key, const std::string &value) {
        m_response->append_header(key, value);
    }
    inline void replaceHeader(const std::string &key, const std::string &value) {
        m_response->replace_header(key, value);
    }
    inline void removeHeader(const std::string &key) {
        m_response->remove_header(key);
    }

    inline std::string const & getBody() const {
        return m_response->get_body();
    }
    inline void setBody(const std::string &body) {
        m_response->set_body(body);
    }



private:
    protocol::http::parser::response::ptr m_response;
};
}




}