// Consider modules
#include "Gherk-min.hpp"

// example usage:
namespace gherk_min::step_defines {

template<>
struct Given<1> {
    static consteval std::string_view GetRegex() { return "^I have (.*) bananas$"; }
    // demonstrate the value is ignored
    using ignored = int;
    static ignored StepDefinition() {
        // some code
        //  will likely have side effects
        return 1;
    }
};

template<>
struct Given<2> {
    static consteval std::string_view GetRegex() { return "^I have (.*) apples$"; }
    // demonstrate the value can be void
    static void StepDefinition() {
        // some code
        return;
    }
};

template<>
struct Given<3> {
    static consteval std::string_view GetRegex() { return "^I have (.*) pasties$"; }
    // demonstrate the function can take a regex group
    static void StepDefinition(const regex::Groups& re_group) {
        auto _ = re_group.size();
        return;
    }
};

template<>
struct Given<4> {
    static consteval std::string_view GetRegex() { return "^I have (.*) Cornish Pasties$"; }
    // demonstrate the function can take a regex group and context
    template<typename Context>
    static void StepDefinition(const regex::Groups& re_group, Context& c) {
        c = re_group.size();
        return;
    }
};
template<>
struct Given<5> {
    static consteval std::string_view GetRegex() { return "^I have (.*) Scones$"; }
    // demonstrate the function can just a context
    template<typename Context>
    static void StepDefinition(Context& c) {
        c = 5;
        return;
    }
};

template<> struct When<1> {
    static consteval std::string_view GetRegex() { return "^I eat a banana$"; }
    
    template<typename Context>
    static Context StepDefinition(Context& c) {
        return --c;
    }
};

template<> struct Then<1> {
    static consteval std::string_view GetRegex() { return "^I have (.*) bananas$"; }
    template<typename Context>
    static void StepDefinition(const regex::Groups& re_group, Context& c) {
        if (std::stoul(std::string{re_group[0]}) != c) { throw "Assertion failed"; }
        ;
    }
};
}

using namespace gherk_min;

namespace test_RemoveScenarioHeader {
    using namespace std::string_view_literals;
    constexpr auto scenario_text = R"(
  Scenario: Eating a banana
    Given I have 5 bananas
    When I eat a banana
    Then I have 4 bananas
)"sv;
    static_assert(RemoveScenarioHeader(scenario_text).size() < scenario_text.size());
    static_assert(RemoveScenarioHeader(scenario_text).find(keywords::Scenario) == std::string_view::npos);
    static_assert(RemoveScenarioHeader(scenario_text).find("Eating a banana") == std::string_view::npos);
    constexpr auto expected_2 = R"(Given I have 5 bananas
    When I eat a banana
    Then I have 4 bananas)"sv;
    static_assert(RemoveScenarioHeader(scenario_text) == expected_2);
}

namespace test_to_array {
    consteval auto ret_vec() { return std::vector<int>{ 1,2,3,4}; }
    // static_assert(to_array<5, int, ret_vec>().size() == 4);
    static_assert(constexpr_utils::to_array<5, int, []() { return ret_vec(); }>().size() == 4);
    static_assert(constexpr_utils::to_array<5, int, []() { return ret_vec(); }>()[0] == 1);
}

namespace test_trim {
    using namespace std::string_view_literals;
    static_assert(constexpr_utils::Trim("  Hello  ").size() == "Hello"sv.size());
    static_assert(constexpr_utils::Trim("  Hello  ") == "Hello");
    static_assert(constexpr_utils::Trim("\n  Hello \n").size() == "Hello"sv.size());
    static_assert(constexpr_utils::Trim("\tHello \t \n").size() == "Hello"sv.size());

    static_assert(constexpr_utils::TrimStart("  Hello").size() == "Hello"sv.size());
    static_assert(constexpr_utils::TrimStart(" \n Hello") == "Hello");
    static_assert(constexpr_utils::TrimStart("\n  Hello").size() == "Hello"sv.size());
    static_assert(constexpr_utils::TrimStart("\tHello").size() == "Hello"sv.size());
}

namespace test_UnionArrays {
    static_assert(
        UnionArrays(std::array{1,2,3,4}, std::array{5,6,7,8,9}) == std::array{1,2,3,4,5,6,7,8,9}
    );
}

namespace test_e2e {
    constexpr auto feature = R"(
Feature: Eating Bananas
  In order to not be hungry
  As a monkey
  I want to eat bananas

  Scenario: Eating a banana
    Given: I have 5 bananas
    When I eat a banana
    Then I have 4 bananas

  Scenario: Eating two banana
    Given: I have 5 bananas
    When I eat a banana
    And I eat a banana
    Then I have 3 bananas
)";
    namespace example {
        using Context = size_t;
    }

    using namespace std::string_view_literals;
    constexpr auto expected = R"(Scenario: Eating a banana
    Given: I have 5 bananas
    When I eat a banana
    Then I have 4 bananas)"sv;
    // auto a = CreateTest<constexpr_utils::NTTPString_view{feature}>();
    constexpr auto feature_0 = std::string_view{feature};
    // constexpr auto feature_1 = constexpr_utils::to_array<1024, char, [](){return std::string_view{feature};}>();
    constexpr auto feature_  = constexpr_utils::NTTPString_view<
                                constexpr_utils::constexprStrlen(feature) +1
                            >{feature};
    static_assert(SplitIntoScenarios<feature_>()[0].size() == expected.size());
    static_assert(SplitIntoScenarios<feature_>().size() == 2);

    static_assert(SplitIntoScenarios<feature_>()[0] == expected);
    constexpr auto expected_  = constexpr_utils::NTTPString_view<
                                constexpr_utils::constexprStrlen(expected.data()) +1
                            >{expected.data()};
    static_assert(SplitIntoScenarios<expected_>()[0].size() == expected.size());
    static_assert(SplitIntoScenarios<expected_>()[0] == expected);

    static_assert(SplitIntoScenarios<expected_>().size() == 1);
    static_assert(SplitIntoNTTPScenarios<feature_>()[0].size() > 1);
    static_assert(SplitIntoNTTPScenarios<feature_>().size() == 2);
    static_assert(SplitIntoNTTPScenarios<expected_>().size() == 1);
    static_assert(SplitIntoNTTPScenarios<feature_>()[0] == expected);

    constexpr constexpr_utils::NTTPString_view<70> test_str = 
        "Given: I have 5 bananas \n When I eat a banana \n Then I have 4 bananas";
    constexpr auto test = CreateTest<test_str, example::Context>();

    // constexpr auto test = CreateTest<SplitIntoNTTPScenarios<feature_>()[0]>();
}

