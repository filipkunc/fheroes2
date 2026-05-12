"""Qt worker thread + dropdown registry for ComfyUI generation.

Lives in its own module so both the monster sheet panel and the hero portrait
panel can share one QThread implementation and one COMFYUI_MODELS dict.
"""

from __future__ import annotations

from PIL import Image
from PySide6.QtCore import QThread, Signal

from .comfyui_client import ComfyUIClient


# Dropdown entries that route through ComfyUI instead of Gemini. Add an entry
# here per new local-model workflow (key = label shown in the UI dropdown,
# value = filename under comfyui/workflows/).
COMFYUI_MODELS: dict[str, str] = {
    "FLUX.1 Kontext [dev] (local)": "flux_kontext_basic.json",
}


def is_comfyui_model(name: str) -> bool:
    return name in COMFYUI_MODELS


class ComfyUIWorker(QThread):
    """Background thread for a single ComfyUI workflow run.

    Same finished/error contract as the Gemini workers — so panels can wire
    both providers through one set of handlers.
    """

    finished = Signal(object)  # PIL Image or None
    error = Signal(str)
    progress = Signal(str)

    def __init__(self, client: ComfyUIClient, image: Image.Image, prompt: str,
                 workflow_name: str, *, guidance: float | None = None,
                 steps: int | None = None):
        super().__init__()
        self._client = client
        self._image = image
        self._prompt = prompt
        self._workflow_name = workflow_name
        self._guidance = guidance
        self._steps = steps
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    def run(self):
        try:
            kwargs = {}
            if self._guidance is not None:
                kwargs["guidance"] = self._guidance
            if self._steps is not None:
                kwargs["steps"] = self._steps
            result = self._client.transform_sheet(
                self._image, self._prompt,
                workflow_name=self._workflow_name,
                on_progress=lambda msg: self.progress.emit(msg),
                should_cancel=lambda: self._cancelled,
                **kwargs,
            )
            self.finished.emit(result)
        except Exception as e:
            self.error.emit(f"{type(e).__name__}: {e}")
