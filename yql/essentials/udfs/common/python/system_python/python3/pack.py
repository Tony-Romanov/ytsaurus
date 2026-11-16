#!/usr/bin/env python3
"""Package version-specific SystemPython backends into a standalone Linux UDF.

Build python3/ and the required python3_X targets with the same system Python
flags used by python3_X, e.g. for Python 3.14:
    ya make -r -DUSE_ARCADIA_PYTHON=no -DUSE_LOCAL_PYTHON=yes \\
        -DPYTHON_BIN=python3.14 -DPYTHON_CONFIG=python3.14-config
Then run:
    python3 pack.py --dispatcher libsystempython3_udf.so \
        --backend 3.12 libsystempython3_12_udf.so \
        --backend 3.14 libsystempython3_14_udf.so --output packaged.so

The system build links to libpython3.so through the Stable ABI. Its version is
selected by the system dynamic loader, independently of PATH. Signature inference
runs entirely through the Stable ABI; only executing user UDFs needs a backend.
The default build uses the repository's Python and executes UDFs directly,
without packaging or a system Python dependency, like the other Python UDFs.

For a system build, the output is the only UDF file delivered to a job. libpython3.so
must be installed on every host loading it, including hosts resolving types. Strip the
input libraries before packaging; stripping the output removes its payload.
The query-tracker build_system_python_udfs.sh performs these steps automatically.
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import tempfile

MAGIC = b'YQL_SYSTEM_PY3\0\0'
FOOTER = struct.Struct('<Q16s')


def pack(dispatcher, backends, output):
    dispatcher, output = Path(dispatcher), Path(output)
    entries = {}
    for version, path in backends:
        if not re.fullmatch(r'3\.[0-9]+', version) or version in entries:
            raise ValueError('Invalid or duplicate Python backend version: ' + version)
        path = Path(path)
        with path.open('rb') as stream:
            if stream.read(4) != b'\x7fELF':
                raise ValueError('Backend is not an ELF shared library: ' + str(path))
        entries[version] = path
    if not entries:
        raise ValueError('At least one backend is required')
    with dispatcher.open('rb') as stream:
        if stream.read(4) != b'\x7fELF':
            raise ValueError('Dispatcher is not an ELF shared library')
        if dispatcher.stat().st_size >= FOOTER.size:
            stream.seek(-FOOTER.size, os.SEEK_END)
            if FOOTER.unpack(stream.read())[1] == MAGIC:
                raise ValueError('Dispatcher is already packaged; use the original build artifact')
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
            with dispatcher.open('rb') as source:
                shutil.copyfileobj(source, stream)
            index = []
            for version, path in sorted(entries.items(), key=lambda item: int(item[0].split('.')[1])):
                offset = stream.tell()
                with path.open('rb') as source:
                    shutil.copyfileobj(source, stream)
                index.append('%s\t%d\t%d\n' % (version, offset, stream.tell() - offset))
            index = ''.join(index).encode('ascii')
            if len(index) > 65536:
                raise ValueError('Backend index is too large')
            stream.write(index)
            stream.write(FOOTER.pack(len(index), MAGIC))
        shutil.copymode(dispatcher, temporary)
        os.replace(temporary, output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dispatcher', required=True, type=Path)
    parser.add_argument('--backend', action='append', nargs=2, metavar=('VERSION', 'LIBRARY'), required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    pack(args.dispatcher, args.backend, args.output)


if __name__ == '__main__':
    main()
