
#include <string_view>
#include <array>
#include <vector>
#include <string>
#include <algorithm>

#include <ctre.hpp>

// Stand-in regex
//  Will require CTRE later
namespace regex {
    using Groups = std::array<std::string_view, 5>;
    using Pattern = std::string_view;
}

namespace gherk_min {


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

template<size_t array_1_size, size_t array_2_size, typename T>
consteval std::array<T, array_1_size + array_2_size> UnionArrays(const std::array<T,array_1_size>& first, const std::array<T, array_2_size>& second) {
    std::array<T, array_1_size + array_2_size> result{};
    auto last = std::copy(first.begin(), first.end(), result.begin());
    std::copy(second.begin(), second.end(), last);
    return result;
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
    consteval size_t size() const { return N; }
    
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


namespace step_defines {

namespace registration {
    template<int index_>
    struct StepDefinition { static constexpr bool is_end = true; };

    /// Registration mechanism:
    ///  Each thing we want to register is a specialisation of a countable
    ///  We then loop through the countable numbers, starting from 0
    ///  When we reach the first countable number that is not specialised we end the loop

    // part of registration mechanism
    template<typename T>
    concept step_end = requires { T::is_end == true; };
    
    // Check template are X<size_t>, i.e. registerable
    template <template <class...> class Template, class... Args>
    void specialization_impl(const Template<Args...>&);

    template <class T, template <size_t...> class Template>
    concept int_specialization_of = requires(const T& t) {
        specialization_impl<Template>(t);
    };
}
// End namespace to start the default, non-specialised, "end" classes

template<size_t index_> struct Given : registration::StepDefinition<index_> {};
template<size_t index_> struct When  : registration::StepDefinition<index_> {};
template<size_t index_> struct Then  : registration::StepDefinition<index_> {};


// Not used, removed to reduce compilation time
#if defined(GHERKMIN_ADD_STEP_END_CONCEPTS)
namespace registration {
    template<typename T> concept given_end = registration::step_end<T> && requires { registration::int_specialization_of<T, Given> == true ; };
    template<typename T> concept when_end  = registration::step_end<T> && requires { registration::int_specialization_of<T, When>  == true ; };
    template<typename T> concept then_end  = registration::step_end<T> && requires { registration::int_specialization_of<T, Then>  == true ; };
}
#endif

}

namespace regex_dep_inversion {
    template<constexpr_utils::NTTPString_view pattern>
    constexpr auto RegexMatches([[maybe_unused]] std::string_view to_match) {
        constexpr auto transformed_pattern = ctll::fixed_string<pattern.size()-1>{pattern.value};
        return ctre::match<transformed_pattern>(to_match);
    }
    using ReGroupType = decltype(RegexMatches<".*">("anything"));
}

namespace user_defined_type_erasure {
    template<typename T, typename Context>
    concept RegexContextInvocable = requires (Context c) { T::StepDefinition(regex::Groups{}, c); };
    template<typename T>
    concept RegexInvocable = requires () { T::StepDefinition(regex::Groups{}); };
    template<typename T, typename Context>
    concept ContextInvocable = requires (Context c) { T::StepDefinition(c); };
    template<typename T>
    concept VoidInvocable = requires () { T::StepDefinition(); };
}

template<typename Context>
using StepDefinitionFunctionSignature = decltype(+[](const regex::Groups&, Context&) -> void {});

template<typename Context>
struct Step {
    constexpr Step() = default;
    constexpr Step(const regex::Groups& re_group, const StepDefinitionFunctionSignature<Context>& func)
        : m_group{re_group}, m_step_code{func}
    {}

    // template<typename Context>
    void operator()(Context& context) const {
        m_step_code(m_group, context);
    }

    regex::Groups m_group;
    StepDefinitionFunctionSignature<Context> m_step_code;
};

template<
    template <size_t> typename StepDefinitionType // Given, When or Then
  , typename Context                              // User defined context
  , size_t N = 1                                  // Current iteration of StepDefinitionType
>               
consteval Step<Context> GetStepDefinition(const std::string_view to_match) 
{
    using step_def_to_check = StepDefinitionType<N>;
    using regex_dep_inversion::RegexMatches;
    if (auto re_groups = RegexMatches(step_def_to_check::GetRegex(), to_match); re_groups.size() > 0) {
        return Step<Context> { re_groups , 
            +[](const regex::Groups& groups, Context& context) -> void {
            using namespace user_defined_type_erasure;
            // some type erasure to allow for user to write functions with multiple signatures
            //  cast to void is to avoid issues with return values
            if constexpr (RegexContextInvocable<step_def_to_check, Context>) {
                return (void)step_def_to_check::StepDefinition(groups, context);
            } else
            if constexpr (RegexInvocable<step_def_to_check>) {
                return (void)step_def_to_check::StepDefinition(groups);
            } else
            if constexpr (ContextInvocable<step_def_to_check, Context>) {
                return (void)step_def_to_check::StepDefinition(context);
            } else
            if constexpr (VoidInvocable<step_def_to_check>) {
                return (void)step_def_to_check::StepDefinition();
            } else {
                // fail
                //  TODO: static_assert(false)
                throw "StepDefinition did not match expected function signature";
            }
        }};
    }
    if constexpr (!step_defines::registration::step_end<StepDefinitionType<N+1>>) {
        return GetStepDefinition<StepDefinitionType, Context, N+1>(to_match);
    } else {
        throw "Step string, e.g. Given: XXX  did not match any regex expressions"
                "\n (reached end of step_defines, please make sure XXX matches a regex expression)";
    }
}
template<typename Context>
consteval auto GetGivenDefinition(const std::string_view to_match) 
{
    return GetStepDefinition<step_defines::Given, Context>(to_match);
}

template<typename Context>
consteval auto GetWhenDefinition(const std::string_view to_match) 
{
    return GetStepDefinition<step_defines::When, Context>(to_match);
}

template<typename Context>
consteval auto GetThenDefinition(const std::string_view to_match) 
{
    return GetStepDefinition<step_defines::Then, Context>(to_match);
}

enum class StepType { Given, When, Then, And };

// Should be moved to a class 
//  Then we can have a concept trickle through after analysing the first line
namespace keywords {
template<typename T>
concept Language = requires {
    T::Scenario;
    T::Background;
    T::Given;
    T::When;
    T::Then;
    T::And;
};

struct en {
    static constexpr std::string_view Scenario = "Scenario: ";
    static constexpr std::string_view Background = "Background: ";
    static constexpr std::string_view Given = "Given: ";
    static constexpr std::string_view When = "When ";
    static constexpr std::string_view Then = "Then ";
    static constexpr std::string_view And = "And ";
};

using default_lang = en;
}

/// Returns the step type and the remaining bits of the string
template<keywords::Language Keywords = keywords::default_lang>
constexpr std::pair<StepType, std::string_view> GetStepType(std::string_view from_line) {
    using namespace constexpr_utils;
    // remove all whitespace from `from_line`
    from_line = TrimStart(from_line);
    // using namespace std::string_view_literals;
    using k = Keywords;
     for (auto search : {k::Given, k::When, k::Then, k::And}) {
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

template<typename Context>
consteval auto GetDefinitionsFactory(StepType step_type) {
    switch (step_type) {
        case StepType::Given: return GetGivenDefinition<Context>;
        case StepType::When: return GetWhenDefinition<Context>;
        case StepType::Then: return GetThenDefinition<Context>;
        case StepType::And: throw "And is not a valid step type for factory";
    }
    throw "String did not start with any expected keywords";
}

// previous really should be a strong type or something more useful
template<typename Context, keywords::Language Keywords = keywords::default_lang>
consteval auto GetStepDefinition(std::string_view to_match, StepType previous) {
    using namespace constexpr_utils;
    // Remove whitespace from start of line
    to_match = TrimStart(to_match);
    const auto contains = [to_match](std::string_view search) {
        return to_match.find(search) != std::string_view::npos;
    };

    if (contains(Keywords::And)) {
        // using namespace std::string_view_literals;
        to_match.remove_prefix(Keywords::And.size());
        GetDefinitionsFactory<Context>(previous)(to_match);
    }
    auto [step_type, remaining] = GetStepType(to_match);
    return std::make_tuple(GetDefinitionsFactory<Context>(step_type)(remaining), step_type);
}

template<keywords::Language Keywords = keywords::default_lang>
consteval std::string_view RemoveScenarioHeader(std::string_view scenario) {
    using constexpr_utils::Trim;
    const auto start_scenario_pos = scenario.find(Keywords::Scenario) + Keywords::Scenario.size(); 
    const auto start_steps_pos = scenario.find('\n', start_scenario_pos);
    scenario = Trim(scenario.substr(start_steps_pos + 1));
    return scenario;
}

// template<typename Context>
template<typename Context, keywords::Language Keywords = keywords::default_lang>
consteval std::vector<Step<Context>> CreateSteps(std::string_view scenario /*, std::vector<Step>&& existing = {}*/) {
    using constexpr_utils::Trim;
    StepType previous{};
    std::vector<Step<Context>> steps = {}; // existing;

    
    for (; scenario.size() > 0; scenario = Trim(scenario)) {
        const auto line_end = scenario.find('\n');
        const auto step_line = scenario.substr(0, line_end);
        auto [step, current] = GetStepDefinition<Context, Keywords>(step_line, previous);
        steps.push_back(step);
        previous = current;
        if (line_end == std::string_view::npos) return steps;
        scenario = scenario.substr(line_end);
    }
    return steps;
}

template<constexpr_utils::NTTPString_view scenario_, typename Context, keywords::Language Keywords = keywords::default_lang>
consteval auto CreateTest() {
    // should not be accessed as default initialised
    //  if it is that means the first value was And
    using namespace constexpr_utils;
    // vector -> array
    auto steps_arr = constexpr_utils::to_array<20, Step<Context>, 
        []() { return CreateSteps<Context, Keywords>(static_cast<std::string_view>(scenario_)); }
    >();
    // array -> class
    using Steps_ = decltype(steps_arr);
    struct test_case {
        constexpr test_case(Steps_ steps) : m_steps{steps} {}
        Steps_ m_steps;
        void operator()(Context context) const {
            for (const auto& step : m_steps) {
                step(context);
            }
        }
    };
    // return anonymous class
    return test_case{steps_arr};
}

template<typename Context>
struct Background {
    constexpr Background() = default;

    // Text should not include any scenarios
    template<keywords::Language Keywords = keywords::default_lang>
    consteval Background(const std::string_view feature_only_text) {
        using namespace std::string_view_literals;
        // check we have a background
        const auto background_start = feature_only_text.find(Keywords::Background);
        if (background_start == std::string_view::npos) { return; }
        const auto background_end = feature_only_text.find(Keywords::Scenario, background_start);
        using namespace constexpr_utils;
        auto background_text = Trim(
            feature_only_text.substr(
                background_start + Keywords::Background.size()
              , background_end
                )
            );
        m_background_steps = CreateSteps<Context, Keywords>(background_text);
    }
    consteval auto GetSteps() { return m_background_steps; }
    std::vector<Step<Context>> m_background_steps;
};

template<constexpr_utils::NTTPString_view feature_, typename Context = int>
consteval std::vector<std::string_view> SplitIntoScenarios() {
    // template<size_t N>
    // using string_view = constexpr_utils::NTTPString_view<N>;
    constexpr keywords::Language auto k = /*GetLanguage(feature_)*/ keywords::default_lang{};

    // basic sanity check
    auto feature = static_cast<std::string_view>(feature_);
    auto scenario_pos = feature.find(k.Scenario);
    if (scenario_pos == std::string_view::npos) throw "No scenario found in feature";
    // Background
    auto background = Background<Context>{feature.substr(0, scenario_pos)};

    // start filling
    std::vector<std::string_view> scenarios{};
    while(scenario_pos != std::string_view::npos) {
        auto next_scenario_pos = feature.find(k.Scenario, scenario_pos + 1);
        auto scenario = feature.substr(scenario_pos, next_scenario_pos - scenario_pos);
        using namespace constexpr_utils;
        scenario = Trim(scenario);
        if (scenario.size() <= 1) throw "Bad scenario creation"; 
        scenarios.push_back(scenario);
        scenario_pos = next_scenario_pos;
    }
    // TODO, add background to each (?)
    return scenarios;
}

template<constexpr_utils::NTTPString_view feature_>
consteval auto SplitIntoNTTPScenarios() {
    constexpr auto scenarios_ = constexpr_utils::to_array<
        100, std::string_view, [](){ return SplitIntoScenarios<feature_>(); }
    >();
    std::array<constexpr_utils::string_view, scenarios_.size()> nttp_scenarios{};
    for (size_t i = 0; const std::string_view& scenario : scenarios_ ) {
        if (scenario.size() < 1) throw "Not valid scenario";
        if (constexpr_utils::string_view{ scenario }.size() < 1) throw "Bad creation of CU::string_view"; 
        nttp_scenarios[i] = { scenario }; // convert
        ++i;
    }
    return nttp_scenarios;
}

}
