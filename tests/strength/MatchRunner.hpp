#ifndef BOSON_MATCH_RUNNER_HPP
#define BOSON_MATCH_RUNNER_HPP

#include "strength/StrengthTypes.hpp"
#include "strength/OpeningBook.hpp"
#include "strength/Statistics.hpp"
#include "board/Position.hpp"
#include <memory>
#include <string>
#include <string_view>

namespace Boson {

class IUciEngine {
public:
    virtual ~IUciEngine() = default;
    virtual void sendCommand(std::string_view cmd) = 0;
    virtual std::string getBestMove(int timeoutMs = 5000) = 0;
    virtual void setParameters(const EngineParameters& params) = 0;
    [[nodiscard]] virtual const std::string& getName() const noexcept = 0;
    [[nodiscard]] virtual const EngineParameters& getParameters() const noexcept = 0;
    [[nodiscard]] virtual std::string getMetadata() const { return ""; }
};

class InProcessUciEngine : public IUciEngine {
public:
    InProcessUciEngine(std::string name, const EngineParameters& params, size_t hashMb = 16);
    void sendCommand(std::string_view cmd) override;
    std::string getBestMove(int timeoutMs = 5000) override;
    void setParameters(const EngineParameters& params) override;
    [[nodiscard]] const std::string& getName() const noexcept override { return m_name; }
    [[nodiscard]] const EngineParameters& getParameters() const noexcept override { return m_params; }
    [[nodiscard]] std::string getMetadata() const override;

private:
    std::string m_name;
    EngineParameters m_params;
    size_t m_hashMb{16};
    Position m_pos;
    std::string m_lastBestMove{};
};

class MatchRunner {
public:
    static MatchRecord runMatch(const MatchConfig& config, IUciEngine* customEngineA = nullptr, IUciEngine* customEngineB = nullptr);
    static GameRecord playGame(uint32_t gameId, IUciEngine& whiteEngine, IUciEngine& blackEngine, const OpeningEntry& opening, const MatchConfig& config);
    [[nodiscard]] static std::string exportPgn(const GameRecord& game);
};

} // namespace Boson

#endif // BOSON_MATCH_RUNNER_HPP
