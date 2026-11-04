"""Format C++ with clang-format, then preserve the project's single-blank-line spacing."""

import argparse
import pathlib
import re
import shutil
import subprocess

def separate_blocks(text):
    lines = text.splitlines()
    result = []
    pending_blank = False

    for line in lines:
        line = line.rstrip()

        if not line:
            pending_blank = True
            continue

        stripped = line.lstrip()
        # Keep continuation indentation in four-space steps as well.
        indent = len(line) - len(stripped)
        line = " " * ((indent + 3) // 4 * 4) + stripped

        if result:
            previous = result[-1].strip()
            closes_block = re.fullmatch(r"}\s*(?:;|\);)?\s*(?://.*)?", previous)
            continues_block = stripped.startswith(("}", "else", "catch", "#else", "#endif"))
            starts_block = re.match(r"(?:if|for|while|switch|try)\s*[({]", stripped)
            access_section = stripped in ("public:", "private:", "protected:", "signals:")
            definition = re.match(r"(?:namespace|class|struct)\b", stripped)
            ui_group = re.match(r"auto (?:root|header|viewportPanel|tools|side|input|xyzPanel|jointPanel|acquire|surface|exports|scroll|graphicsFailure)\s*=", stripped)
            opening = previous.endswith("{")
            gap = pending_blank or (closes_block and not continues_block)
            gap = gap or ((starts_block or access_section) and not opening and not continues_block)
            gap = gap or ((definition or ui_group) and not opening)

            if gap:
                result.append("")

        result.append(line)
        pending_blank = False

    return "\n".join(result) + "\n"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", default=shutil.which("clang-format"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    if not args.clang_format:
        parser.error("Supply --clang-format with the clang-format executable path")

    root = pathlib.Path(__file__).resolve().parent.parent
    paths = sorted(path for folder in ("src", "tests") for path in (root / folder).iterdir()
        if path.suffix in (".h", ".cpp"))
    changed = []

    for path in paths:
        original = path.read_text(encoding="utf-8")
        formatted = subprocess.run([args.clang_format, str(path)], check=True,
            capture_output=True, encoding="utf-8").stdout
        formatted = separate_blocks(formatted)

        if original != formatted:
            changed.append(str(path.relative_to(root)))

            if not args.check:
                path.write_text(formatted, encoding="utf-8", newline="\n")

    print(f"Checked {len(paths)} C++ files; {len(changed)} {'need formatting' if args.check else 'formatted'}.")

    if args.check and changed:
        print("\n".join(changed))
        return 1

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
