import re
import sys
import subprocess

def stdin_to_generator():
    for item in sys.stdin:
        yield item

def bake(lines, last_src_name):
    replacements = {}
    while True:
        item = next(lines)
        if item == "\n":
            break
        parts = item.split(' ', 1)
        value = int(parts[0])
        name = parts[1][:-1] # Remove trailing newline
        replacements[name] = value
    with open(last_src_name, 'r') as f:
        src_lines = f.readlines()
    var_regex = r'\bvar\("([^"]*)"(,[^)]*)?\)'
    def replace_func(m):
        # Try to look up key, using whole match as a fallback.
        # It's legal to `str`-ify either one.
        return str(replacements.get(m.group(1), m.group(0)))
    src2_lines = [re.sub(var_regex, replace_func, l) for l in src_lines]
    with open(last_src_name, 'w') as f:
        f.writelines(src2_lines)

lines = stdin_to_generator()

last_src_name = None
while True:
    try:
        item = next(lines)
    except StopIteration:
        break
    if item == "/bake\n":
        bake(lines, last_src_name)
        continue
    item = item.split(' ', 2)[2].strip()
    item = "src/dl_tmp/" + item
    if not (item.endswith(".c") or item.endswith(".cpp")):
        print(f"Ignored {repr(item)}")
        continue
    last_src_name = item
    print(f"Building {repr(item)}")
    subprocess.run(["./dl_build.sh", item])
