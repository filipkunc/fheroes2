"""API call logging and cost estimation for Gemini sprite generation."""

import json
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path


COST_LOG_PATH = Path.home() / ".config" / "sprite_editor" / "cost_log.json"

# Rough cost estimates per API call (image generation)
MODEL_COSTS = {
    "gemini-3-pro-image-preview": 0.005,
    "gemini-2.0-flash-preview-image-generation": 0.001,
}


@dataclass
class ApiCall:
    timestamp: float
    model: str
    frame_indices: list[int]
    input_size: tuple[int, int]  # sheet w, h
    output_size: tuple[int, int] | None = None
    estimated_cost: float = 0.0
    success: bool = True


@dataclass
class CostTracker:
    calls: list[ApiCall] = field(default_factory=list)

    @property
    def total_cost(self) -> float:
        return sum(c.estimated_cost for c in self.calls)

    @property
    def total_calls(self) -> int:
        return len(self.calls)

    @property
    def successful_calls(self) -> int:
        return sum(1 for c in self.calls if c.success)

    def log_call(self, model: str, frame_indices: list[int],
                 input_size: tuple[int, int], output_size: tuple[int, int] | None = None,
                 success: bool = True):
        cost = MODEL_COSTS.get(model, 0.003)
        call = ApiCall(
            timestamp=time.time(),
            model=model,
            frame_indices=frame_indices,
            input_size=input_size,
            output_size=output_size,
            estimated_cost=cost,
            success=success,
        )
        self.calls.append(call)
        self._save()
        return call

    def _save(self):
        COST_LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        data = [asdict(c) for c in self.calls]
        COST_LOG_PATH.write_text(json.dumps(data, indent=2))

    @classmethod
    def load(cls) -> "CostTracker":
        tracker = cls()
        if COST_LOG_PATH.exists():
            try:
                data = json.loads(COST_LOG_PATH.read_text())
                for entry in data:
                    tracker.calls.append(ApiCall(**entry))
            except (json.JSONDecodeError, TypeError):
                pass
        return tracker

    def summary(self) -> str:
        return f"{self.total_calls} calls, {self.successful_calls} OK, ~${self.total_cost:.3f}"
