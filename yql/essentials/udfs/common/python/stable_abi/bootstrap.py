import sys
import types


def install_types(type_base_source, typing_source, yql_typing_source):
    # Keep the bundled type-description code separate from the user's installed
    # yt package. In particular, do not shadow yt.wrapper or yt.yson.
    package = types.ModuleType('_yql_signature_type_info')
    package.__path__ = []
    sys.modules[package.__name__] = package

    def load(name, source):
        module = types.ModuleType(package.__name__ + '.' + name)
        sys.modules[module.__name__] = module
        setattr(package, name, module)
        exec(compile(source, module.__name__, 'exec'), module.__dict__)
        return module

    type_base = load('type_base', type_base_source)
    typing = load('typing', typing_source)
    yql = sys.modules.get('yql')
    if yql is None:
        yql = types.ModuleType('yql')
        yql.__path__ = []
        sys.modules['yql'] = yql
    yql_typing = types.ModuleType('yql.typing')
    sys.modules['yql.typing'] = yql_typing
    yql.typing = yql_typing
    context = {'__name__': '_yql_signature_annotations'}
    exec(compile(yql_typing_source, context['__name__'], 'exec'), context)
    context['main'](type_base, typing)
