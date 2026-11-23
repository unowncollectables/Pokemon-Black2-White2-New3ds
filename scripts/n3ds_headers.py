#!/usr/bin/env python3
"""Write the constant and text headers the C includes. No ROM.

The lists in generated/ and the message files in res/text/ are already in the
tree. configure.py would do this as part of a ROM build; this is the same
generators, stopped before anything reads orig/ or extract/.
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.data.gen_constants import generate  # noqa: E402
from tools.text.pms_words import header as pms_header  # noqa: E402
from tools.text.text_ids import header as text_header  # noqa: E402


def main() -> None:
    constants = ROOT / "build" / "include" / "generated" / "constants"
    constants.mkdir(parents=True, exist_ok=True)
    count = 0
    for source in sorted((ROOT / "generated").glob("*.txt")):
        (constants / f"{source.stem}.h").write_text(generate(source))
        count += 1
    (constants / "pms_words.h").write_text(pms_header())
    text_count = 0
    for folder in ("system", "script"):
        out = ROOT / "build" / "include" / "generated" / "text" / folder
        out.mkdir(parents=True, exist_ok=True)
        for source in sorted((ROOT / "res" / "text" / folder).glob("*.txt")):
            (out / f"{source.stem}.h").write_text(text_header(source))
            text_count += 1
    print(f"headers: {count} constant lists, pms_words, {text_count} text banks (no ROM)")


if __name__ == "__main__":
    main()
