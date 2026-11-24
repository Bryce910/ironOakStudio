"""Small read-only snapshot of player authoring/preferences for demo QA."""
import hashlib
from benchmark_graphics_settings import PROJECT


def protected_state():
    paths={PROJECT/'world.studio', PROJECT/'graphics.settings',
           PROJECT/'ai/npc_appearances.vwlooks'}
    ai=PROJECT/'ai'
    if ai.exists():
        paths.update(p for p in ai.rglob('*') if p.is_file() and p.suffix in ('.vwlooks','.vwmemory','.vwminds'))
    return {str(p.relative_to(PROJECT)):
            hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None
            for p in sorted(paths)}
