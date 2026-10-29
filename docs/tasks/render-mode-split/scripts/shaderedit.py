"""Helpers for pruning engine/shaders/edge_perspective.frag in task render-mode-split."""
import re, sys
def cut(s, start, end, keep_end=True):
    """Remove s[start:end), start/end markers must each occur exactly once."""
    assert s.count(start) == 1, start
    a = s.index(start)
    assert s.count(end) == 1, end
    b = s.index(end) if keep_end else s.index(end) + len(end)
    assert a < b
    return s[:a] + s[b:]
def drop(s, text):
    assert s.count(text) == 1, text
    return s.replace(text, '')
def rep(s, a, b):
    assert s.count(a) == 1, a
    return s.replace(a, b)
def used(s, name):
    return len(re.findall(r'\b' + re.escape(name) + r'\b', s))
