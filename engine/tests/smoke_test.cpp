#include "plume/engine.h"

#include <cassert>
#include <cstring>

int main() {
    plume::Engine engine;
    assert(!engine.is_running());

    plume::EngineConfig config;
    config.application_name = "Smoke Test";
    config.width = 800;
    config.height = 600;

    assert(engine.initialize(config));
    assert(engine.is_running());
    assert(engine.config().width == 800);
    assert(std::strcmp(plume::Engine::version(), "0.1.0") == 0);

    engine.shutdown();
    assert(!engine.is_running());
    return 0;
}
