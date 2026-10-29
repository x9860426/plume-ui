"""Plume UI Python API.

The package prefers the native C++ binding. When the extension is not built,
it falls back to a lightweight pure-Python engine so scripts can still run.
"""

from __future__ import annotations

from dataclasses import dataclass

__version__ = "0.1.0"

try:
    from ._plume import EngineConfig, NativeEngine as Engine
    NATIVE_AVAILABLE = True
except ImportError:
    NATIVE_AVAILABLE = False

    @dataclass
    class EngineConfig:
        application_name: str = "Plume UI Python Application"
        width: int = 1280
        height: int = 720
        enable_vsync: bool = True

    class Engine:
        """Lightweight Python fallback; not a full C++ engine replacement."""

        def __init__(self) -> None:
            self._config = EngineConfig()
            self._running = False

        def initialize(self, config: EngineConfig | None = None) -> bool:
            config = config or EngineConfig()
            if config.width <= 0 or config.height <= 0:
                return False
            self._config = config
            self._running = True
            return True

        def shutdown(self) -> None:
            self._running = False

        def is_running(self) -> bool:
            return self._running

        def config(self) -> EngineConfig:
            return self._config

        @staticmethod
        def version() -> str:
            return __version__

__all__ = ["Engine", "EngineConfig", "NATIVE_AVAILABLE", "__version__"]
