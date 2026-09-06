#ifndef BOSON_PARAMETER_REGISTRY_HPP
#define BOSON_PARAMETER_REGISTRY_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <unordered_map>
#include <functional>
#include <iostream>
#include "config/EngineParameters.hpp"

namespace Boson {

enum class ParamType : uint8_t {
    Int,
    Bool,
    Double
};

using ParamValue = std::variant<int64_t, bool, double>;

enum class ParamGroup : uint8_t {
    Memory,
    SearchPruning,
    SearchReductions,
    SearchSelectivity,
    SearchOrdering,
    Evaluation,
    TimeManagement,
    Debug
};

enum class ResetRequirement : uint8_t {
    None,
    ClearTT,
    ClearHistory,
    FullReset
};

[[nodiscard]] constexpr std::string_view paramTypeToString(ParamType type) noexcept {
    switch (type) {
    case ParamType::Int: return "Int";
    case ParamType::Bool: return "Bool";
    case ParamType::Double: return "Double";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view paramGroupToString(ParamGroup group) noexcept {
    switch (group) {
    case ParamGroup::Memory: return "Memory";
    case ParamGroup::SearchPruning: return "SearchPruning";
    case ParamGroup::SearchReductions: return "SearchReductions";
    case ParamGroup::SearchSelectivity: return "SearchSelectivity";
    case ParamGroup::SearchOrdering: return "SearchOrdering";
    case ParamGroup::Evaluation: return "Evaluation";
    case ParamGroup::TimeManagement: return "TimeManagement";
    case ParamGroup::Debug: return "Debug";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view resetRequirementToString(ResetRequirement req) noexcept {
    switch (req) {
    case ResetRequirement::None: return "None";
    case ResetRequirement::ClearTT: return "ClearTT";
    case ResetRequirement::ClearHistory: return "ClearHistory";
    case ResetRequirement::FullReset: return "FullReset";
    }
    return "None";
}

struct ParameterDescriptor {
    std::string name{};
    std::string description{};
    ParamType type{ParamType::Int};
    ParamGroup group{ParamGroup::Debug};
    ResetRequirement resetRequirement{ResetRequirement::None};
    ParamValue defaultValue{int64_t{0}};
    ParamValue minValue{int64_t{0}};
    ParamValue maxValue{int64_t{0}};
    ParamValue currentValue{int64_t{0}};
    bool exposeUci{true};
    bool tunableSpsa{false};

    [[nodiscard]] int64_t getInt() const {
        return std::get<int64_t>(currentValue);
    }
    [[nodiscard]] bool getBool() const {
        return std::get<bool>(currentValue);
    }
    [[nodiscard]] double getDouble() const {
        return std::get<double>(currentValue);
    }

    [[nodiscard]] bool isValid(const ParamValue& val) const noexcept {
        switch (type) {
        case ParamType::Int: {
            if (!std::holds_alternative<int64_t>(val)) return false;
            int64_t v = std::get<int64_t>(val);
            int64_t minV = std::get<int64_t>(minValue);
            int64_t maxV = std::get<int64_t>(maxValue);
            return v >= minV && v <= maxV;
        }
        case ParamType::Bool: {
            return std::holds_alternative<bool>(val);
        }
        case ParamType::Double: {
            if (!std::holds_alternative<double>(val)) return false;
            double v = std::get<double>(val);
            double minV = std::get<double>(minValue);
            double maxV = std::get<double>(maxValue);
            return v >= minV && v <= maxV;
        }
        }
        return false;
    }

    static ParameterDescriptor createInt(std::string name, std::string desc, ParamGroup group,
                                         int64_t defVal, int64_t minVal, int64_t maxVal,
                                         ResetRequirement reset = ResetRequirement::None,
                                         bool exposeUci = true, bool tunableSpsa = false) {
        ParameterDescriptor d;
        d.name = std::move(name);
        d.description = std::move(desc);
        d.type = ParamType::Int;
        d.group = group;
        d.resetRequirement = reset;
        d.defaultValue = defVal;
        d.minValue = minVal;
        d.maxValue = maxVal;
        d.currentValue = defVal;
        d.exposeUci = exposeUci;
        d.tunableSpsa = tunableSpsa;
        return d;
    }

    static ParameterDescriptor createBool(std::string name, std::string desc, ParamGroup group,
                                          bool defVal,
                                          ResetRequirement reset = ResetRequirement::None,
                                          bool exposeUci = true, bool tunableSpsa = false) {
        ParameterDescriptor d;
        d.name = std::move(name);
        d.description = std::move(desc);
        d.type = ParamType::Bool;
        d.group = group;
        d.resetRequirement = reset;
        d.defaultValue = defVal;
        d.minValue = false;
        d.maxValue = true;
        d.currentValue = defVal;
        d.exposeUci = exposeUci;
        d.tunableSpsa = tunableSpsa;
        return d;
    }

    static ParameterDescriptor createDouble(std::string name, std::string desc, ParamGroup group,
                                            double defVal, double minVal, double maxVal,
                                            ResetRequirement reset = ResetRequirement::None,
                                            bool exposeUci = true, bool tunableSpsa = false) {
        ParameterDescriptor d;
        d.name = std::move(name);
        d.description = std::move(desc);
        d.type = ParamType::Double;
        d.group = group;
        d.resetRequirement = reset;
        d.defaultValue = defVal;
        d.minValue = minVal;
        d.maxValue = maxVal;
        d.currentValue = defVal;
        d.exposeUci = exposeUci;
        d.tunableSpsa = tunableSpsa;
        return d;
    }
};

class ParameterRegistry {
public:
    static ParameterRegistry& getInstance() noexcept;

    // Parameter Registration
    bool registerParam(const ParameterDescriptor& desc);

    // Parameter Lookup
    [[nodiscard]] const ParameterDescriptor* getParam(std::string_view name) const noexcept;
    [[nodiscard]] ParameterDescriptor* getMutableParam(std::string_view name) noexcept;
    [[nodiscard]] bool hasParam(std::string_view name) const noexcept;

    // Parameter Modification
    bool setParam(std::string_view name, const ParamValue& val);
    bool setParamFromString(std::string_view name, std::string_view strVal);

    // Typed Convenience Getters
    [[nodiscard]] int64_t getInt(std::string_view name) const;
    [[nodiscard]] bool getBool(std::string_view name) const;
    [[nodiscard]] double getDouble(std::string_view name) const;

    // Reset & Synchronization
    void resetToDefaults();
    void syncToEngineParameters(EngineParameters& params) const;
    void loadFromEngineParameters(const EngineParameters& params);

    // UCI Option Emission & Command Handling
    void printUciOptions(std::ostream& os = std::cout) const;
    bool handleUciCommand(std::string_view line, std::ostream& os = std::cout);
    bool handleSetOption(std::string_view line, std::ostream& os = std::cout);

    // Serialization
    [[nodiscard]] std::string serializeJson() const;

    // Reset Hook / Notification
    using ResetCallback = std::function<void(ResetRequirement, std::string_view)>;
    void setResetCallback(ResetCallback cb) { m_resetCallback = std::move(cb); }
    [[nodiscard]] uint64_t getClearTTCount() const noexcept { return m_clearTTCount; }
    void resetClearTTCount() noexcept { m_clearTTCount = 0; }

    // Introspection
    [[nodiscard]] const std::vector<ParameterDescriptor>& getAllParams() const noexcept { return m_params; }
    [[nodiscard]] size_t size() const noexcept { return m_params.size(); }

    // Helpers
    static std::string normalizeName(std::string_view name);
    static bool parseCliParam(std::string_view spec, std::string& outName, std::string& outVal);

private:
    ParameterRegistry();
    void registerDefaultParameters();

    std::vector<ParameterDescriptor> m_params;
    std::unordered_map<std::string, size_t> m_lookup;
    ResetCallback m_resetCallback;
    uint64_t m_clearTTCount = 0;
};

} // namespace Boson

#endif // BOSON_PARAMETER_REGISTRY_HPP
