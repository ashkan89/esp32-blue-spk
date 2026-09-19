"""Compile a project-local WebServer parser with a pre-body admission hook.

Arduino invokes handlers after multipart fields have already been allocated.
The bounded API therefore needs admission immediately after headers. This
middleware replaces only this project's Parsing.cpp object; the installed
framework stays untouched. Fail closed if the pinned parser changes shape.
"""
from pathlib import Path

def guarded(source):
    anchor='    if (!isForm && _currentHandler && _currentHandler->canRaw(*this, _currentUri)) {'
    if source.count(anchor)!=1: raise RuntimeError('WebServer parser changed: review HTTP admission guard')
    declaration='extern "C" bool speaker_http_body_allowed(const char *, const char *, size_t);\n'
    source=source.replace('#include "WebServer.h"','#include "WebServer.h"\n'+declaration,1)
    guard='''    // Reject before raw/form body allocation, including unknown routes.
    if (!speaker_http_body_allowed(_currentUri.c_str(), header("Content-Type").c_str(), _clientContentLength)) {
      send(413, "application/json", "{\\"error\\":\\"Request refused: check sign-in, content type and device limits\\"}");
      client.stop();
      return false;
    }
'''
    return source.replace(anchor,guard+anchor,1)

try:
    Import('env')
except NameError:
    env=None
if env is not None:
    def replace_parser(build_env,node):
        path=Path(node.srcnode().get_abspath())
        if path.name!='Parsing.cpp' or 'WebServer' not in path.parts: return node
        target=Path(build_env.subst('$BUILD_DIR'))/'guarded_http'/'Parsing.cpp'
        target.parent.mkdir(parents=True,exist_ok=True)
        text=guarded(path.read_text(encoding='utf-8'))
        if not target.exists() or target.read_text(encoding='utf-8')!=text: target.write_text(text,encoding='utf-8')
        return build_env.File(str(target))
    env.AddBuildMiddleware(replace_parser)
