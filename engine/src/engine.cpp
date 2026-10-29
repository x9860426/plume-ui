#include "plume/engine.h"

namespace plume {

Engine::Engine() = default;
Engine::~Engine() { shutdown(); }

bool Engine::initialize(const EngineConfig& config) {
    if (running_) {
        return true;
    }
    if (config.width == 0 || config.height == 0) {
        return false;
    }
    config_ = config;
    running_ = true;
    return true;
}

void Engine::shutdown() {
    running_ = false;
}

bool Engine::is_running() const noexcept {
    return running_;
}

const EngineConfig& Engine::config() const noexcept {
    return config_;
}

const char* Engine::version() noexcept {
    return "0.1.0";
}

}  // namespace plume
