#include <arithmetics/services/Module.hpp>

#include <charconv>
#include <exception>
#include <iostream>
#include <optional>
#include <string_view>

#include <lsp/connection.h>
#include <lsp/io/socket.h>
#include <lsp/io/standardio.h>

#include <pegium/lsp/LanguageServerRuntime.hpp>
#include <pegium/services/SharedServices.hpp>

namespace {

std::optional<unsigned short> parse_port_arg(int argc, char **argv) {
  constexpr std::string_view portPrefix = "--port=";

  for (int index = 1; index < argc; ++index) {
    const std::string_view arg(argv[index]);
    if (!arg.starts_with(portPrefix)) {
      continue;
    }

    unsigned short port = 0;
    const auto portString = arg.substr(portPrefix.size());
    const auto [ptr, ec] = std::from_chars(
        portString.data(), portString.data() + portString.size(), port);
    (void)ptr;
    if (ec == std::errc{}) {
      return port;
    }
  }

  return std::nullopt;
}

int run_language_server(::lsp::io::Stream &stream) {
  ::lsp::Connection connection(stream);

  pegium::services::SharedServices services;
  if (!arithmetics::services::register_language_services(services)) {
    std::cerr << "Failed to register arithmetics language services\n";
    return 1;
  }

  services.lsp.connection = &connection;
  return pegium::lsp::startLanguageServer(services);
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (const auto port = parse_port_arg(argc, argv); port.has_value()) {
      std::cerr << "arithmetics-lsp listening on "
                << ::lsp::io::Socket::Localhost << ':' << *port << '\n';
      auto listener = ::lsp::io::SocketListener(*port, 1);
      auto socket = listener.listen();
      std::cerr << "arithmetics-lsp accepted socket connection\n";
      return run_language_server(socket);
    }

    auto &stream = ::lsp::io::standardIO();
    return run_language_server(stream);
  } catch (const std::exception &error) {
    std::cerr << "arithmetics-lsp fatal error: " << error.what() << '\n';
    return 2;
  }
}
