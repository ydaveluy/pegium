#include <gtest/gtest.h>

#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <pegium/utils/Stream.hpp>

namespace pegium::utils {
namespace {

TEST(StreamTest, MakeStreamCollectsSetAndMapRanges) {
  const std::set<int> values{1, 2, 3};
  EXPECT_EQ(collect(make_stream(values)), (std::vector<int>{1, 2, 3}));

  const std::map<std::string, int> entries{{"a", 1}, {"b", 2}};
  EXPECT_EQ(collect(make_stream(entries)),
            (std::vector<std::pair<const std::string, int>>{
                {"a", 1}, {"b", 2}}));
}

TEST(StreamTest, SingleStreamAndConcatPreserveOrder) {
  const int value = 7;
  EXPECT_EQ(collect(single_stream(value)), (std::vector<int>{7}));

  auto joined = concat<int>(make_stream(std::array{1, 2}),
                            make_stream(std::array{3}),
                            make_stream(std::array<int, 0>{}));
  EXPECT_EQ(collect(std::move(joined)), (std::vector<int>{1, 2, 3}));
}

TEST(StreamTest, FilterMapDistinctAndReduceCompose) {
  auto values = make_stream(std::array{1, 2, 2, 3, 4, 4, 5});
  auto evenSquares = map(
      distinct(filter(std::move(values), [](int value) { return value % 2 == 0; })),
      [](int value) { return value * value; });

  EXPECT_EQ(collect(std::move(evenSquares)), (std::vector<int>{4, 16}));

  auto sum = reduce(make_stream(std::array{1, 2, 3, 4}), 0,
                    [](int acc, int value) { return acc + value; });
  EXPECT_EQ(sum, 10);
}

TEST(StreamTest, DistinctWithKeyKeepsFirstValuePerKey) {
  struct Entry {
    int id;
    std::string name;

    bool operator==(const Entry &) const = default;
  };

  auto values = make_stream(std::array<Entry, 4>{
      Entry{1, "alpha"}, Entry{2, "beta"}, Entry{1, "shadow"},
      Entry{2, "duplicate"},
  });

  auto deduplicated =
      distinct(std::move(values), [](const Entry &entry) { return entry.id; });

  EXPECT_EQ(collect(std::move(deduplicated)),
            (std::vector<Entry>{{1, "alpha"}, {2, "beta"}}));
}

} // namespace
} // namespace pegium::utils
