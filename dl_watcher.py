import sys
import subprocess

for item in sys.stdin:
    print(item, end='') # Newline will be part of `item`
    item = item.split(' ', 2)[2].strip()
    print(f"=> {item}")
    if not (item.endswith(".c") or item.endswith(".cpp")):
        continue
    print("running...")
    subprocess.run(["./dl_build.sh", item])
