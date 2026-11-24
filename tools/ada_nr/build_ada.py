"""Build an isolated Ada NR model; never overwrite the user's source DLL."""
import argparse
import hashlib
import json
from pathlib import Path
import patcher

EXPECTED_SHA256 = "2bc19bc09a7b89dd44f73e2ac0108fcc586e2154bdd0830f242f4f040e28789f"


def fp8_lines(text):
    return [line.strip() for line in text.splitlines()
            if "e4m3" in line and not line.lstrip().startswith("//")]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cuda-root", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error("Output must differ from the original model")
    if hashlib.sha256(args.input.read_bytes()).hexdigest() != EXPECTED_SHA256:
        parser.error("Only the verified original 310.9.16 model SHA-256 is supported")

    def tool(name):
        matches = list(args.cuda_root.rglob(name + ".exe"))
        if len(matches) != 1:
            parser.error(f"Expected one {name}.exe below --cuda-root")
        return matches[0].resolve()

    tools = patcher.CudaTools(cuobjdump=tool("cuobjdump"), ptxas=tool("ptxas"),
                              fatbinary=tool("fatbinary"))
    transform = patcher.transform_ptx
    audit = []
    commands = []
    original_run = patcher.run_tool

    def checked_transform(source, target=patcher.ADA, **kwargs):
        if target != patcher.ADA:
            raise patcher.PatchError("This wrapper only builds native-FP8 Ada kernels")
        output, stats = transform(source, target, **kwargs)
        if fp8_lines(source) != fp8_lines(output):
            raise patcher.PatchError("Native FP8 instructions were changed")
        audit.append({"native_fp8_lines_preserved": len(fp8_lines(output)),
                      "transforms": stats.values})
        return output, stats

    def logged_run(command, **kwargs):
        commands.append([str(part) for part in command])
        return original_run(command, **kwargs)

    patcher.transform_ptx = checked_transform
    patcher.run_tool = logged_run
    patcher.patch_file(args.input, args.output, tools, force=False, dry_run=False,
                       work_dir=args.work_dir, architectures=(patcher.ADA,))
    args.output.with_suffix(".build.json").write_text(json.dumps({
        "input_sha256": EXPECTED_SHA256,
        "output_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
        "target": "sm_89", "fp8_lowering": False,
        "modules": audit, "commands": commands,
    }, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
