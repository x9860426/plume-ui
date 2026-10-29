from plume import Engine, EngineConfig, NATIVE_AVAILABLE

engine = Engine()
config = EngineConfig(application_name="Quickstart", width=960, height=540)

if not engine.initialize(config):
    raise RuntimeError("Failed to initialize Plume UI")

print("Plume UI", engine.version())
print("Native binding:", NATIVE_AVAILABLE)
print("Running:", engine.is_running())
engine.shutdown()
