/*
 * Copyright 2015, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "staticlib/pion/http_request.hpp"

namespace staticlib { 
namespace pion {

http_request::http_request(const std::string& resource) : 
m_method(REQUEST_METHOD_GET), 
m_resource(resource) { }

http_request::http_request() : 
m_method(REQUEST_METHOD_GET) { }

http_request::~http_request() { }

void http_request::clear() {
    http_message::clear();
    m_method.erase();
    m_resource.erase();
    m_original_resource.erase();
    m_query_string.erase();
    m_query_params.clear();
}

bool http_request::is_content_length_implied() const {
    return false;
}

const std::string& http_request::get_method(void) const {
    return m_method;
}

const std::string& http_request::get_resource() const {
    return m_resource;
}

const std::string& http_request::get_original_resource() const {
    return m_original_resource;
}

const std::string& http_request::get_query_string() const {
    return m_query_string;
}

const std::string& http_request::get_query(const std::string& key) const {
    return get_value(m_query_params, key);
}

std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& http_request::get_queries() {
    return m_query_params;
}

bool http_request::has_query(const std::string& key) const {
    return (m_query_params.find(key) != m_query_params.end());
}

void http_request::set_method(const std::string& str) {
    m_method = str;
    clear_first_line();
}

void http_request::set_resource(const std::string& str) {
    m_resource = m_original_resource = str;
    clear_first_line();
}

void http_request::change_resource(const std::string& str) {
    m_resource = str;
}

void http_request::set_query_string(const std::string& str) {
    m_query_string = str;
    clear_first_line();
}

void http_request::add_query(const std::string& key, const std::string& value) {
    m_query_params.insert(std::make_pair(key, value));
}

void http_request::change_query(const std::string& key, const std::string& value) {
    change_value(m_query_params, key, value);
}

void http_request::delete_query(const std::string& key) {
    delete_value(m_query_params, key);
}

void http_request::use_query_params_for_query_string() {
    set_query_string(make_query_string(m_query_params));
}

void http_request::use_query_params_for_post_content() {
    std::string post_content(make_query_string(m_query_params));
    set_content_length(post_content.size());
    char *ptr = create_content_buffer(); // null-terminates buffer
    if (!post_content.empty()) {
        memcpy(ptr, post_content.c_str(), post_content.size());
    }
    set_method(REQUEST_METHOD_POST);
    set_content_type(CONTENT_TYPE_URLENCODED);
}

void http_request::set_content(const std::string &value) {
    set_content_length(value.size());
    char *ptr = create_content_buffer();
    if (!value.empty()) {
        memcpy(ptr, value.c_str(), value.size());
    }
}

void http_request::set_content(const char* value, size_t size) {
    if (nullptr == value || 0 == size) return;
    set_content_length(size);
    char *ptr = create_content_buffer();
    memcpy(ptr, value, size);
}

void http_request::set_payload_handler(http_parser::payload_handler_type ph) {
    m_payload_handler = std::move(ph);
    // this should be done only once during request processing
    // and we won't restrict request_reader lifecycle after 
    // payload is processed, so we drop the pointer to it
    if (m_request_reader) {
        // request will always outlive reader
        m_request_reader->set_payload_handler(m_payload_handler);
        m_request_reader = nullptr;
    }
}

http_parser::payload_handler_type& http_request::get_payload_handler_wrapper() {
    return m_payload_handler;
}

void http_request::set_request_reader(http_parser* rr) {
    m_request_reader = rr;
}

void http_request::update_first_line() const {
    // start out with the request method
    m_first_line = m_method;
    m_first_line += ' ';
    // append the resource requested
    m_first_line += m_resource;
    if (!m_query_string.empty()) {
        // append query string if not empty
        m_first_line += '?';
        m_first_line += m_query_string;
    }
    m_first_line += ' ';
    // append HTTP version
    m_first_line += get_version_string();
}

void http_request::append_cookie_headers() {
    for (std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>::const_iterator i = get_cookies().begin(); i != get_cookies().end(); ++i) {
        std::string cookie_header;
        cookie_header = i->first;
        cookie_header += COOKIE_NAME_VALUE_DELIMITER;
        cookie_header += i->second;
        add_header(HEADER_COOKIE, cookie_header);
    }
}

} // namespace
}
