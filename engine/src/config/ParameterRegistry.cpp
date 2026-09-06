#include "config/ParameterRegistry.hpp"
#include "search/Search.hpp"
#include "search/SearchController.hpp"
#include "benchmark/BenchmarkRunner.hpp"
#include "evaluation/Evaluator.hpp"
#include "system/EngineInfo.hpp"
#include <cctype>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Boson {

ParameterRegistry& ParameterRegistry::getInstance() noexcept {
    static ParameterRegistry instance;
    return instance;
}

ParameterRegistry::ParameterRegistry() {
    registerDefaultParameters();
}

void ParameterRegistry::registerDefaultParameters() {
    // 1. Memory Subsystem
    registerParam(ParameterDescriptor::createInt(
        "Hash", "Transposition table memory allocation size in megabytes",
        ParamGroup::Memory, 16, 1, 65536, ResetRequirement::ClearTT));

    // 2. Search Pruning
    registerParam(ParameterDescriptor::createInt(
        "NMP_BaseReduction", "Null-move pruning base depth reduction (R)",
        ParamGroup::SearchPruning, 2, 1, 6, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "NMP_DepthDivisor", "Minimum depth threshold to trigger null-move pruning",
        ParamGroup::SearchPruning, 3, 1, 10, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "RFP_MarginBase", "Base margin in centipawns per depth for Reverse Futility Pruning",
        ParamGroup::SearchPruning, 75, 20, 200, ResetRequirement::None, true, true));

    // 3. Search Reductions (LMR)
    registerParam(ParameterDescriptor::createDouble(
        "LMR_Base", "Late Move Reduction logarithmic scaling base offset",
        ParamGroup::SearchReductions, 0.5, 0.0, 3.0, ResetRequirement::None));
    registerParam(ParameterDescriptor::createDouble(
        "LMR_Divisor", "Late Move Reduction logarithmic scaling divisor",
        ParamGroup::SearchReductions, 1.95, 0.5, 5.0, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "LMR_MinDepth", "Minimum depth before LMR is applied",
        ParamGroup::SearchReductions, 3, 1, 10, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "LMR_MinMoveCount", "Minimum move index threshold before LMR is applied",
        ParamGroup::SearchReductions, 4, 1, 20, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "LMR_ImprovingBonus", "Additional depth reduction applied to eligible quiet moves when position is improving",
        ParamGroup::SearchReductions, 1, 0, 2, ResetRequirement::None, true, true));

    // 4. Search Selectivity (Aspiration)
    registerParam(ParameterDescriptor::createInt(
        "Aspiration_InitialWindow", "Initial half-window delta for aspiration search in centipawns",
        ParamGroup::SearchSelectivity, 30, 5, 200, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "Aspiration_MaxDelta", "Maximum window expansion threshold before falling back to full window",
        ParamGroup::SearchSelectivity, 400, 50, 2000, ResetRequirement::None));

    // 5. Search Move Ordering
    registerParam(ParameterDescriptor::createInt(
        "History_MaxScore", "Maximum saturation ceiling for history heuristic tables",
        ParamGroup::SearchOrdering, 16384, 256, 65536, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "Killer_SlotCount", "Number of killer move storage slots per ply",
        ParamGroup::SearchOrdering, 2, 1, 4, ResetRequirement::None));

    // 6. Evaluation Subsystem
    registerParam(ParameterDescriptor::createInt(
        "PawnValue", "Pawn base evaluation value in centipawns",
        ParamGroup::Evaluation, 100, 1, 2000, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "KnightValue", "Knight base evaluation value in centipawns",
        ParamGroup::Evaluation, 320, 1, 2000, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "BishopValue", "Bishop base evaluation value in centipawns",
        ParamGroup::Evaluation, 330, 1, 2000, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "RookValue", "Rook base evaluation value in centipawns",
        ParamGroup::Evaluation, 500, 1, 3000, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "QueenValue", "Queen base evaluation value in centipawns",
        ParamGroup::Evaluation, 900, 1, 5000, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "MaxCorrection", "Maximum correction history magnitude cap in centipawns",
        ParamGroup::Evaluation, 1024, 0, 4096, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "CorrScaleFactor", "Correction history weight scaling divisor",
        ParamGroup::Evaluation, 256, 1, 2048, ResetRequirement::None));

    // 7. Time Management
    registerParam(ParameterDescriptor::createInt(
        "Time_NodeCheckPeriod", "Search node polling frequency for time check",
        ParamGroup::TimeManagement, 2048, 64, 65536, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "Time_MoveAllocationDivisor", "Divisor applied to remaining time for move allocation",
        ParamGroup::TimeManagement, 20, 5, 100, ResetRequirement::None));
    registerParam(ParameterDescriptor::createInt(
        "Time_IncDivisor", "Divisor applied to increment addition",
        ParamGroup::TimeManagement, 2, 1, 10, ResetRequirement::None));
    registerParam(ParameterDescriptor::createDouble(
        "Time_HardLimitMultiplier", "Multiplier on allocated time to calculate hard search limit",
        ParamGroup::TimeManagement, 3.0, 1.0, 10.0, ResetRequirement::None));

    // 8. Debug & Ablation Flags
    registerParam(ParameterDescriptor::createBool(
        "Enable_NMP", "Enable Null Move Pruning heuristic",
        ParamGroup::Debug, true, ResetRequirement::None));
    registerParam(ParameterDescriptor::createBool(
        "Enable_LMR", "Enable Late Move Reductions heuristic",
        ParamGroup::Debug, true, ResetRequirement::None));
    registerParam(ParameterDescriptor::createBool(
        "Enable_Aspiration", "Enable Aspiration Windows search",
        ParamGroup::Debug, true, ResetRequirement::None));
    registerParam(ParameterDescriptor::createBool(
        "Enable_CMH", "Enable Counter Move History heuristic",
        ParamGroup::Debug, true, ResetRequirement::None));
    registerParam(ParameterDescriptor::createBool(
        "Enable_ContHist", "Enable Continuation History heuristic",
        ParamGroup::Debug, true, ResetRequirement::None));
    registerParam(ParameterDescriptor::createBool(
        "Enable_CorrHist", "Enable Evaluation Correction History heuristic",
        ParamGroup::Debug, true, ResetRequirement::None));
}

std::string ParameterRegistry::normalizeName(std::string_view name) {
    std::string norm;
    norm.reserve(name.size());
    for (char c : name) {
        if (c == ' ' || c == '-') {
            norm.push_back('_');
        } else {
            norm.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return norm;
}

bool ParameterRegistry::parseCliParam(std::string_view spec, std::string& outName, std::string& outVal) {
    if (spec.starts_with("--param=")) {
        spec = spec.substr(8);
    } else if (spec.starts_with("--param")) {
        spec = spec.substr(7);
    }
    size_t eqPos = spec.find('=');
    if (eqPos == std::string_view::npos || eqPos == 0 || eqPos + 1 > spec.size()) {
        return false;
    }
    std::string_view namePart = spec.substr(0, eqPos);
    std::string_view valPart = spec.substr(eqPos + 1);

    while (!namePart.empty() && std::isspace(static_cast<unsigned char>(namePart.front()))) namePart.remove_prefix(1);
    while (!namePart.empty() && std::isspace(static_cast<unsigned char>(namePart.back()))) namePart.remove_suffix(1);
    while (!valPart.empty() && std::isspace(static_cast<unsigned char>(valPart.front()))) valPart.remove_prefix(1);
    while (!valPart.empty() && std::isspace(static_cast<unsigned char>(valPart.back()))) valPart.remove_suffix(1);

    if (namePart.empty() || valPart.empty()) {
        return false;
    }

    outName = std::string(namePart);
    outVal = std::string(valPart);
    return true;
}

bool ParameterRegistry::registerParam(const ParameterDescriptor& desc) {
    std::string norm = normalizeName(desc.name);
    if (m_lookup.find(norm) != m_lookup.end()) {
        return false;
    }
    size_t idx = m_params.size();
    m_params.push_back(desc);
    m_lookup[norm] = idx;
    return true;
}

const ParameterDescriptor* ParameterRegistry::getParam(std::string_view name) const noexcept {
    std::string norm = normalizeName(name);
    auto it = m_lookup.find(norm);
    if (it == m_lookup.end()) return nullptr;
    return &m_params[it->second];
}

ParameterDescriptor* ParameterRegistry::getMutableParam(std::string_view name) noexcept {
    std::string norm = normalizeName(name);
    auto it = m_lookup.find(norm);
    if (it == m_lookup.end()) return nullptr;
    return &m_params[it->second];
}

bool ParameterRegistry::hasParam(std::string_view name) const noexcept {
    return getParam(name) != nullptr;
}

bool ParameterRegistry::setParam(std::string_view name, const ParamValue& val) {
    ParameterDescriptor* desc = getMutableParam(name);
    if (!desc) return false;
    if (!desc->isValid(val)) return false;

    desc->currentValue = val;

    switch (desc->resetRequirement) {
    case ResetRequirement::ClearTT:
        m_clearTTCount++;
        Search::s_tt.clear();
        break;
    case ResetRequirement::ClearHistory:
        Search::clearCMH();
        Search::clearContHist();
        break;
    case ResetRequirement::FullReset:
        m_clearTTCount++;
        BenchmarkRunner::resetSearchState();
        break;
    case ResetRequirement::None:
        break;
    }

    if (m_resetCallback) {
        m_resetCallback(desc->resetRequirement, desc->name);
    }

    syncToEngineParameters(SearchController::getInstance().getMutableParams());
    return true;
}

bool ParameterRegistry::setParamFromString(std::string_view name, std::string_view strVal) {
    ParameterDescriptor* desc = getMutableParam(name);
    if (!desc) return false;

    while (!strVal.empty() && std::isspace(static_cast<unsigned char>(strVal.front()))) strVal.remove_prefix(1);
    while (!strVal.empty() && std::isspace(static_cast<unsigned char>(strVal.back()))) strVal.remove_suffix(1);
    if (strVal.empty()) return false;

    switch (desc->type) {
    case ParamType::Int: {
        try {
            size_t idx = 0;
            std::string s(strVal);
            long long val = std::stoll(s, &idx);
            if (idx != s.size()) return false;
            return setParam(name, static_cast<int64_t>(val));
        } catch (...) {
            return false;
        }
    }
    case ParamType::Bool: {
        std::string s;
        for (char c : strVal) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        if (s == "true" || s == "1" || s == "yes" || s == "on") {
            return setParam(name, true);
        } else if (s == "false" || s == "0" || s == "no" || s == "off") {
            return setParam(name, false);
        }
        return false;
    }
    case ParamType::Double: {
        try {
            size_t idx = 0;
            std::string s(strVal);
            double val = std::stod(s, &idx);
            if (idx != s.size()) return false;
            return setParam(name, val);
        } catch (...) {
            return false;
        }
    }
    }
    return false;
}

int64_t ParameterRegistry::getInt(std::string_view name) const {
    const auto* p = getParam(name);
    if (!p) return 0;
    return p->getInt();
}

bool ParameterRegistry::getBool(std::string_view name) const {
    const auto* p = getParam(name);
    if (!p) return false;
    return p->getBool();
}

double ParameterRegistry::getDouble(std::string_view name) const {
    const auto* p = getParam(name);
    if (!p) return 0.0;
    return p->getDouble();
}

void ParameterRegistry::resetToDefaults() {
    for (auto& p : m_params) {
        p.currentValue = p.defaultValue;
    }
    syncToEngineParameters(SearchController::getInstance().getMutableParams());
}

void ParameterRegistry::syncToEngineParameters(EngineParameters& params) const {
    params.search.lmrBase = getDouble("LMR_Base");
    params.search.lmrDivisor = getDouble("LMR_Divisor");
    params.search.lmrMinDepth = static_cast<int>(getInt("LMR_MinDepth"));
    params.search.lmrMinMoveCount = static_cast<int>(getInt("LMR_MinMoveCount"));
    params.search.nmpMinDepth = static_cast<int>(getInt("NMP_DepthDivisor"));
    params.search.nmpReduction = static_cast<int>(getInt("NMP_BaseReduction"));
    params.search.aspirationInitialDelta = static_cast<int>(getInt("Aspiration_InitialWindow"));
    params.search.aspirationMaxDelta = static_cast<int>(getInt("Aspiration_MaxDelta"));
    params.search.killerSlotCount = static_cast<int>(getInt("Killer_SlotCount"));
    params.search.rfpMarginBase = static_cast<int>(getInt("RFP_MarginBase"));
    params.search.lmrImprovingBonus = static_cast<int>(getInt("LMR_ImprovingBonus"));

    params.eval.pawnValue = static_cast<int>(getInt("PawnValue"));
    params.eval.knightValue = static_cast<int>(getInt("KnightValue"));
    params.eval.bishopValue = static_cast<int>(getInt("BishopValue"));
    params.eval.rookValue = static_cast<int>(getInt("RookValue"));
    params.eval.queenValue = static_cast<int>(getInt("QueenValue"));
    params.eval.maxCorrection = static_cast<int>(getInt("MaxCorrection"));
    params.eval.corrScaleFactor = static_cast<int>(getInt("CorrScaleFactor"));

    params.time.nodeCheckPeriod = static_cast<uint64_t>(getInt("Time_NodeCheckPeriod"));
    params.time.allocDivisor = static_cast<int>(getInt("Time_MoveAllocationDivisor"));
    params.time.incDivisor = static_cast<int>(getInt("Time_IncDivisor"));
    params.time.hardLimitMultiplier = getDouble("Time_HardLimitMultiplier");

    params.debug.enableNMP = getBool("Enable_NMP");
    params.debug.enableLMR = getBool("Enable_LMR");
    params.debug.enableAspiration = getBool("Enable_Aspiration");
    params.debug.enableCMH = getBool("Enable_CMH");
    params.debug.enableContHist = getBool("Enable_ContHist");
    params.debug.enableCorrHist = getBool("Enable_CorrHist");
}

void ParameterRegistry::loadFromEngineParameters(const EngineParameters& params) {
    setParam("LMR_Base", params.search.lmrBase);
    setParam("LMR_Divisor", params.search.lmrDivisor);
    setParam("LMR_MinDepth", static_cast<int64_t>(params.search.lmrMinDepth));
    setParam("LMR_MinMoveCount", static_cast<int64_t>(params.search.lmrMinMoveCount));
    setParam("LMR_ImprovingBonus", static_cast<int64_t>(params.search.lmrImprovingBonus));
    setParam("NMP_DepthDivisor", static_cast<int64_t>(params.search.nmpMinDepth));
    setParam("NMP_BaseReduction", static_cast<int64_t>(params.search.nmpReduction));
    setParam("RFP_MarginBase", static_cast<int64_t>(params.search.rfpMarginBase));
    setParam("Aspiration_InitialWindow", static_cast<int64_t>(params.search.aspirationInitialDelta));
    setParam("Aspiration_MaxDelta", static_cast<int64_t>(params.search.aspirationMaxDelta));
    setParam("Killer_SlotCount", static_cast<int64_t>(params.search.killerSlotCount));

    setParam("PawnValue", static_cast<int64_t>(params.eval.pawnValue));
    setParam("KnightValue", static_cast<int64_t>(params.eval.knightValue));
    setParam("BishopValue", static_cast<int64_t>(params.eval.bishopValue));
    setParam("RookValue", static_cast<int64_t>(params.eval.rookValue));
    setParam("QueenValue", static_cast<int64_t>(params.eval.queenValue));
    setParam("MaxCorrection", static_cast<int64_t>(params.eval.maxCorrection));
    setParam("CorrScaleFactor", static_cast<int64_t>(params.eval.corrScaleFactor));

    setParam("Time_NodeCheckPeriod", static_cast<int64_t>(params.time.nodeCheckPeriod));
    setParam("Time_MoveAllocationDivisor", static_cast<int64_t>(params.time.allocDivisor));
    setParam("Time_IncDivisor", static_cast<int64_t>(params.time.incDivisor));
    setParam("Time_HardLimitMultiplier", params.time.hardLimitMultiplier);

    setParam("Enable_NMP", params.debug.enableNMP);
    setParam("Enable_LMR", params.debug.enableLMR);
    setParam("Enable_Aspiration", params.debug.enableAspiration);
    setParam("Enable_CMH", params.debug.enableCMH);
    setParam("Enable_ContHist", params.debug.enableContHist);
    setParam("Enable_CorrHist", params.debug.enableCorrHist);
}

void ParameterRegistry::printUciOptions(std::ostream& os) const {
    for (const auto& p : m_params) {
        if (!p.exposeUci) continue;
        switch (p.type) {
        case ParamType::Int:
            os << "option name " << p.name << " type spin default "
               << std::get<int64_t>(p.defaultValue) << " min "
               << std::get<int64_t>(p.minValue) << " max "
               << std::get<int64_t>(p.maxValue) << "\n";
            break;
        case ParamType::Bool:
            os << "option name " << p.name << " type check default "
               << (std::get<bool>(p.defaultValue) ? "true" : "false") << "\n";
            break;
        case ParamType::Double:
            os << "option name " << p.name << " type string default "
               << std::get<double>(p.defaultValue) << "\n";
            break;
        }
    }
}

bool ParameterRegistry::handleSetOption(std::string_view line, std::ostream& os) {
    (void)os;
    std::string lowerLine;
    lowerLine.reserve(line.size());
    for (char c : line) lowerLine.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    size_t namePos = lowerLine.find("name ");
    if (namePos == std::string::npos) return false;
    namePos += 5;

    size_t valPos = lowerLine.find(" value ", namePos);
    std::string name;
    std::string val;

    if (valPos != std::string::npos) {
        name = std::string(line.substr(namePos, valPos - namePos));
        val = std::string(line.substr(valPos + 7));
    } else {
        name = std::string(line.substr(namePos));
        val = "";
    }

    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.erase(name.begin());
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
    while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(val.begin());
    while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();

    if (name.empty()) return false;

    const ParameterDescriptor* desc = getParam(name);
    if (!desc) {
        return false;
    }

    if (val.empty() && desc->type == ParamType::Bool) {
        return setParam(name, true);
    }

    return setParamFromString(name, val);
}

bool ParameterRegistry::handleUciCommand(std::string_view line, std::ostream& os) {
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) line.remove_prefix(1);
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.remove_suffix(1);

    if (line.empty()) return true;

    if (line == "uci") {
        os << "id name " << EngineInfo::getName() << " " << EngineInfo::getVersion() << "\n";
        os << "id author " << EngineInfo::AUTHOR << "\n";
        printUciOptions(os);
        os << "uciok\n";
        return true;
    }

    if (line == "isready") {
        os << "readyok\n";
        return true;
    }

    if (line.starts_with("setoption")) {
        return handleSetOption(line, os);
    }

    if (line == "ucinewgame") {
        BenchmarkRunner::resetSearchState();
        return true;
    }

    return false;
}

std::string ParameterRegistry::serializeJson() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"schemaVersion\": \"1.0.0\",\n";
    oss << "  \"parameterCount\": " << m_params.size() << ",\n";
    oss << "  \"parameters\": [\n";
    for (size_t i = 0; i < m_params.size(); ++i) {
        const auto& p = m_params[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << p.name << "\",\n";
        oss << "      \"description\": \"" << p.description << "\",\n";
        oss << "      \"type\": \"" << paramTypeToString(p.type) << "\",\n";
        oss << "      \"group\": \"" << paramGroupToString(p.group) << "\",\n";
        oss << "      \"resetRequirement\": \"" << resetRequirementToString(p.resetRequirement) << "\",\n";
        switch (p.type) {
        case ParamType::Int:
            oss << "      \"default\": " << std::get<int64_t>(p.defaultValue) << ",\n";
            oss << "      \"min\": " << std::get<int64_t>(p.minValue) << ",\n";
            oss << "      \"max\": " << std::get<int64_t>(p.maxValue) << ",\n";
            oss << "      \"current\": " << std::get<int64_t>(p.currentValue) << "\n";
            break;
        case ParamType::Bool:
            oss << "      \"default\": " << (std::get<bool>(p.defaultValue) ? "true" : "false") << ",\n";
            oss << "      \"current\": " << (std::get<bool>(p.currentValue) ? "true" : "false") << "\n";
            break;
        case ParamType::Double:
            oss << "      \"default\": " << std::get<double>(p.defaultValue) << ",\n";
            oss << "      \"min\": " << std::get<double>(p.minValue) << ",\n";
            oss << "      \"max\": " << std::get<double>(p.maxValue) << ",\n";
            oss << "      \"current\": " << std::get<double>(p.currentValue) << "\n";
            break;
        }
        oss << "    }" << (i + 1 < m_params.size() ? "," : "") << "\n";
    }
    oss << "  ]\n";
    oss << "}";
    return oss.str();
}

} // namespace Boson
