from .. import DuckDBPyConnection

from typing import Dict

SPECIAL: FunctionNullHandling
DEFAULT: FunctionNullHandling

NATIVE: PythonUDFType
ARROW: PythonUDFType

COMMON: PythonUDFKind
PREDICTION: PythonUDFKind
PROCESS_PREDICTION: PythonUDFKind
SCHEDULE_PREDICTION: PythonUDFKind
THREAD_SCHEDULE_PREDICTION: PythonUDFKind

class FunctionNullHandling:
    DEFAULT: FunctionNullHandling
    SPECIAL: FunctionNullHandling
    def __int__(self) -> int: ...
    def __index__(self) -> int: ...
    @property
    def __members__(self) -> Dict[str, FunctionNullHandling]: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class PythonUDFType:
    NATIVE: PythonUDFType
    ARROW: PythonUDFType
    def __int__(self) -> int: ...
    def __index__(self) -> int: ...
    @property
    def __members__(self) -> Dict[str, PythonUDFType]: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...


class PythonUDFKind:
    COMMON: PythonUDFKind
    PREDICTION: PythonUDFKind
    PROCESS_PREDICTION: PythonUDFKind
    SCHEDULE_PREDICTION: PythonUDFKind
    THREAD_SCHEDULE_PREDICTION: PythonUDFKind
    def __int__(self) -> int: ...
    def __index__(self) -> int: ...
    @property
    def __members__(self) -> Dict[str, PythonUDFKind]: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...