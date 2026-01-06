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

gp_re = re.compile(r'\bgp\("([^"]*)')
empty_gp_re = re.compile(r'\bgp\(\)')
# This matches calls to the `var`/`pvar`/`rvar` functions,
# assuming you don't nest `()` more than one layer deep inside.
# If you need more, I'll have to start basically parsing C source for real lol
var_re = re.compile(r'\b([pr]?var\("([^"]*)")([^()]|\([^()]*\))*\)')

def get_gp(line):
    m = gp_re.search(line)
    if m is None:
        return None
    return m.group(1)

def bake(lines, last_src_name):
    group_replacements = {}
    while (item := next(lines)) != "\n":
        parts = item.split(' ', 1)
        if parts[0] != 'gp':
            raise Exception(f"parts[0] should be \"gp\", not {repr(parts[0])}")
        replacements = {}
        group_replacements[parts[1].strip()] = replacements
        while (item := next(lines)) != "\n":
            parts = item.split(' ', 1)
            name = parts[0]
            value = parts[1].strip()
            replacements[name] = value
    replacements = group_replacements.get("", {})

    def replace_func(m):
        key = m.group(2)
        if key not in replacements:
            return m.group(0) # No change
        return f"{m.group(1)}, {replacements[key]})";

    segments = parse_src(last_src_name)
    for s in segments:
        if isinstance(s, NormalSegment):
            for i in range(len(s.lines)):
                l = s.lines[i]
                g = get_gp(l)
                if g is not None:
                    replacements = group_replacements.get(g, {})
                s.lines[i] = var_re.sub(replace_func, l)

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

def check_src(filename):
    segments = parse_src(filename)
    empty_gps = []
    largest_gp = -1
    for s in segments:
        if isinstance(s, NormalSegment):
            for i in range(len(s.lines)):
                l = s.lines[i]
                if empty_gp_re.search(l):
                    empty_gps.append((s, i))
                gp = get_gp(l)
                if gp is not None:
                    try:
                        numeric = int(gp)
                        if numeric > largest_gp:
                            largest_gp = numeric
                    except ValueError:
                        # Other (non-numeric) group names are totally fine
                        pass

    modified = False
    # Maybe we'll do other modifications in the future, idk
    for (seg, line) in empty_gps:
        modified = True
        largest_gp += 1
        seg.lines[line] = empty_gp_re.sub(f'gp("{largest_gp}")', seg.lines[line])

    if modified:
        print(f"Rewriting {repr(filename)}")
        # Flatten out segment lists
        result_lines = [l for s in segments for l in s.lines]
        with open(filename, 'w') as f:
            f.writelines(result_lines)
        # inotifywait will let us know that something was written, so compiling now would just duplicate.
    else:
        print(f"Building {repr(filename)}")
        subprocess.run(["./dl_build.sh", filename])

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
    orig_item = item
    item = item.split(' ', 2)[2].strip()
    item = "src/dl_tmp/" + item
    if not (item.endswith(".c") or item.endswith(".cpp")):
        print(f"Ignored {repr(item)}")
        # IDK why it's reporting a number as the filename,
        # maybe it's the directory's write time being updated?
        #if not '.' in item:
        #    print(f"(debug: {repr(orig_item)})")
        continue
    last_src_name = item
    check_src(last_src_name)
