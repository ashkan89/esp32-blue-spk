"""Make pinned libhelix allocation failures recoverable in project-local deps.

PlatformIO installs these files below .pio/libdeps, never in the global SDK.
Every replacement is checked: a dependency upgrade requires reviewing this fix.
The middleware runs after dependency installation, before any compilation.
"""
from pathlib import Path

MARKER = '// speaker: recoverable allocation v1\n'

def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Pinned libhelix changed: review allocation guard: ' + old[:70])
    return text.replace(old, new, 1)

def guarded(name, text):
    if text.startswith(MARKER):
        return text
    if name == 'Allocator.h':
        text = replace_once(text, '      while(1);', '      return nullptr;')
        text = text.replace('      while(true);', '      return nullptr;')
        text = replace_once(text, '    T* ref = new (addr) T();', '    if (!addr) return nullptr;\n    T* ref = new (addr) T();')
    elif name == 'Vector.h':
        text = '#include <new>\n' + text
        text = replace_once(text, '    resize_internal(newSize, true);\n    this->len = newSize;',
                            '    resize_internal(newSize, true);\n    if (newSize > 0 && (!p_data || bufferLen < newSize)) return false;\n    this->len = newSize;')
        text = replace_once(text, '      p_data = newArray(newSize);  // new T[newSize+1];\n      assert(p_data != nullptr);',
                            '      T *next = newArray(newSize);\n      if (!next) return;\n      p_data = next;')
        text = replace_once(text, '    data = new T[newSize];', '    data = new (std::nothrow) T[newSize];')
    elif name == 'CommonHelix.h':
        text = replace_once(text, '    pcm_buffer.resize(maxPCMSize());\n    memset(pcm_buffer.data(), 0, maxPCMSize());',
                            '    pcm_buffer.resize(maxPCMSize());\n'
                            '    if (!frame_buffer.data() || !pcm_buffer.data() || pcm_buffer.size() < maxPCMSize()) {\n'
                            '      frame_buffer.reset();\n      end();\n      return false;\n    }\n'
                            '    memset(pcm_buffer.data(), 0, maxPCMSize());')
    else:
        raise ValueError(name)
    return MARKER + text

def patch_tree(root):
    for relative in ('utils/Allocator.h', 'utils/Vector.h', 'CommonHelix.h'):
        path = root / relative
        original = path.read_text(encoding='utf-8')
        updated = guarded(path.name, original)
        if updated != original:
            path.write_text(updated, encoding='utf-8')

try:
    Import('env')
except NameError:
    env = None
if env is not None:
    patched = False
    def prepare_helix(build_env, node):
        global patched
        if not patched:
            root = Path(build_env.subst('$PROJECT_LIBDEPS_DIR')) / build_env.subst('$PIOENV') / 'libhelix' / 'src'
            patch_tree(root)
            patched = True
        return node
    env.AddBuildMiddleware(prepare_helix)
