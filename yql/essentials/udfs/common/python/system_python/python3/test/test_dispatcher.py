import os
from pathlib import Path
import runpy
import subprocess
from types import SimpleNamespace

import pytest
import yatest.common

ROOT = 'yql/essentials/udfs/common/python/system_python/python3'
SQL = """
$f = SystemPython3::f(Callable<(String)->String>, @@
def f(x):
    return x
@@);
SELECT $f('hello');
"""


def binary(path):
    return Path(yatest.common.binary_path(path))


@pytest.fixture
def setup(tmp_path):
    state = SimpleNamespace()
    state.embedded = os.environ.get('SYSTEM_PYTHON3_TEST_EMBEDDED') == '1'
    state.pack = runpy.run_path(yatest.common.source_path(ROOT + '/pack.py'))['pack']
    state.dispatcher = Path(os.environ.get('SYSTEM_PYTHON3_TEST_LIBRARY') or binary(ROOT + '/libsystempython3_udf.so'))
    state.backend = binary(ROOT + '/test/backend/libsystempython_dispatcher_test_udf.so')
    state.python_lib = tmp_path / 'libpython3.so'
    state.packed = tmp_path / 'libsystempython3_udf.so'
    state.pack(state.dispatcher, [('3.8', state.backend)], state.packed)
    program = tmp_path / 'query.sql'
    program.write_text(SQL)
    state.env = os.environ.copy()
    # Only the dynamic loader's library selection matters; python3 is absent.
    state.env['PATH'] = str(tmp_path / 'no-python-executable')
    state.env['LD_LIBRARY_PATH'] = str(tmp_path)
    state.env['LD_PRELOAD'] = str(binary(ROOT + '/test/python/libsystempython_dispatcher_test_python.so'))
    state.env['SYSTEM_PYTHON_DISPATCHER_TEST_VERSION'] = '3.8.99'
    state.env.pop('SYSTEM_PYTHON_DISPATCHER_TEST_INITIALIZED', None)

    def run(library=None, validate=False):
        command = [
            str(binary('yql/essentials/tools/minirun/minirun')),
            '--program',
            str(program),
            '--sql',
            '--print-result',
            '--udf',
            str(library or state.packed),
        ]
        if validate:
            command.append('--validate')
        return subprocess.run(command, cwd=tmp_path, env=state.env, capture_output=True, text=True)

    state.run = run
    return state


@pytest.fixture
def system_runtime(setup):
    if setup.embedded:
        pytest.skip('requires a build with USE_ARCADIA_PYTHON=no')


@pytest.fixture
def embedded_runtime(setup):
    if not setup.embedded:
        pytest.skip('requires a build with USE_ARCADIA_PYTHON=yes')


def test_types_only_does_not_inspect_or_initialize_python(setup):
    setup.env.pop('SYSTEM_PYTHON_DISPATCHER_TEST_VERSION')
    setup.env['SYSTEM_PYTHON_DISPATCHER_TEST_INITIALIZED'] = '1'
    result = setup.run(setup.dispatcher, validate=True)
    assert result.returncode == 0, result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_selects_backend_from_linked_libpython(setup):
    result = setup.run()
    assert result.returncode != 0
    assert 'dispatcher test backend selected' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_unsupported_version_reports_available_backends(setup):
    setup.env['SYSTEM_PYTHON_DISPATCHER_TEST_VERSION'] = '3.12.99'
    result = setup.run()
    assert result.returncode != 0
    assert 'libpython3.so selects 3.12' in result.stderr
    assert 'backends for: 3.8' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_dynamic_loader_requires_valid_libpython_even_for_types_only(setup):
    setup.python_lib.write_bytes(b'not an ELF library')
    result = setup.run(validate=True)
    assert result.returncode != 0
    assert 'libpython3.so' in result.stderr
    assert 'file too short' in result.stderr or 'invalid ELF' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_unpacked_dispatcher_reports_packaging_error(setup):
    result = setup.run(setup.dispatcher)
    assert result.returncode != 0
    assert 'no embedded backends' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_rejects_non_python3_version(setup):
    setup.env['SYSTEM_PYTHON_DISPATCHER_TEST_VERSION'] = '2.7.18'
    result = setup.run()
    assert result.returncode != 0
    assert 'expected CPython 3' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_rejects_free_threaded_python(setup):
    setup.env['SYSTEM_PYTHON_DISPATCHER_TEST_VERSION'] = '3.8.99 free-threading build'
    result = setup.run()
    assert result.returncode != 0
    assert 'free-threaded CPython is not supported' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_rejects_missing_version(setup):
    setup.env.pop('SYSTEM_PYTHON_DISPATCHER_TEST_VERSION')
    result = setup.run()
    assert result.returncode != 0
    assert 'returned no Python version' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_rejects_initialized_runtime(setup):
    setup.env['SYSTEM_PYTHON_DISPATCHER_TEST_INITIALIZED'] = '1'
    result = setup.run()
    assert result.returncode != 0
    assert 'already initialized' in result.stderr


def test_pack_rejects_duplicate_versions(setup, tmp_path):
    with pytest.raises(ValueError, match='duplicate'):
        setup.pack(setup.dispatcher, [('3.8', setup.backend), ('3.8', setup.backend)], tmp_path / 'output.so')


def test_pack_rejects_already_packaged_dispatcher(setup, tmp_path):
    with pytest.raises(ValueError, match='already packaged'):
        setup.pack(setup.packed, [('3.8', setup.backend)], tmp_path / 'output.so')


def test_ignores_python_executable_from_path(setup, tmp_path):
    python = tmp_path / 'python3'
    python.write_text('#!/bin/sh\nexit 99\n')
    python.chmod(0o755)
    setup.env['PATH'] = str(tmp_path)
    result = setup.run()
    if setup.embedded:
        assert result.returncode == 0, result.stderr
    else:
        assert 'dispatcher test backend selected' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_package_with_multiple_backends(setup, tmp_path):
    output = tmp_path / 'multiple.so'
    setup.pack(setup.dispatcher, [('3.7', setup.backend), ('3.8', setup.backend), ('3.12', setup.backend)], output)
    setup.env['SYSTEM_PYTHON_DISPATCHER_TEST_VERSION'] = '3.12.99'
    result = setup.run(output)
    assert 'dispatcher test backend selected' in result.stderr


@pytest.mark.usefixtures('system_runtime')
def test_corrupt_index_size_is_rejected(setup):
    with setup.packed.open('r+b') as stream:
        stream.seek(-24, os.SEEK_END)
        stream.write((1 << 63).to_bytes(8, 'little'))
    result = setup.run()
    assert result.returncode != 0
    assert 'corrupt backend index' in result.stderr


def test_inferred_signature_executes_python_during_query_preparation(setup, tmp_path):
    # Type inference executes Python using only the stable layer, even without
    # any embedded runtime backend.
    (tmp_path / 'query.sql').write_text('''
$f = SystemPython3::f(@@
def f(x):
    """(String)->String"""
    return x
@@);
SELECT $f('hello');
''')
    result = setup.run(setup.dispatcher, validate=True)
    assert result.returncode == 0, result.stderr


def validate_script(setup, tmp_path, script):
    (tmp_path / 'query.sql').write_text("$f = SystemPython3::f(@@" + script + "@@); SELECT $f('hello');")
    return setup.run(setup.dispatcher, validate=True)


def test_annotations_without_runtime_backend(setup, tmp_path):
    # No matching backend exists, and the dispatcher is not packaged at all.
    setup.env['SYSTEM_PYTHON_DISPATCHER_TEST_VERSION'] = '3.99.0'
    result = validate_script(
        setup,
        tmp_path,
        '''
from yql.typing import String
def f(x: String) -> String:
    return x
''',
    )
    assert result.returncode == 0, result.stderr


def test_annotations_resolve_import_aliases_and_module_globals(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
import yql.typing as t
Alias = t.String
def f(x: Alias) -> t.String:
    return x
''',
    )
    assert result.returncode == 0, result.stderr


def test_inference_does_not_call_user_function_body(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
def f(x):
    """(String)->String"""
    raise RuntimeError('function body must not run during inference')
''',
    )
    assert result.returncode == 0, result.stderr


def test_invalid_annotation_has_python_error(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
def f(x: int) -> str:
    return str(x)
''',
    )
    assert result.returncode != 0
    assert 'Unknown type annotation' in result.stderr


def test_missing_return_annotation_has_python_error(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
from yql.typing import String
def f(x: String):
    return x
''',
    )
    assert result.returncode != 0
    assert 'No return type annotation' in result.stderr


def test_invalid_python_syntax_is_reported(setup, tmp_path):
    result = validate_script(setup, tmp_path, 'def f(:\n')
    assert result.returncode != 0
    assert 'SyntaxError' in result.stderr


def test_import_failure_is_reported(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
import _yql_signature_nonexistent_module
def f(x):
    """(String)->String"""
    return x
''',
    )
    assert result.returncode != 0
    assert 'ModuleNotFoundError' in result.stderr


def test_backend_attaches_after_stable_inference(setup, tmp_path):
    (tmp_path / 'query.sql').write_text('''
$f = SystemPython3::f(@@
def f(x):
    """(String)->String"""
    return x
@@);
SELECT $f('hello');
''')
    result = setup.run()
    if setup.embedded:
        assert result.returncode == 0, result.stderr
    else:
        assert result.returncode != 0
        assert 'dispatcher test backend selected' in result.stderr
    assert 'already initialized' not in result.stderr


def test_nested_yql_type_annotations_without_backend(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
from yql.typing import String, List, Struct, Optional, Int32, Tagged
def f(x: String) -> List[Struct["value": Optional[Int32], "tag": Tagged[String, "demo"]]]:
    raise RuntimeError('type inference must not execute this body')
''',
    )
    assert result.returncode == 0, result.stderr


def test_none_default_is_supported(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
from yql.typing import String, Optional
def f(x: Optional[String] = None) -> String:
    return x or b""
''',
    )
    assert result.returncode == 0, result.stderr


def test_non_none_default_is_reported(setup, tmp_path):
    result = validate_script(
        setup,
        tmp_path,
        '''
from yql.typing import String
def f(x: String = b"default") -> String:
    return x
''',
    )
    assert result.returncode != 0
    assert 'Only None is supported as default value' in result.stderr


@pytest.mark.usefixtures('embedded_runtime')
def test_embedded_runtime_executes_without_system_library_or_backend(setup):
    setup.python_lib.write_bytes(b'not an ELF library')
    result = setup.run(setup.dispatcher)
    assert result.returncode == 0, result.stderr
    assert 'hello' in result.stdout


@pytest.mark.usefixtures('embedded_runtime')
def test_embedded_runtime_executes_annotated_function(setup, tmp_path):
    (tmp_path / 'query.sql').write_text('''
$f = SystemPython3::f(@@
from yql.typing import String
def f(x: String) -> String:
    return x + b"!"
@@);
SELECT $f('hello');
''')
    result = setup.run(setup.dispatcher)
    assert result.returncode == 0, result.stderr
    assert 'hello!' in result.stdout
