#include "http.hpp"

namespace m_sylar {
void http::Request::parserParams() {
  std::string uri = m_request->get_uri();
  int path_end = uri.find("?");
  if (path_end != std::string::npos) {
    m_path = uri.substr(0, path_end);
  }

  int param_begin = path_end + 1;
  int param_end = uri.find("#");
  if (param_end == std::string::npos) {
    param_end = uri.size();
  }
  while (param_begin < param_end) {
    int key_end = uri.find("=", param_begin);
    if (key_end == std::string::npos || key_end > param_end) {
      break;
    }
    std::string key = uri.substr(param_begin, key_end - param_begin);
    int value_end = uri.find("&", key_end);
    if (value_end == std::string::npos || value_end > param_end) {
      value_end = param_end;
    }
    std::string value = uri.substr(key_end + 1, value_end - key_end - 1);
    m_params.insert({key, value});
    param_begin = value_end + 1;
  }
  return;
}

void http::Request::parserCookies(const std::string& cookie_str) {
  std::istringstream ss(cookie_str);
  std::string item;
  while (std::getline(ss, item, ';')) {
  size_t pos = item.find('=');
  if (pos != std::string::npos) {
      std::string key = item.substr(0, pos);
      std::string val = item.substr(pos + 1);
      key.erase(0, key.find_first_not_of(" \t"));
      val.erase(0, val.find_first_not_of(" \t"));
      m_cookies.insert({key, val});
  }
  }
}
} // namespace m_sylar
