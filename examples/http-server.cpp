// examples/http-server.cpp                                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/net29/net.hpp>
#include <beman/execution26/execution.hpp>
#include "demo_algorithm.hpp"
#include "demo_scope.hpp"
#include "demo_task.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace ex  = beman::execution26;
namespace net = beman::net29;
using namespace std::chrono_literals;

// ----------------------------------------------------------------------------

std::unordered_map<std::string, std::string> files{
    {"/", "examples/data/index.html"},
    {"/favicon.ico", "examples/data/favicon.icon"},
};

auto process_request(auto& stream, std::string request) -> demo::task<>
{
    std::istringstream in(request);
    std::string method, url, version;
    if (!(in >> method >> url >> version) || method != "GET")
    {
        std::cout << "not a [supported] HTTP request\n";
        co_return;
    }
    auto it(files.find(url));
    std::cout << "url='" << url << "' -> " << (it == files.end()? "not found": it->second) << "\n";
    std::ostringstream out;
    out << std::ifstream(it == files.end()? std::string(): it->second).rdbuf();
    auto body{out.str()};
    out.clear();
    out.str(std::string());
    out << "HTTP/1.1 " << (it == files.end()? "404 not found": "200 found\r\n")
        << "Content-Length: " << body.size() << "\r\n\r\n"
        << body;
    auto response(out.str());
    co_await net::async_send(stream, net::buffer(response));
}

auto make_client(auto stream) -> demo::task<>
{
    char        buffer[16];
    std::string request;
    for (std::size_t n; 0 < (n = co_await net::async_receive(stream, net::buffer(buffer))); )
    {
        std::string_view sv(buffer, n);
        request += sv; 
        if (request.npos != sv.find("\r\n\r\n")) {
            co_await process_request(stream, std::move(request));
            break;
        }
    }
    std::cout << "client done\n";
}

auto main() -> int
{
    net::io_context context;
    demo::scope     scope;
    net::ip::tcp::endpoint ep(net::ip::address_v4::any(), 12346);
    net::ip::tcp::acceptor server(context, ep);
    std::cout << "listening on " << ep << "\n";

    scope.spawn(std::invoke([](auto, auto& scope, auto& server) -> demo::task<> {
        while (true)
        {
            auto[stream, address] = co_await net::async_accept(server);
            std::cout << "received connection from " << address << "\n";
            scope.spawn(make_client(std::move(stream)));

        }
    }, context.get_scheduler(), scope, server));

    context.run();
}