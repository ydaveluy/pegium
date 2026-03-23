#include "BenchmarkSupport.hpp"

namespace pegium::bench {
void register_arithmetics_benchmarks(BenchmarkRegistry &registry);
void register_domainmodel_benchmarks(BenchmarkRegistry &registry);
void register_parser_benchmarks(BenchmarkRegistry &registry);
void register_requirements_benchmarks(BenchmarkRegistry &registry);
void register_scope_benchmarks(BenchmarkRegistry &registry);
void register_statemachine_benchmarks(BenchmarkRegistry &registry);
} // namespace pegium::bench

int main(int argc, char **argv) {
  pegium::bench::BenchmarkRegistry registry;
  pegium::bench::register_arithmetics_benchmarks(registry);
  pegium::bench::register_domainmodel_benchmarks(registry);
  pegium::bench::register_parser_benchmarks(registry);
  pegium::bench::register_requirements_benchmarks(registry);
  pegium::bench::register_scope_benchmarks(registry);
  pegium::bench::register_statemachine_benchmarks(registry);

  std::string filter;
  if (argc > 1) {
    filter = argv[1];
  } else {
    filter = pegium::bench::get_env_string("PEGIUM_BENCH_FILTER");
  }

  try {
    return registry.runAll(filter);
  } catch (const std::exception &error) {
    std::cerr << "Benchmark failed: " << error.what() << '\n';
    return 1;
  }
}
