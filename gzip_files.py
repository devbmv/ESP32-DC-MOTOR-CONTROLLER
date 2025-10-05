import os
import gzip
from pathlib import Path
from SCons.Script import Import

Import("env")

def gzip_file(src_path, dest_path):
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    with open(src_path, "rb") as f_in:
        with gzip.open(dest_path, "wb") as f_out:
            f_out.writelines(f_in)
    print(f"Comprimat: {src_path} → {dest_path}")

def main():
    print("📦 Comprimare fișiere din /templates → /data")

    templates_dir = Path("templates")
    data_dir = Path("data")

    if not templates_dir.exists():
        print("❌ Folderul 'templates/' nu există.")
        return

    extensions = [".html", ".css", ".js"]
    for file in templates_dir.glob("**/*"):
        if file.suffix in extensions and not file.name.endswith(".gz") and file.is_file():
            relative_path = file.relative_to(templates_dir)
            dest_file = data_dir / (relative_path.as_posix() + ".gz")
            gzip_file(file, dest_file)

    print("🏁 Comprimare terminată.")

def before_build(source, target, env):
    main()

before_build(None, None, None)
