"""Win32 file primitives used by portable packaging and artist collaboration.

Ordinary parent handles are held without delete sharing during each file read,
preventing directory replacement after reparse checks. No system installation.
"""
import ctypes
from ctypes import wintypes
import msvcrt
import os
from pathlib import Path

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
    ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
kernel.GetFileAttributesW.argtypes = [wintypes.LPCWSTR]
kernel.GetFileAttributesW.restype = wintypes.DWORD
kernel.MoveFileExW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.DWORD]
kernel.MoveFileExW.restype = wintypes.BOOL
INVALID = ctypes.c_void_p(-1).value
REPARSE, DIRECTORY = 0x400, 0x10


class FileRead:
    def __init__(self, stream, parents):
        self.stream, self.parents = stream, parents
    def __getattr__(self, name):
        return getattr(self.stream, name)
    def __enter__(self):
        return self
    def close(self):
        try:
            self.stream.close()
        finally:
            while self.parents:
                kernel.CloseHandle(self.parents.pop())
    def __exit__(self, *args):
        self.close()


def open_regular(path):
    path = Path(path)
    parents, handle = [], None
    try:
        for parent in reversed(path.parents):
            if parent == Path(path.anchor):
                continue
            handle = kernel.CreateFileW(str(parent), 0x80, 3, None, 3, 0x02200000, None)
            if handle == INVALID:
                raise ctypes.WinError(ctypes.get_last_error())
            parents.append(handle);handle = None
            attrs = kernel.GetFileAttributesW(str(parent))
            if attrs == 0xffffffff or attrs & REPARSE or not attrs & DIRECTORY:
                raise ValueError('Runtime parent is a reparse point or non-directory: '+parent.name)
        handle = kernel.CreateFileW(str(path), 0x80000000, 3, None, 3, 0x08200000, None)
        if handle == INVALID:
            handle = None
            raise ctypes.WinError(ctypes.get_last_error())
        attrs = kernel.GetFileAttributesW(str(path))
        if attrs == 0xffffffff or attrs & (REPARSE | DIRECTORY):
            raise ValueError('Runtime input must be an ordinary file: '+path.name)
        fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
        handle = None
        return FileRead(os.fdopen(fd, 'rb'), parents)
    except BaseException:
        if handle is not None and handle != INVALID:
            kernel.CloseHandle(handle)
        while parents:
            kernel.CloseHandle(parents.pop())
        raise


def publish_directory(stage, destination):
    # MOVEFILE_WRITE_THROUGH, deliberately without REPLACE_EXISTING.
    if not kernel.MoveFileExW(str(stage), str(destination), 8):
        raise ctypes.WinError(ctypes.get_last_error())
