"""Exact read-only LIVE shader view for immutable historical checkers only.

The parent verifies physical old/new aliases and every actual shader byte first.
This shim never changes files, source/build validation, catalog or nonshader IO.
"""
import atexit,json,os,sys
from pathlib import Path
try:
    config=json.loads(os.environ['VALWOOD_HISTORICAL_SHADER_VIEW'])
    if str(Path(sys.argv[0]).absolute()) not in config['allowed_scripts']:
        os._exit(91)  # sitecustomize exceptions are otherwise swallowed by Python.
except Exception:
    os._exit(91)
aliases={Path(k):Path(v) for k,v in config['alias_targets'].items()}
read_bank=Path(config['read_bank']);counts={'resolve':{},'open':{}}
original_resolve=Path.resolve;original_open=Path.open

def resolve(path,*args,**kwargs):
    if path in aliases:
        key=str(path);counts['resolve'][key]=counts['resolve'].get(key,0)+1
        return aliases[path]
    return original_resolve(path,*args,**kwargs)

def open_read(path,mode='r',*args,**kwargs):
    for alias in aliases:
        if path.is_relative_to(alias):
            if any(c in mode for c in 'wax+'):
                raise RuntimeError('Historical checker attempted shader write')
            relative=path.relative_to(alias)
            if relative.as_posix() not in config['shader_paths']:
                raise RuntimeError('Historical checker read an unapproved shader path')
            key=str(alias);counts['open'][key]=counts['open'].get(key,0)+1
            return original_open(read_bank/relative,mode,*args,**kwargs)
    return original_open(path,mode,*args,**kwargs)
Path.resolve=resolve;Path.open=open_read
@atexit.register
def record():
    Path.resolve=original_resolve;Path.open=original_open
    with original_open(Path(config['trace'])/(str(os.getpid())+'.json'),'x') as handle:
        json.dump(dict(script=str(Path(sys.argv[0]).absolute()),counts=counts),handle)
