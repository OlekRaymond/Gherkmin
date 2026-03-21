
#include <string_view>
#include <array>
#include <vector>
#include <string>
#include <algorithm>

namespace regex {
    using Groups = std::array<std::string_view, 5>;
    using Pattern = std::string_view;
}

namespace example {
    using Context = size_t;
}

namespace step_defines {

template<int index_>
struct StepDefinition { static constexpr bool is_end = true; };

// part of registration mechanism
template<typename T>
concept step_end = requires { T::is_end == true; };

// default to the end
//  all done in one place so we can change `is_end` var easily
template<size_t index_> struct Given : StepDefinition<index_> {};
template<size_t index_> struct When : StepDefinition<index_> {};
template<size_t index_> struct Then : StepDefinition<index_> {};


namespace detail_ {
    // Check template are X<size_t>, i.e. registerable
    template <template <class...> class Template, class... Args>
    void specialization_impl(const Template<Args...>&);

    template <class T, template <size_t...> class Template>
    concept int_specialization_of = requires(const T& t) {
        specialization_impl<Template>(t);
    };
}

// part of registration mechanism
template<typename T> concept given_end = step_end<T> && requires { detail_::int_specialization_of<T, Given> == true ; };
template<typename T> concept when_end = step_end<T> && requires { detail_::int_specialization_of<T, When> == true ; };
template<typename T> concept then_end = step_end<T> && requires { detail_::int_specialization_of<T, Then> == true ; };

// specialise everything else
template<>
struct Given<1> {
    static consteval std::string_view GetRegex() { return "^I have (.*) bananas$"; }
    // demonstate the value is ignored
    using ignored = int;
    static ignored StepDefinition() {
        // some code, can have side effects
        return 1;
    }
};

template<>
struct Given<2> {
    static consteval std::string_view GetRegex() { return "^I have (.*) apples$"; }
    // demonstate the value can be void
    static void StepDefinition() {
        // some code
        return;
    }
};

template<>
struct Given<3> {
    static consteval std::string_view GetRegex() { return "^I have (.*) pasties$"; }
    // demonstate the function can take a regex group
    static void StepDefinition(const regex::Groups& re_group) {
        auto _ = re_group.size();
        return;
    }
};

template<>
struct Given<4> {
    static consteval std::string_view GetRegex() { return "^I have (.*) Cornish Pasties$"; }
    // demonstate the function can take a regex group and context
    static void StepDefinition(const regex::Groups& re_group, example::Context& c) {
        c = re_group.size();
        return;
    }
};
template<>
struct Given<5> {
    static consteval std::string_view GetRegex() { return "^I have (.*) Scones$"; }
    // demonstate the function can just a context
    static void StepDefinition(example::Context& c) {
        c = 5;
        return;
    }
};

template<> struct When<1> {
    static consteval std::string_view GetRegex() { return "^I eat a banana$"; }
    static example::Context StepDefinition(example::Context& c) {
        return --c;
    }
};

template<> struct Then<1> {
    static consteval std::string_view GetRegex() { return "^I have (.*) bananas$"; }
    static void StepDefinition(const regex::Groups& re_group, example::Context& c) {
        if (std::stoul(std::string{re_group[0]}) != c) { throw "Assertion failed"; }
        ;
    }
};

}

constexpr auto RegexMatches(auto pattern, [[maybe_unused]] std::string_view to_match) { 
    // dummy impl for now 
    return regex::Groups{"true"};
    // (pattern.size() %2 == 0);
}
using ReGroupType = decltype(RegexMatches(".*", "anything"));

template<typename F>
concept RegexInvocable = std::invocable<F, regex::Groups&>;
template<typename F, typename Context>
concept RegexContextInvocable = std::invocable<F, regex::Groups&, Context>;
template<typename F, typename Context>
concept ContextInvocable = std::invocable<F, Context>;
template<typename F>
concept VoidInvocable = std::invocable<F>;

using StepDefintionFunctionSignature = decltype(+[](const regex::Groups&, example::Context&) -> void {});

struct Step {
    constexpr Step() = default;
    constexpr Step(const regex::Groups& re_group, const StepDefintionFunctionSignature& func)
        : m_group{re_group}, m_step_code{func}
    {}

    template<typename Context>
    void operator()(Context& context) const {
        m_step_code(m_group, context);
    }

    regex::Groups m_group;
    StepDefintionFunctionSignature m_step_code;
};

// as we move through we might 
template<template <size_t> typename StepDefinationType , size_t N = 1>
consteval Step GetStepDefinition(const std::string_view to_match) 
{
    using Context = example::Context; // this needs to be template param I think
    using step_def_to_check = StepDefinationType<N>;

    if (auto re_groups = RegexMatches(step_def_to_check::GetRegex(), to_match); re_groups.size() > 0) {
        return Step { re_groups , 
            +[](const regex::Groups& groups, Context& context) -> void {
            // some type erasure to allow for user to write functions with multiple signatures
            if constexpr (RegexContextInvocable<decltype(step_def_to_check::StepDefinition), Context>) {
                return (void)step_def_to_check::StepDefinition(groups, context);
            } else
            if constexpr (RegexInvocable<decltype(step_def_to_check::StepDefinition)>) {
                return (void)step_def_to_check::StepDefinition(groups);
            } else
            if constexpr (ContextInvocable<decltype(step_def_to_check::StepDefinition), Context>) {
                return (void)step_def_to_check::StepDefinition(context);
            } else
            if constexpr (VoidInvocable<decltype(step_def_to_check::StepDefinition)>) {
                return (void)step_def_to_check::StepDefinition();
            } else {
                // fail
                throw "StepDefinition did not match expected function signature";
            }
        }};
    }
    if constexpr (!step_defines::step_end<StepDefinationType<N+1>>) {
        return GetStepDefinition<StepDefinationType, N+1>(to_match);
    } else {
        throw "`Given` string did not match any `Given` regex expressions"
                "\n (reached end of step_defines::Given)";
    }
}
template<size_t N = 1>
consteval auto GetGivenDefinition(const std::string_view to_match) 
{
    return GetStepDefinition<step_defines::Given, N>(to_match);
}

template<size_t N = 1>
consteval auto GetWhenDefinition(const std::string_view to_match) 
{
    return GetStepDefinition<step_defines::When, N>(to_match);
}

template<size_t N = 1>
consteval auto GetThenDefinition(const std::string_view to_match) 
{
    return GetStepDefinition<step_defines::Then, N>(to_match);
}

enum class StepType { Given, When, Then, And };

namespace constexpr_utils {

constexpr std::string_view TrimStart(std::string_view str) {
    const auto first_not_newline = str.find_first_not_of('\n');
    const auto first_not_space = str.find_first_not_of(' ', first_not_newline);
    const auto first_not_tab = str.find_first_not_of('\t', first_not_space);
    const auto first_char = first_not_tab;
    if (first_char >= str.size()) throw "String only contains whitespace characters";
    if (first_char != 0) {
        return TrimStart(str.substr(first_char));
    }
    return str.substr(first_char);
}
namespace {
    using namespace std::string_view_literals;
    static_assert(TrimStart("  Hello").size() == "Hello"sv.size());
    static_assert(TrimStart(" \n Hello") == "Hello");
    static_assert(TrimStart("\n  Hello").size() == "Hello"sv.size());
    static_assert(TrimStart("\tHello").size() == "Hello"sv.size());
}
constexpr std::string_view TrimEnd(std::string_view str) {
    const auto last_not_newline = str.find_last_not_of('\n');
    const auto last_not_space = str.find_last_not_of(' ', last_not_newline);
    const auto last_not_tab = str.find_last_not_of('\t', last_not_space);
    const auto last_char = last_not_tab;
    if (last_char >= str.size()) throw "String only contains whitespace characters";
    auto trimmed = str.substr(0, last_char + 1);
    if (trimmed.size() == str.size()) {
        return trimmed;
    } else {
        return TrimEnd(trimmed);
    }
}
constexpr std::string_view Trim(std::string_view str) {
    return TrimEnd(TrimStart(str));
}
namespace {
    using namespace std::string_view_literals;
    static_assert(Trim("  Hello  ").size() == "Hello"sv.size());
    static_assert(Trim("  Hello  ") == "Hello");
    static_assert(Trim("\n  Hello \n").size() == "Hello"sv.size());
    static_assert(Trim("\tHello \t \n").size() == "Hello"sv.size());
}

// the constexpr 2 step copy to go from vector to array
template<size_t max_size, typename ValueType, auto builder, auto... args>
constexpr auto to_array() noexcept {
    namespace r = std::ranges;
    constexpr auto data = []() {
        const auto vec = builder();
        // using no_ref_value = std::remove_cvref<decltype(vec[0])>;
        std::array<ValueType, max_size> result{};
        const auto end_position = r::copy(vec, r::begin(result)).out;
        const auto size_ = r::distance(begin(result), end_position);
        return std::pair {result, size_};
    }(args...);
    std::array<ValueType, data.second> result{};
    for (size_t i = 0; i < data.second; ++i ) { result[i] = data.first[i]; }
    return result;
}
namespace {
    consteval auto ret_vec() { return std::vector<int>{ 1,2,3,4}; }
    // static_assert(to_array<5, int, ret_vec>().size() == 4);
    static_assert(to_array<5, int, []() { return ret_vec(); }>().size() == 4);
    static_assert(to_array<5, int, []() { return ret_vec(); }>()[0] == 1);


}

template<size_t array_1_size, size_t array_2_size, typename T>
consteval std::array<T, array_1_size + array_2_size> UnionArrays(const std::array<T,array_1_size>& first, const std::array<T, array_2_size>& second) {
    std::array<T, array_1_size + array_2_size> result{};
    auto last = std::copy(first.begin(), first.end(), result.begin());
    std::copy(second.begin(), second.end(), last);
    return result;
}
namespace {
    static_assert(
        UnionArrays(std::array{1,2,3,4}, std::array{5,6,7,8,9}) == std::array{1,2,3,4,5,6,7,8,9}
    );
}

constexpr size_t constexprStrlen(const char* str) {
    for (size_t i = 0; ; ++i) {
        if (str[i] == '\0') return i;
    }
    return -1;
}

template<size_t N>
struct NTTPString_view {
    explicit consteval NTTPString_view(const char* str) {
        std::copy(str, str + N, value);
    }
    consteval NTTPString_view(const char (&str)[N]) {
        std::copy(str, str + N, value);
    }
    consteval NTTPString_view(const std::array<char, N> str) {
        std::copy(str, N, value);
    }

    consteval operator std::string_view() const {
        return std::string_view{value, N - 1 };
    }
    
    char value[N];
};

template<>
struct NTTPString_view<std::string_view::npos> {
    const char* begin_;
    size_t end_;
    constexpr const char* begin() { return begin_; }
    constexpr const char* end() { return begin_ + end_; }
    constexpr NTTPString_view() : begin_(), end_() {}
    constexpr NTTPString_view(std::string_view from)
        : begin_(from.begin()) , end_(from.size())
    {}
    consteval operator std::string_view() const {
        return std::string_view{begin_, end_ };
    }
    constexpr size_t size() { return end_; }
};
using string_view = NTTPString_view<std::string_view::npos>;

}

/// Returns the step type and the remaining bits of the string
constexpr std::pair<StepType, std::string_view> GetStepType(std::string_view from_line) {
    using namespace constexpr_utils;
    // remove all whitespace from `from_line`
    from_line = TrimStart(from_line);
    using namespace std::string_view_literals;
    // TODO: move to constants for localisation
     for (auto search : {"Given: "sv, "When "sv, "Then "sv, "And "sv}) {
        if (from_line.find(search) == 0 ) {
            from_line.remove_prefix(search.size());
            return {
                // tad ugly
                  search[0] == 'G' ? StepType::Given
                : search[0] == 'W' ? StepType::When
                : search[0] == 'A' ? StepType::And
                : StepType::Then,
              from_line
            };
        }
    }
    // throw from_line;
    throw "String did not start with any expected keywords";
}

static constexpr std::string_view ScenarioKeyword = "Scenario: ";
static constexpr std::string_view BackgroundKeyword = "Background: ";

consteval auto GetDefinitionsFactory(StepType step_type) {
    switch (step_type) {
        case StepType::Given: return GetGivenDefinition<1>;
        case StepType::When: return GetWhenDefinition<1>;
        case StepType::Then: return GetThenDefinition<1>;
        case StepType::And: throw "And is not a valid step type for factory";
    }
    throw "String did not start with any expected keywords";
}

// previous really should be a strong type or something more useful
consteval auto GetStepDefinition(std::string_view to_match, StepType previous) {
    using namespace constexpr_utils;
    // Remove whitespace from start of line
    to_match = TrimStart(to_match);
    const auto contains = [to_match](std::string_view search) {
        return to_match.find(search) != std::string_view::npos;
    };

    if (contains("And ")) {
        using namespace std::string_view_literals;
        to_match.remove_prefix("And "sv.size());
        GetDefinitionsFactory(previous)(to_match);
    }
    auto [step_type, remaining] = GetStepType(to_match);
    return std::make_tuple(GetDefinitionsFactory(step_type)(remaining), step_type);
}

consteval std::string_view RemoveScenarioHeader(std::string_view scenario) {
    using constexpr_utils::Trim;
    const auto start_scenario_pos = scenario.find(ScenarioKeyword) + ScenarioKeyword.size(); 
    const auto start_steps_pos = scenario.find('\n', start_scenario_pos);
    scenario = Trim(scenario.substr(start_steps_pos + 1));
    return scenario;
}
namespace {
    using namespace std::string_view_literals;
    constexpr auto scen_text = R"(
  Scenario: Eating a banana
    Given I have 5 bananas
    When I eat a banana
    Then I have 4 bananas
)"sv;
    static_assert(RemoveScenarioHeader(scen_text).size() < scen_text.size());
    static_assert(RemoveScenarioHeader(scen_text).find(ScenarioKeyword) == std::string_view::npos);
    static_assert(RemoveScenarioHeader(scen_text).find("Eating a banana") == std::string_view::npos);
    constexpr auto expected_2 = R"(Given I have 5 bananas
    When I eat a banana
    Then I have 4 bananas)"sv;
    static_assert(RemoveScenarioHeader(scen_text) == expected_2);
}


consteval std::vector<Step> CreateSteps(std::string_view scenario /*, std::vector<Step>&& existing = {}*/) {
    using constexpr_utils::Trim;
    StepType previous{};
    std::vector<Step> steps = {}; // existing;

    
    for (; scenario.size() > 0; scenario = Trim(scenario)) {
        const auto line_end = scenario.find('\n');
        const auto step_line = scenario.substr(0, line_end);
        auto [step, current] = GetStepDefinition(step_line, previous);
        steps.push_back(step);
        previous = current;
        if (line_end == std::string_view::npos) return steps;
        scenario = scenario.substr(line_end);
    }
    return steps;
}

template<constexpr_utils::NTTPString_view scenario_>
consteval auto CreateTest() {
    // should not be accessed as default initalised
    //  if it is that means the first value was And
    using namespace constexpr_utils;
    // vector -> array
    auto steps_arr = constexpr_utils::to_array<20, Step, 
        []() { return CreateSteps(static_cast<std::string_view>(scenario_)); }
    >();
    // array -> class
    using Steps_ = decltype(steps_arr);
    struct test_case {
        constexpr test_case(Steps_ steps) : m_steps{steps} {}
        Steps_ m_steps;
        auto operator()(example::Context context) const {
            for (const auto& step : m_steps) {
                step(context);
            }
        }
    };
    // return annon class
    return test_case{steps_arr};
}

// constexpr char feature[329] = 
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


// Scenario needs to be a class
//  A feature can have a background so we need to parse that first 
//   then create a background class which has all the background
//  Each Scenario needs to have a nullable stack pointer to background
//
struct Background {
    constexpr Background() = default;
    // Text should not include any scen
    consteval Background(const std::string_view feature_only_text) {
        using namespace std::string_view_literals;
        // check we have a background
        const auto background_start = feature_only_text.find(BackgroundKeyword);
        if (background_start == std::string_view::npos) { return; }
        const auto background_end = feature_only_text.find(ScenarioKeyword, background_start);
        using namespace constexpr_utils;
        auto background_text = Trim(
            feature_only_text.substr(
                background_start + BackgroundKeyword.size()
              , background_end
                )
            );
        m_background_steps = CreateSteps(background_text);
    }
    consteval auto GetSteps() { return m_background_steps; }
    std::vector<Step> m_background_steps;
};

struct Scenario {
    template<constexpr_utils::NTTPString_view scenario>
    consteval Scenario(Background background) {
        
    }
};

template<constexpr_utils::NTTPString_view feature_>
consteval std::vector<std::string_view> SplitIntoScenarios() {
    // template<size_t N>
    // using string_view = constexpr_utils::NTTPString_view<N>;
    // basic sanity check
    auto feature = static_cast<std::string_view>(feature_);
    auto scenario_pos = feature.find(ScenarioKeyword);
    if (scenario_pos == std::string_view::npos) throw "No scenario found in feature";
    // Background
    auto background = Background{feature.substr(0, scenario_pos)};

    // start filling
    std::vector<std::string_view> scenarios{};
    while(scenario_pos != std::string_view::npos) {
        auto next_scenario_pos = feature.find(ScenarioKeyword, scenario_pos + 1);
        auto scenario = feature.substr(scenario_pos, next_scenario_pos - scenario_pos);
        using namespace constexpr_utils;
        scenario = Trim(scenario);
        if (scenario.size() <= 1) throw "Bad scenario creation"; 
        scenarios.push_back(scenario);
        scenario_pos = next_scenario_pos;
    }
    return scenarios;
}

template<constexpr_utils::NTTPString_view feature_>
consteval auto SplitIntoNTTPScenarios() {
    constexpr auto scenarios_ = constexpr_utils::to_array<
        100, std::string_view, [](){ return SplitIntoScenarios<feature_>(); }
    >();
    std::array<constexpr_utils::string_view, scenarios_.size()> nttp_scenarios{};
    // std::ranges::copy(scenarios_, nttp_scenarios);
    for (size_t i = 0; const std::string_view& scenario : scenarios_ ) {
        if (scenario.size() < 1) throw "Not valid scenario";
        if (constexpr_utils::string_view{ scenario }.size() < 1) throw "Bad creation of CU::string_view"; 
        nttp_scenarios[i] = { scenario }; // implicit conversion
        ++i;
    }
    return nttp_scenarios;
}

namespace {
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
    constexpr auto test = CreateTest<test_str>();

    // constexpr auto test = CreateTest<SplitIntoNTTPScenarios<feature_>()[0]>();
}


