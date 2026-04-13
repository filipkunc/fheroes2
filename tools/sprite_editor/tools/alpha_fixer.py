"""Background removal and alpha channel fixing tools.

Provides flood fill from corners, threshold-based removal, and edge refinement.
"""

import numpy as np
from PIL import Image
from scipy import ndimage


def flood_fill_remove_bg(image: Image.Image, tolerance: int = 10) -> Image.Image:
    """Remove background using flood fill from image corners.

    This is the most reliable approach for sprites with dark pixels near
    the background color. Only removes pixels connected to the border.
    """
    img = image.convert("RGB")
    pixels = np.array(img, dtype=np.int16)

    # Use corner pixel as background color reference
    bg_color = pixels[0, 0].astype(np.int16)

    # Find all pixels close to background color
    diff = np.abs(pixels - bg_color)
    is_bg_candidate = np.all(diff <= tolerance, axis=2)

    # Label connected regions
    labeled, num_features = ndimage.label(is_bg_candidate)

    # Find which labels touch the border
    border_labels = set()
    h, w = labeled.shape
    border_labels.update(labeled[0, :].tolist())      # top edge
    border_labels.update(labeled[h - 1, :].tolist())   # bottom edge
    border_labels.update(labeled[:, 0].tolist())        # left edge
    border_labels.update(labeled[:, w - 1].tolist())    # right edge
    border_labels.discard(0)  # 0 = non-candidate pixels

    # Create alpha mask: transparent only for border-connected bg regions
    is_bg = np.isin(labeled, list(border_labels))
    alpha = np.where(is_bg, 0, 255).astype(np.uint8)

    # Compose RGBA result
    result = image.convert("RGBA")
    result_pixels = np.array(result)
    result_pixels[:, :, 3] = alpha
    return Image.fromarray(result_pixels)


def threshold_remove_bg(image: Image.Image, tolerance: int = 10) -> Image.Image:
    """Simple threshold-based background removal.

    Removes any pixel within tolerance of the corner pixel color.
    Faster but less accurate than flood fill — may remove interior pixels.
    """
    img = image.convert("RGB")
    pixels = np.array(img, dtype=np.int16)

    bg_color = pixels[0, 0].astype(np.int16)
    diff = np.abs(pixels - bg_color)
    is_bg = np.all(diff <= tolerance, axis=2)

    alpha = np.where(is_bg, 0, 255).astype(np.uint8)

    result = image.convert("RGBA")
    result_pixels = np.array(result)
    result_pixels[:, :, 3] = alpha
    return Image.fromarray(result_pixels)


def feather_edges(image: Image.Image, radius: int = 1) -> Image.Image:
    """Apply edge feathering to alpha channel using distance transform."""
    pixels = np.array(image.convert("RGBA"))
    alpha = pixels[:, :, 3]

    if radius <= 0:
        return image

    # Distance from transparent pixels
    dist = ndimage.distance_transform_edt(alpha > 0)

    # Feather: pixels within radius of the edge get partial alpha
    feathered = np.clip(dist / radius * 255, 0, 255).astype(np.uint8)
    feathered = np.minimum(feathered, alpha)

    pixels[:, :, 3] = feathered
    return Image.fromarray(pixels)
