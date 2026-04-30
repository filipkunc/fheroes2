"""Entry point for the sprite editor.

Run with: python -m tools.sprite_editor
From the fheroes2 project root directory.
"""

import sys
from pathlib import Path

# Ensure the project root is on the path so relative imports work
project_root = Path(__file__).parent.parent.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

from PySide6.QtWidgets import QApplication, QMessageBox
from PySide6.QtCore import Qt

from tools.sprite_editor.main_window import MainWindow, BUILD_DIR
from tools.sprite_editor.tools.icn_extractor import ensure_extractor_built


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("fheroes2 Sprite Editor")
    app.setStyle("Fusion")  # consistent cross-platform look

    # The editor uses the AGG `extractor` binary to pull base ICN sprites and
    # BIN files at runtime. Build it now if missing — palette-remap monsters
    # (Blood Dragon, Avenger, ...) and any newly-created custom monster need it
    # before the user can do anything useful.
    if ensure_extractor_built(BUILD_DIR) is None:
        ret = QMessageBox.warning(
            None, "Extractor not available",
            f"Could not find or build the AGG extractor binary in {BUILD_DIR}.\n\n"
            "The editor will still run, but palette-remap monsters won't render "
            "and base ICN sprites can't be loaded for new custom monsters.\n\n"
            "Continue anyway?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.Yes,
        )
        if ret == QMessageBox.StandardButton.No:
            return

    window = MainWindow()
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
