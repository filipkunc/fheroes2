"""ComfyUI HTTP client for local image generation.

Uses stdlib (urllib) only — no websocket-client dependency. Progress is
polled via /history/{prompt_id} instead of streamed over /ws. For a typical
~30 s FLUX inference, the 0.5 s poll interval is invisible.

Public entry point: ``ComfyUIClient.transform_sheet(sheet, prompt, ...)``
returns a PIL Image with the same dimensions as the input sheet, matching
the contract the existing GeminiClient exposes.
"""

from __future__ import annotations

import copy
import io
import json
import random
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from pathlib import Path

from PIL import Image


DEFAULT_SERVER = "127.0.0.1:8188"
DEFAULT_WORKFLOW = "flux_kontext_basic.json"
DEFAULT_TIMEOUT_SECONDS = 300

# Canonical filenames for the FLUX.1 Kontext [dev] download. Users who picked
# different variants (e.g. the full bf16 checkpoint or t5xxl_fp16) can override
# these per call.
DEFAULT_UNET_NAME = "flux1-dev-kontext_fp8_scaled.safetensors"
DEFAULT_CLIP_L_NAME = "clip_l.safetensors"
DEFAULT_T5XXL_NAME = "t5xxl_fp8_e4m3fn_scaled.safetensors"
DEFAULT_VAE_NAME = "ae.safetensors"

DEFAULT_STEPS = 20
DEFAULT_GUIDANCE = 2.5


def _server_settings_path() -> Path:
    return Path("~/.config/comfyui/server").expanduser()


def load_server_address() -> str:
    """Load ComfyUI server URL from ~/.config/comfyui/server, defaulting to 127.0.0.1:8188."""
    p = _server_settings_path()
    if p.exists():
        text = p.read_text().strip()
        if text:
            return text
    return DEFAULT_SERVER


def save_server_address(address: str) -> None:
    p = _server_settings_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(address.strip() + "\n")


def _load_workflow_template(name: str = DEFAULT_WORKFLOW) -> dict:
    path = Path(__file__).parent / "workflows" / name
    return json.loads(path.read_text())


def _substitute_workflow(template: dict, mapping: dict[str, object]) -> dict:
    """Walk the workflow dict and replace ${KEY} placeholders.

    Strings of the exact form ``"${KEY}"`` are replaced with the raw value
    (so numbers stay numeric in KSampler.seed / steps / guidance). Embedded
    placeholders inside longer strings are formatted with str().
    """
    def visit(node):
        if isinstance(node, dict):
            return {k: visit(v) for k, v in node.items() if not k.startswith("_")}
        if isinstance(node, list):
            return [visit(x) for x in node]
        if isinstance(node, str):
            stripped = node.strip()
            if stripped.startswith("${") and stripped.endswith("}") and stripped.count("$") == 1:
                key = stripped[2:-1]
                if key not in mapping:
                    raise KeyError(f"Workflow placeholder ${{{key}}} has no binding")
                return mapping[key]
            # Fallback: simple .replace for any embedded placeholder.
            result = node
            for key, value in mapping.items():
                result = result.replace(f"${{{key}}}", str(value))
            return result
        return node

    return visit(template)


class ComfyUIClient:
    """Talks to a running ComfyUI server via HTTP.

    The caller is responsible for starting the server. Use ``ping()`` to
    check reachability before showing the UI as enabled.
    """

    def __init__(self, server: str = DEFAULT_SERVER,
                 timeout_seconds: int = DEFAULT_TIMEOUT_SECONDS):
        # Strip any accidental scheme/path — we always speak http:// to ComfyUI.
        if "://" in server:
            server = server.split("://", 1)[1]
        self.server = server.rstrip("/")
        self.timeout_seconds = timeout_seconds
        self.client_id = uuid.uuid4().hex

    # --- low-level HTTP helpers --------------------------------------------------

    def _url(self, path: str) -> str:
        return f"http://{self.server}{path}"

    def ping(self, timeout_seconds: float = 2.0) -> bool:
        """Return True if the server responds to /system_stats within the timeout."""
        try:
            with urllib.request.urlopen(self._url("/system_stats"), timeout=timeout_seconds) as resp:
                return resp.status == 200
        except (urllib.error.URLError, ConnectionError, TimeoutError):
            return False

    def upload_image(self, image: Image.Image, filename: str | None = None) -> str:
        """POST an image to /upload/image. Returns the server-side filename used in LoadImage."""
        if filename is None:
            filename = f"fheroes2_{uuid.uuid4().hex}.png"

        buf = io.BytesIO()
        image.convert("RGB").save(buf, format="PNG")
        png_bytes = buf.getvalue()

        boundary = f"----fheroes2{uuid.uuid4().hex}"
        body = io.BytesIO()
        def write(s):
            if isinstance(s, str):
                s = s.encode("utf-8")
            body.write(s)
        write(f"--{boundary}\r\n")
        write(f'Content-Disposition: form-data; name="image"; filename="{filename}"\r\n')
        write("Content-Type: image/png\r\n\r\n")
        write(png_bytes)
        write(f"\r\n--{boundary}\r\n")
        write('Content-Disposition: form-data; name="type"\r\n\r\ninput\r\n')
        write(f"--{boundary}\r\n")
        write('Content-Disposition: form-data; name="overwrite"\r\n\r\ntrue\r\n')
        write(f"--{boundary}--\r\n")

        req = urllib.request.Request(
            self._url("/upload/image"),
            data=body.getvalue(),
            headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
        )
        with urllib.request.urlopen(req, timeout=30) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
        # Server returns {"name": "...", "subfolder": "...", "type": "input"}
        return payload.get("name", filename)

    def queue_prompt(self, workflow: dict) -> str:
        """POST a workflow to /prompt. Returns prompt_id."""
        body = json.dumps({"prompt": workflow, "client_id": self.client_id}).encode("utf-8")
        req = urllib.request.Request(
            self._url("/prompt"),
            data=body,
            headers={"Content-Type": "application/json"},
        )
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                data = json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            err_body = e.read().decode("utf-8", errors="replace")
            raise RuntimeError(
                f"ComfyUI rejected the workflow ({e.code}): {err_body[:500]}"
            ) from e
        return data["prompt_id"]

    def fetch_history(self, prompt_id: str) -> dict | None:
        """GET /history/{prompt_id}. Returns the history entry, or None until it appears."""
        with urllib.request.urlopen(
            self._url(f"/history/{prompt_id}"), timeout=10,
        ) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        return data.get(prompt_id)

    def fetch_view(self, filename: str, subfolder: str, folder_type: str) -> bytes:
        query = urllib.parse.urlencode({
            "filename": filename, "subfolder": subfolder, "type": folder_type,
        })
        with urllib.request.urlopen(self._url(f"/view?{query}"), timeout=30) as resp:
            return resp.read()

    def interrupt(self) -> None:
        """POST /interrupt to cancel the currently running prompt."""
        req = urllib.request.Request(self._url("/interrupt"), data=b"", method="POST")
        try:
            urllib.request.urlopen(req, timeout=5)
        except urllib.error.URLError:
            pass

    # --- high-level entry --------------------------------------------------------

    def transform_sheet(
        self,
        sheet: Image.Image,
        prompt: str,
        *,
        workflow_name: str = DEFAULT_WORKFLOW,
        seed: int | None = None,
        steps: int = DEFAULT_STEPS,
        guidance: float = DEFAULT_GUIDANCE,
        unet_name: str = DEFAULT_UNET_NAME,
        clip_l_name: str = DEFAULT_CLIP_L_NAME,
        t5xxl_name: str = DEFAULT_T5XXL_NAME,
        vae_name: str = DEFAULT_VAE_NAME,
        on_progress=None,
        should_cancel=None,
    ) -> Image.Image | None:
        """Run a FLUX Kontext edit on ``sheet`` with the given prompt.

        ``on_progress(message: str)`` is called with status updates for the
        UI status label. ``should_cancel()`` is polled between waits and, if
        true, sends /interrupt and returns None.

        Returns a PIL Image resized to the input dimensions so the existing
        slice_sheet logic works unchanged.
        """
        if seed is None:
            seed = random.randint(0, 2**31 - 1)

        if on_progress:
            on_progress("Uploading input to ComfyUI...")
        server_filename = self.upload_image(sheet)

        template = _load_workflow_template(workflow_name)
        workflow = _substitute_workflow(template, {
            "INPUT_IMAGE": server_filename,
            "PROMPT": prompt,
            "SEED": int(seed),
            "STEPS": int(steps),
            "GUIDANCE": float(guidance),
            "UNET_NAME": unet_name,
            "CLIP_L_NAME": clip_l_name,
            "T5XXL_NAME": t5xxl_name,
            "VAE_NAME": vae_name,
        })

        if on_progress:
            on_progress(f"Queueing workflow (seed={seed}, steps={steps})...")
        prompt_id = self.queue_prompt(workflow)

        if on_progress:
            on_progress("Generating (this can take ~30s on a 4090, longer on slower GPUs)...")

        deadline = time.time() + self.timeout_seconds
        poll_interval = 0.5
        while True:
            if should_cancel is not None and should_cancel():
                self.interrupt()
                return None
            if time.time() > deadline:
                self.interrupt()
                raise RuntimeError(
                    f"ComfyUI did not finish within {self.timeout_seconds}s "
                    f"(prompt_id={prompt_id})."
                )

            entry = self.fetch_history(prompt_id)
            if entry is not None:
                status = entry.get("status", {})
                if status.get("status_str") == "error":
                    msgs = []
                    for m in status.get("messages", []):
                        if isinstance(m, list) and len(m) >= 2:
                            msgs.append(repr(m[1]))
                    raise RuntimeError(
                        f"ComfyUI workflow errored: {'; '.join(msgs) or 'unknown error'}"
                    )
                # Look for the first SaveImage output.
                outputs = entry.get("outputs", {})
                for node_id, node_out in outputs.items():
                    for image_meta in node_out.get("images", []):
                        png_bytes = self.fetch_view(
                            image_meta["filename"],
                            image_meta.get("subfolder", ""),
                            image_meta.get("type", "output"),
                        )
                        result = Image.open(io.BytesIO(png_bytes)).convert("RGB")
                        if result.size != sheet.size:
                            result = result.resize(sheet.size, Image.Resampling.BOX)
                        return result
                # History entry exists but no images yet — keep polling.

            time.sleep(poll_interval)
