# Run a Blender asset build script with some of its top-level constants
# overridden, without editing the script. Usage (from the repo root):
#   /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 \
#     -P tools/blender-override.py -- <build script> [NAME=expr ...]
#
# The script's top-level code runs first, then each NAME=expr is evaluated in
# the script's own namespace (so its helpers work: 'WRIST=v(0.2,-0.33,-0.14)')
# and assigned to NAME, then the script's main() runs. The script runs from its
# own path, so HERE, __file__ and its env vars (PXE_SKIP_EXPORT, PXE_DESERT_OUT,
# PXE_SKIP_PREVIEW, ...) behave as in a plain run.
#
# Each NAME must already be a top-level name in the script. A value the script
# computed from NAME while loading (a module-level constant derived from it)
# does not follow the override: override the derived value too.
#
# Prints one line per override before main() runs: "override: NAME = <repr>".
# Exit status: whatever main() does (a raised exception exits 1 through
# --python-exit-code 1); 2 on bad usage (no script, a script without main() or
# without an 'if __name__ == "__main__"' guard around its call, a malformed
# NAME=expr, an unknown name, an expression that raises).
import ast
import os
import sys

USAGE = ("usage: Blender -b --python-exit-code 1 -P tools/blender-override.py -- "
         "<build script> [NAME=expr ...]")


def usage_error(message):
    sys.stdout.flush()
    sys.stderr.write("blender-override: %s\n%s\n" % (message, USAGE))
    sys.stderr.flush()
    # os._exit: Blender's own exit handling must not turn the status into 1.
    os._exit(2)


argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if not argv:
    usage_error("no build script given")
script, specs = argv[0], argv[1:]
path = os.path.abspath(script)
if not os.path.isfile(path):
    usage_error("no such script: %s" % script)

overrides = []
for spec in specs:
    name, eq, expr = spec.partition("=")
    name = name.strip()
    if not eq or not name.isidentifier() or not expr.strip():
        usage_error("malformed override %r (expected NAME=expr)" % spec)
    overrides.append((name, expr))

# A script that calls main() unguarded (build_ruins.py) would run its whole
# build, export included, while loading: refuse it before executing anything.
with open(path, encoding="utf-8") as fh:
    source = fh.read()
tree = ast.parse(source, path)
has_main = any(isinstance(n, ast.FunctionDef) and n.name == "main" for n in tree.body)
has_guard = any(isinstance(n, ast.If) and any(isinstance(x, ast.Name) and x.id == "__name__"
                                              for x in ast.walk(n.test)) for n in tree.body)
if not (has_main and has_guard):
    usage_error('%s has no main() behind an \'if __name__ == "__main__"\' guard' % script)

# Top-level code only: __name__ isn't "__main__", so the script's own
# 'if __name__ == "__main__"' guard stays closed and main() is called below.
g = {"__name__": "__blender_override__", "__file__": path, "__builtins__": __builtins__}
sys.path.insert(0, os.path.dirname(path))
exec(compile(tree, path, "exec"), g)

for name, expr in overrides:
    if name not in g:
        usage_error("%s has no top-level name %s" % (script, name))
    try:
        value = eval(expr, g)
    except Exception as e:
        usage_error("%s: %s: %s" % (expr, type(e).__name__, e))
    g[name] = value
    print("override: %s = %r" % (name, value))
sys.stdout.flush()
g["main"]()
