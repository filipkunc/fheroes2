I've compiled all the technical details, the Python implementation, and the workflow logic into a clean Markdown structure. You can copy this directly into a `README.md` or a project documentation file.

---

# Pixel Art Transformation Workflow

This document outlines the automated workflow for transforming legacy game sprites using the **Gemini API**.

## 🛠️ Prerequisites
* **API Key:** Obtain from [Google AI Studio](https://aistudio.google.com/).
* **Environment:** Python 3.9+
* **Library:** `google-genai`

```bash
pip install -U google-genai pillow
```

---

## 🐍 Automation Script (Batch Processor)
This script allows you to point to a folder of original sprites and apply a transformation prompt to all of them at once.

```python
import os
from google import genai
from google.genai import types
from PIL import Image

# Configuration
API_KEY = "YOUR_API_KEY"
INPUT_DIR = "./sprites/original"
OUTPUT_DIR = "./sprites/transformed"
PROMPT = (
    "Transform this pixel art sprite. Change the base color from green to azure blue. "
    "Replace fire/flame effects with yellow lightning bolts. "
    "Maintain the 16-bit aesthetic and original silhouette."
)

client = genai.Client(api_key=API_KEY)

if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

def process_sprites():
    for filename in os.listdir(INPUT_DIR):
        if filename.endswith((".png", ".jpg", ".webp")):
            print(f"Processing {filename}...")
            
            img_path = os.path.join(INPUT_DIR, filename)
            raw_image = Image.open(img_path)

            response = client.models.generate_content(
                model="gemini-3.1-flash-image-preview",
                contents=[PROMPT, raw_image],
                config=types.GenerateContentConfig(
                    response_modalities=["IMAGE"]
                )
            )

            # Save the result
            for part in response.candidates[0].content.parts:
                if part.inline_data:
                    out_path = os.path.join(OUTPUT_DIR, f"transformed_{filename}")
                    with open(out_path, "wb") as f:
                        f.write(part.inline_data.data)
            print(f"Saved to {out_path}")

if __name__ == "__main__":
    process_sprites()
```

---

## 📐 Technical Specifications

| Parameter | Value | Description |
| :--- | :--- | :--- |
| **Model** | `gemini-3.1-flash-image-preview` | Optimized for speed and image-to-image tasks. |
| **Input Modality** | Multi-modal (Text + Image) | You can provide up to 14 reference images. |
| **Output Type** | `IMAGE` | Returns raw bytes of the generated PNG. |
| **Style Retention** | High | The model uses the input image as a structural "mask." |

---

## 🚀 Optimization Tips

* **Seed Control:** If you want a specific "type" of lightning across all sprites, include a secondary reference image of just a lightning bolt to serve as a style guide.
* **Resolution:** For pixel art, keep the input images at their native resolution. The AI handles the "upscaling" logic internally but maintains the pixelated edges better if the source isn't blurry.
* **Prompting:** Be specific about what **not** to change. (e.g., *"Do not change the background color or the pose of the character."*)

---

**Would you like me to add a section on how to handle "sprite sheet" splitting if your assets are all in one large image?**