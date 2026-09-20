#ifndef BOSON_MATCH_RUNNER_HPP
#define BOSON_MATCH_RUNNER_HPP

#include "strength/StrengthTypes.hpp"
#include "strength/OpeningBook.hpp"
#include "strength/Statistics.hpp"
#include "board/Position.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

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

protected:
    std::string m_name;
    EngineParameters m_params;
    size_t m_hashMb{16};
    Position m_pos;
    std::string m_lastBestMove{};
};

class PersistentWorkerEngine : public IUciEngine {
public:
    PersistentWorkerEngine(std::string name, const EngineParameters& params, size_t hashMb = 16);
    ~PersistentWorkerEngine() override;

    void sendCommand(std::string_view cmd) override;
    std::string getBestMove(int timeoutMs = 5000) override;
    void setParameters(const EngineParameters& params) override;
    [[nodiscard]] const std::string& getName() const noexcept override { return m_name; }
    [[nodiscard]] const EngineParameters& getParameters() const noexcept override { return m_params; }
    [[nodiscard]] std::string getMetadata() const override;

    [[nodiscard]] std::thread::id getWorkerThreadId() const noexcept { return m_workerThreadId; }
    [[nodiscard]] size_t getSearchCount() const noexcept { return m_searchCount.load(std::memory_order_relaxed); }
    [[nodiscard]] size_t getResetCount() const noexcept { return m_resetCount.load(std::memory_order_relaxed); }

private:
    void workerLoop();
    void postTask(std::function<void()> task);

    std::string m_name;
    EngineParameters m_params;
    size_t m_hashMb{16};
    int m_enforceEvalMode{0};

    Position m_pos;
    std::string m_lastBestMove{};

    std::thread m_workerThread;
    std::thread::id m_workerThreadId{};
    std::atomic<size_t> m_searchCount{0};
    std::atomic<size_t> m_resetCount{0};

    std::mutex m_mutex;
    std::condition_variable m_cvTask;
    std::condition_variable m_cvDone;
    std::condition_variable m_cvReady;
    std::function<void()> m_task;
    bool m_hasTask{false};
    bool m_taskDone{false};
    bool m_ready{false};
    bool m_stop{false};
};

class MatchRunner {
public:
    static MatchRecord runMatch(const MatchConfig& config, IUciEngine* customEngineA = nullptr, IUciEngine* customEngineB = nullptr);
    static GameRecord playGame(uint32_t gameId, IUciEngine& whiteEngine, IUciEngine& blackEngine, const OpeningEntry& opening, const MatchConfig& config);
    [[nodiscard]] static std::string exportPgn(const GameRecord& game);
};

} // namespace Boson

#endif // BOSON_MATCH_RUNNER_HPP
