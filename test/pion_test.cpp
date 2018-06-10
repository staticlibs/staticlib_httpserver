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

/* 
 * File:   pion_test.cpp
 * Author: alex
 *
 * Created on March 3, 2015, 9:22 AM
 */

#include <iostream>
#include <memory>
#include <utility>
#include <functional>
#include <thread>
#include <fstream>
#include <chrono>
#include <cstdint>

#include "asio.hpp"

#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/http_response_writer.hpp"
#include "staticlib/pion/http_server.hpp"

const uint16_t SECONDS_TO_RUN = 1;
const uint16_t TCP_PORT = 8080;

void hello_service(sl::pion::http_request_ptr, sl::pion::response_writer_ptr resp) {
    resp->write("Hello World!\n");
    resp->send();
}

void hello_service_post(sl::pion::http_request_ptr, sl::pion::response_writer_ptr resp) {
    resp->write("Hello POST!\n");
    resp->send();
}

class file_writer {
    mutable std::unique_ptr<std::ofstream> stream;
 
public:
    // copy constructor is required due to std::function limitations
    // but it doesn't used by server implementation
    // move-only payload handlers can use move logic
    // instead of copy one (with mutable member fields)
    // in MSVC only moved-to instance will be used
    // in GCC copy constructor won't be called at all
    file_writer(const file_writer& other) :
    stream(std::move(other.stream)) { }

    file_writer& operator=(const file_writer&) = delete;  

    file_writer(file_writer&& other) :
    stream(std::move(other.stream)) { }

    file_writer& operator=(file_writer&&) = delete;

    file_writer(const std::string& filename) {
        stream = std::unique_ptr<std::ofstream>{new std::ofstream{filename, std::ios::out | std::ios::binary}};
        stream->exceptions(std::ofstream::failbit | std::ofstream::badbit);
    }

    void operator()(const char* s, std::size_t n) {
        stream->write(s, n);
    }

    void close() {
        std::cout << "I am closed" << std::endl;
        stream->close();
    }
};

class file_sender : public std::enable_shared_from_this<file_sender> {
    sl::pion::response_writer_ptr writer;
    std::ifstream stream;
    std::array<char, 8192> buf;
    std::mutex mutex;

public:
    file_sender(const std::string& filename, sl::pion::response_writer_ptr writer) : 
    writer(writer),
    stream(filename, std::ios::in | std::ios::binary) {
        stream.exceptions(std::ifstream::badbit);
    }

    void send() {
        std::error_code ec{};
        handle_write(ec, 0);
    }

    void handle_write(const std::error_code& ec, std::size_t /* bytes_written */) {
        std::lock_guard<std::mutex> lock{mutex};
        if (!ec) {
            stream.read(buf.data(), buf.size());
            writer->clear();
            writer->write_no_copy(buf.data(), static_cast<size_t>(stream.gcount()));
            if (stream) {
                auto self = shared_from_this();
                writer->send_chunk([self](const std::error_code& ec, size_t bt) {
                    self->handle_write(ec, bt);
                });
            } else {
                writer->send_final_chunk();
            }
        } else {
            // make sure it will get closed
            writer->get_connection()->set_lifecycle(sl::pion::tcp_connection::LIFECYCLE_CLOSE);
        }
    }
};

void file_upload_resource(sl::pion::http_request_ptr req, sl::pion::response_writer_ptr resp) {
    auto ph = req->get_payload_handler<file_writer>();
    if (ph) {
        ph->close();
    } else {
        std::cout << "No payload handler found in main handler" << std::endl;
    }
    auto fs = std::make_shared<file_sender>("uploaded.dat", resp);
    fs->send();
}

file_writer file_upload_payload_handler_creator(sl::pion::http_request_ptr& req) {
    (void) req;
    return file_writer{"uploaded.dat"};
}

void test_pion() {
    STATICLIB_PION_LOG_SETLEVEL_INFO(STATICLIB_PION_GET_LOGGER("staticlib.pion"))
    // pion
    sl::pion::http_server server(2, TCP_PORT);
    server.add_handler("GET", "/hello", hello_service);
    server.add_handler("POST", "/hello/post", hello_service_post);
    server.add_handler("POST", "/fu", file_upload_resource);
    server.add_payload_handler("POST", "/fu", file_upload_payload_handler_creator);
    server.add_handler("POST", "/fu1", file_upload_resource);
    server.get_scheduler().set_thread_stop_hook([]() STATICLIB_NOEXCEPT {
        std::cout << "Thread stopped. " << std::endl;
    });
    server.start();
    std::this_thread::sleep_for(std::chrono::seconds{SECONDS_TO_RUN});
    server.stop(true);
}

int main() {
    try {
        test_pion();
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
    return 0;
}
