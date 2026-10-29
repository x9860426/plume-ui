#pragma once

#include <cstdint>
#include <string>

namespace plume {

struct EngineConfig {
    std::string application_name = "Plume UI Application";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    bool enable_vsync = true;
};

class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool initialize(const EngineConfig& config = {});
    void shutdown();
    bool is_running() const noexcept;

    const EngineConfig& config() const noexcept;
    static const char* version() noexcept;

private:
    EngineConfig config_;
    bool running_ = false;
};

}  // namespace plume
