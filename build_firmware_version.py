# build_firmware_version.py
from datetime import datetime
import os
Import("env")

def increment_build_number(source, target, env):
    header_path = "src/build_number.cpp"
    build_number = 0
    if os.path.exists(header_path):
        with open(header_path, "r") as f:
            for line in f:
                if "buildNumber" in line:
                    try:
                        build_number = int(line.split("=")[1].strip().strip(";"))
                    except:
                        build_number = 0
                    break
    build_number += 1
    now = datetime.now()
    build_date = now.strftime("%Y-%m-%d")
    build_time = now.strftime("%H:%M:%S")
    header = f"""// build_number.cpp

int buildNumber = {build_number};
const char* buildDate = "{build_date}";
const char* buildTime = "{build_time}";
"""
    with open(header_path, "w") as f:
        f.write(header)

# Aici legăm funcția DOAR de comanda 'upload'
env.AddPreAction("upload", increment_build_number)


