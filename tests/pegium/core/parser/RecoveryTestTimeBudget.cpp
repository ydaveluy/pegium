#include <gtest/gtest.h>

#include <iostream>
#include <string_view>

namespace {

constexpr int64_t kSoftBudgetMs = 10;
constexpr int64_t kHardBudgetMs = 50;

constexpr std::string_view kRecoveryTestPrefix = "Recovery";

bool test_is_recovery_focused(const testing::TestInfo &info) noexcept {
  const std::string_view suite{info.test_suite_name()};
  return suite.starts_with(kRecoveryTestPrefix);
}

class RecoveryTimeBudgetListener final
    : public ::testing::EmptyTestEventListener {
public:
  void OnTestEnd(const testing::TestInfo &info) override {
    if (!test_is_recovery_focused(info)) {
      return;
    }
    const auto *result = info.result();
    if (result == nullptr) {
      return;
    }
    const int64_t elapsedMs = result->elapsed_time();
    if (elapsedMs > kHardBudgetMs) {
      std::cerr << "[RECOVERY-PERF] " << info.test_suite_name() << "."
                << info.name() << " exceeded hard budget (" << elapsedMs
                << " ms > " << kHardBudgetMs << " ms)\n";
    } else if (elapsedMs > kSoftBudgetMs) {
      std::cerr << "[RECOVERY-PERF] " << info.test_suite_name() << "."
                << info.name() << " exceeded soft budget (" << elapsedMs
                << " ms > " << kSoftBudgetMs << " ms)\n";
    }
  }
};

struct ListenerRegistrar {
  ListenerRegistrar() {
    auto &listeners = testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new RecoveryTimeBudgetListener{});
  }
};

[[maybe_unused]] const ListenerRegistrar kRegistrar;

} // namespace
