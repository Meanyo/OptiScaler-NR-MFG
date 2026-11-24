"""Audit preserved FP8 PTX and native Ada QMMA instructions in every generated module."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--cuobjdump", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = []
    for directory in sorted(args.work_dir.glob("fatbin_*")):
        source = next(path for path in directory.glob("*.ptx")
                      if path.name != "ada.ptx" and not path.name.startswith("source_"))
        original = source.read_text(encoding="utf-8")
        generated = (directory / "ada.ptx").read_text(encoding="utf-8")

        def fp8_lines(text):
            return [line.strip() for line in text.splitlines()
                    if "e4m3" in line and not line.lstrip().startswith("//")]

        if fp8_lines(original) != fp8_lines(generated):
            raise RuntimeError(f"FP8 PTX changed: {directory}")
        sass = subprocess.check_output([
            str(args.cuobjdump), "--dump-sass", str(directory / "ada.sm_89.cubin")
        ], text=True, encoding="utf-8", errors="replace")
        if "code for sm_89" not in sass:
            raise RuntimeError(f"Wrong GPU target: {directory}")
        rows.append({
            "module": directory.name,
            "fp8_ptx_lines_preserved": len(fp8_lines(generated)),
            "native_fp8_qmma": len(re.findall(r"\bQMMA\.[^\s;]*E4M3", sass)),
            "native_fp8_conversions": len(re.findall(r"\bF2FP\.[^\s;]*E4M3", sass)),
        })
    if len(rows) != 15 or not sum(row["native_fp8_qmma"] for row in rows):
        raise RuntimeError("Expected 15 modules with native Ada FP8 matrix operations")
    manifest = {
        "output_sha256": hashlib.sha256(args.model.read_bytes()).hexdigest().upper(),
        "target": "sm_89", "fp8_lowering": False, "modules": rows,
    }
    args.output.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps({"modules": len(rows),
                      "fp8_ptx_lines_preserved": sum(row["fp8_ptx_lines_preserved"] for row in rows),
                      "native_fp8_qmma": sum(row["native_fp8_qmma"] for row in rows),
                      "sha256": manifest["output_sha256"]}))


if __name__ == "__main__":
    main()
