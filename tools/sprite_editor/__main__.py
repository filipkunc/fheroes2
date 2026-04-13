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

from PySide6.QtWidgets import QApplication
from PySide6.QtCore import Qt

from tools.sprite_editor.main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("fheroes2 Sprite Editor")
    app.setStyle("Fusion")  # consistent cross-platform look

    window = MainWindow()
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
