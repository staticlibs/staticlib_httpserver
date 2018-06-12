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

#include "staticlib/pion/tcp_connection.hpp"

#include "asio.hpp"
#include "asio/ssl.hpp"

namespace staticlib {
namespace pion {

tcp_connection::tcp_connection(asio::io_service& io_service, ssl_context_type& ssl_context,
        const bool ssl_flag, connection_handler finished_handler) :
m_ssl_socket(io_service, ssl_context), 
m_ssl_flag(ssl_flag),
m_lifecycle(LIFECYCLE_CLOSE),
m_finished_handler(finished_handler),
strand(io_service),
timer(io_service) {
    save_read_pos(nullptr, nullptr);
}

tcp_connection::~tcp_connection() {
    close();
}

bool tcp_connection::is_open() const {
    return const_cast<ssl_socket_type&> (m_ssl_socket).lowest_layer().is_open();
}

void tcp_connection::close() {
    if (is_open()) {
        try {
            // shutting down SSL will wait forever for a response from the remote end,
            // which causes it to hang indefinitely if the other end died unexpectedly
            // if (get_ssl_flag()) m_ssl_socket.shutdown();

            // windows seems to require this otherwise it doesn't
            // recognize that connections have been closed
            m_ssl_socket.next_layer().shutdown(asio::ip::tcp::socket::shutdown_both);
        } catch (...) {
        } // ignore exceptions

        // close the underlying socket (ignore errors)
        std::error_code ec;
        m_ssl_socket.next_layer().close(ec);
    }
}

// there is no good way to do this on windows until vista or later (0x0600)
// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/basic_stream_socket/cancel/overload2.html
// note that the asio docs are misleading because close() is not thread-safe,
// and the suggested #define statements cause WAY too much trouble and heartache
void tcp_connection::cancel() {
#if !defined(_MSC_VER) || (_WIN32_WINNT >= 0x0600)
    std::error_code ec;
    m_ssl_socket.next_layer().cancel(ec);
#endif
}

void tcp_connection::cancel_timer() {
#if !defined(_MSC_VER) || (_WIN32_WINNT >= 0x0600)
    std::error_code ec;
    timer.cancel(ec);
#endif
}

void tcp_connection::finish() {
    tcp_connection_ptr conn = shared_from_this();
    if (m_finished_handler) m_finished_handler(conn);
}

bool tcp_connection::get_ssl_flag() const {
    return m_ssl_flag;
}

void tcp_connection::set_lifecycle(lifecycle_type t) {
    m_lifecycle = t;
}

tcp_connection::lifecycle_type tcp_connection::get_lifecycle() const {
    return m_lifecycle;
}

bool tcp_connection::get_keep_alive() const {
    return m_lifecycle != LIFECYCLE_CLOSE;
}

bool tcp_connection::get_pipelined() const {
    return m_lifecycle == LIFECYCLE_PIPELINED;
}

tcp_connection::read_buffer_type& tcp_connection::get_read_buffer() {
    return m_read_buffer;
}

void tcp_connection::save_read_pos(const char *read_ptr, const char *read_end_ptr) {
    m_read_position.first = read_ptr;
    m_read_position.second = read_end_ptr;
}

void tcp_connection::load_read_pos(const char *&read_ptr, const char *&read_end_ptr) const {
    read_ptr = m_read_position.first;
    read_end_ptr = m_read_position.second;
}

asio::ip::tcp::endpoint tcp_connection::get_remote_endpoint() const {
    asio::ip::tcp::endpoint remote_endpoint;
    try {
        // const_cast is required since lowest_layer() is only defined non-const in asio
        remote_endpoint = const_cast<ssl_socket_type&> (m_ssl_socket).lowest_layer().remote_endpoint();
    } catch (asio::system_error& /* e */) {
        // do nothing
    }
    return remote_endpoint;
}

asio::ip::address tcp_connection::get_remote_ip() const {
    return get_remote_endpoint().address();
}

unsigned short tcp_connection::get_remote_port() const {
    return get_remote_endpoint().port();
}

asio::io_service& tcp_connection::get_io_service() {
    return m_ssl_socket.lowest_layer().get_io_service();
}

tcp_connection::socket_type& tcp_connection::get_socket() {
    return m_ssl_socket.next_layer();
}

tcp_connection::ssl_socket_type& tcp_connection::get_ssl_socket() {
    return m_ssl_socket;
}

const tcp_connection::socket_type& tcp_connection::get_socket() const {
    return const_cast<ssl_socket_type&> (m_ssl_socket).next_layer();
}

const tcp_connection::ssl_socket_type& tcp_connection::get_ssl_socket() const {
    return m_ssl_socket;
}

asio::io_service::strand& tcp_connection::get_strand() {
    return strand;
}

asio::steady_timer& tcp_connection::get_timer() {
    return timer;
}

} // namespace
}
