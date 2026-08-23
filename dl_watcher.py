import re
import sys
import subprocess
import traceback

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

class NameSegment(FileSegment):
    def __init__(self, lines, name):
        super().__init__(lines)
        self.name = name

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
        stripped = line.strip()
        if stripped == '//#add_here':
            segment_break()
            ret.append(AddHereSegment([line]))
        elif stripped.startswith('//#name '):
            segment_break()
            ret.append(NameSegment([line, next(lines)], stripped[8:]))
        elif stripped.startswith('/*#'):
            segment_break()
            name = stripped[3:]
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

def write_segs(segments, filename):
    # Flatten out segment lists
    result_lines = [l for s in segments for l in s.lines]

    with open(filename, 'w') as f:
        f.writelines(result_lines)

gp_re = re.compile(r'\bgp\("([^"]*)')
numeric_gp_name_re = re.compile(r'(.*\.|)([0-9]+)')
empty_gp_re = re.compile(r'//#gp\b( [^ \n]*)?')
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

    write_segs(segments, last_src_name)

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

    write_segs(segments, last_src_name)

def rmgp(name, last_src_name):
    segments = parse_src(last_src_name)
    for s in segments:
        if isinstance(s, NormalSegment):
            keeping = True
            new_lines = []
            for l in s.lines:
                g = get_gp(l)
                if g is not None:
                    keeping = (g != name)
                if keeping:
                    new_lines.append(l)
            s.lines = new_lines

    write_segs(segments, last_src_name)

def edit_load(filename):
    segments = parse_src("src/" + filename)

    for s in segments:
        if isinstance(s, NameSegment):
            s.lines[1] = s.lines[1].replace(s.name, "lvlUpd")
            break

    write_segs(segments, "src/dl_tmp/tmp.cpp")

def edit_save(filename, last_src_name):
    segments = parse_src(last_src_name)

    for s in segments:
        if isinstance(s, NameSegment):
            s.lines[1] = s.lines[1].replace("lvlUpd", s.name)
            break

    write_segs(segments, "src/" + filename)

def check_src(filename):
    segments = parse_src(filename)
    empty_gps = []
    largest_gps = {}
    for s in segments:
        if isinstance(s, NormalSegment):
            for i in range(len(s.lines)):
                l = s.lines[i]
                if empty_gp_re.search(l):
                    empty_gps.append((s, i))
                gp = get_gp(l)
                if gp is None:
                    continue
                m = numeric_gp_name_re.fullmatch(gp)
                if m is None:
                    continue
                k = m.group(1)
                v = int(m.group(2))
                if k not in largest_gps or largest_gps[k] < v:
                    largest_gps[k] = v

    def replace_func(m):
        pfx = m.group(1)
        if pfx is None:
            pfx = ''
        else:
            # Remove leading space, add '.'
            pfx = pfx[1:] + '.'
        if pfx not in largest_gps:
            largest_gps[pfx] = 0
        largest_gps[pfx] += 1
        name = pfx + str(largest_gps[pfx])
        return f'gp("{name}");'

    modified = False
    # Maybe we'll do other modifications in the future, idk
    for (seg, line) in empty_gps:
        modified = True
        seg.lines[line] = empty_gp_re.sub(replace_func, seg.lines[line])

    if modified:
        print(f"Rewriting {repr(filename)}")
        write_segs(segments, filename)
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
    try:
        if item == "/bake\n":
            bake(lines, last_src_name)
            continue
        if item.startswith("/hotbar"):
            hotbar(item.split(' ', 1)[1].strip(), last_src_name)
            continue
        if item.startswith("/rmgp "):
            rmgp(item[6:].strip(), last_src_name)
            continue
        if item.startswith("/edit_load "):
            edit_load(item.split(' ', 1)[1].strip())
            continue
        if item.startswith("/edit_save "):
            edit_save(item.split(' ', 1)[1].strip(), last_src_name)
            continue
        if item.startswith("/"):
            print(f"Unknown command {repr(item)}")
            continue
    except Exception:
        traceback.print_exc()
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
