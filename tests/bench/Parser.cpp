#include "BenchmarkSupport.hpp"

#include <charconv>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pegium::bench {
namespace {

using namespace pegium::parser;

struct Expr : pegium::AstNode {};

struct NumberExpr : Expr {
  int value = 0;
};

struct BinaryExpr : Expr {
  pointer<Expr> left;
  std::string op;
  pointer<Expr> right;
};

struct ParserBenchHarness final : PegiumParser {
  Terminal<> WS{"WS", some(s)};
  Terminal<int> Number{
      "Number", some(d),
      opt::with_converter(
          [](std::string_view text) noexcept -> opt::ConversionResult<int> {
        if (text == "13") {
          return opt::conversion_error<int>("bench conversion failure");
        }
        int value = 0;
        const auto [ptr, ec] =
            std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc() || ptr != text.data() + text.size()) {
          return opt::conversion_error<int>("invalid integer");
        }
        return opt::conversion_value<int>(value);
      })};
  Rule<Expr> Primary{
      "Primary", create<NumberExpr>() + assign<&NumberExpr::value>(Number)};
  Infix<BinaryExpr, &BinaryExpr::left, &BinaryExpr::op, &BinaryExpr::right>
      Expression{"Expression", Primary, LeftAssociation("+"_kw | "-"_kw)};
  Rule<Expr> Root{"Root", Expression};
  Skipper skipper = SkipperBuilder().ignore(WS).build();

  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }
};

std::string make_expression_source(std::size_t targetBytes, bool malformed,
                                   bool conversionFailures) {
  std::string source = "1";
  std::size_t index = 1;
  while (source.size() < targetBytes) {
    source += (index % 2 == 0) ? " + " : " - ";
    if (malformed && index % 17 == 0) {
      source += "+ 1";
    } else if (conversionFailures && index % 19 == 0) {
      source += "13";
    } else {
      source += std::to_string((index % 97) + 1);
    }
    ++index;
  }
  return source;
}

BenchmarkTimings run_iteration(const std::string &source,
                               bool expectDiagnostics) {
  ParserBenchHarness parser;
  pegium::workspace::Document document;
  document.setText(source);

  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  parser.parse(document);
  const auto end = Clock::now();

  if (document.parseResult.cst == nullptr) {
    throw std::runtime_error("Benchmark parser did not build a CST.");
  }
  if (expectDiagnostics && document.parseResult.parseDiagnostics.empty()) {
    throw std::runtime_error("Benchmark input did not trigger diagnostics.");
  }

  BenchmarkTimings timings{};
  timings[static_cast<std::size_t>(BenchmarkStep::Parsing)] =
      std::chrono::duration<double, std::milli>(end - start).count();
  timings[static_cast<std::size_t>(BenchmarkStep::FullBuild)] =
      timings[static_cast<std::size_t>(BenchmarkStep::Parsing)];
  return timings;
}

} // namespace

void register_parser_benchmarks(BenchmarkRegistry &registry) {
  const auto recoverySource =
      make_expression_source(benchmark_target_bytes(), true, false);
  registry.add("parser-recovery", recoverySource.size(),
               [source = recoverySource] {
                 return run_iteration(source, true);
               });

  const auto conversionSource =
      make_expression_source(benchmark_target_bytes(), false, true);
  registry.add("parser-conversion-diagnostics", conversionSource.size(),
               [source = conversionSource] {
                 return run_iteration(source, true);
               });
}

} // namespace pegium::bench
