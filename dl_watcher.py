import re
import sys
import subprocess

def stdin_to_generator():
    for item in sys.stdin:
        yield item

class FileSegment:
    def __init__(self, lines):
        self.lines = lines
        if type(self) is FileSegment:
            raise Exception('FileSegment is abstract')

class NormalSegment(FileSegment):
    pass

class AddHereSegment(FileSegment):
    pass

class HotbarSegment(FileSegment):
    def __init__(self, lines, name):
        super().__init__(lines)
        self.contents = lines[1:-1]
        self.name = name

def parse_src(filename):
    with open(filename, "r") as f:
        lines = (l for l in f.readlines())

    normal_lines = []
    ret = []
    def segment_break():
        nonlocal normal_lines
        if len(normal_lines) == 0:
            return
        ret.append(NormalSegment(normal_lines))
        normal_lines = []

    while True:
        try:
            line = next(lines)
        except StopIteration:
            break
        if line.strip() == '//#add_here':
            segment_break()
            ret.append(AddHereSegment([line]))
        elif line.strip().startswith('/*#'):
            segment_break()
            name = line.strip()[3:]
            hotbar_lines = []
            while True:
                hotbar_lines.append(line)
                if line.strip() == '*/':
                    break
                line = next(lines)
            ret.append(HotbarSegment(hotbar_lines, name))
        else:
            normal_lines.append(line)
    segment_break()
    return ret

def bake(lines, last_src_name):
    replacements = {}
    while True:
        item = next(lines)
        if item == "\n":
            break
        parts = item.split(' ', 1)
        value = int(parts[0])
        name = parts[1][:-1] # Remove trailing newline
        replacements[name] = str(value)

    # This matches calls to the `var` function, assuming you don't nest `()` more than one layer deep inside.
    # If you need more, I'll have to start basically parsing C source for real lol
    var_regex = r'\bvar\("([^"]*)"([^()]|\([^()]*\))*\)'
    def replace_func(m):
        # Try to look up key, using whole match as a fallback.
        return replacements.get(m.group(1), m.group(0))

    segments = parse_src(last_src_name)
    for s in segments:
        if isinstance(s, NormalSegment):
            s.lines = [re.sub(var_regex, replace_func, l) for l in s.lines]

    # Flatten out segment lists
    result_lines = [l for s in segments for l in s.lines]

    with open(last_src_name, 'w') as f:
        f.writelines(result_lines)

def hotbar(name, last_src_name):
    segments = parse_src(last_src_name)

    for s in segments:
        if isinstance(s, HotbarSegment) and s.name == name:
            contents = s.contents
            break
    else:
        print(f"hotbar: didn't find a segment named {repr(name)}")
        return

    for s in segments:
        if isinstance(s, AddHereSegment):
            s.lines = contents + s.lines
            break
    else:
        print("hotbar: Didn't find the //#add_here marker")
        return

    # Flatten out segment lists
    result_lines = [l for s in segments for l in s.lines]

    with open(last_src_name, 'w') as f:
        f.writelines(result_lines)

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
    if item.startswith("/hotbar"):
        hotbar(item.split(' ', 1)[1].strip(), last_src_name)
        continue
    if item.startswith("/"):
        print(f"Unknown command {repr(item)}")
        continue
    # Else it's probably from inotifywait, telling us a file changed
    item = item.split(' ', 2)[2].strip()
    item = "src/dl_tmp/" + item
    if not (item.endswith(".c") or item.endswith(".cpp")):
        print(f"Ignored {repr(item)}")
        continue
    last_src_name = item
    print(f"Building {repr(item)}")
    subprocess.run(["./dl_build.sh", item])
