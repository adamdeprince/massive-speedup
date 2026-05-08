"""Internal helpers for native condition-code enum classes."""

from __future__ import annotations

import enum
import operator
from collections.abc import Mapping


class _StrictConditionIntEnum(enum.IntEnum):
    def __eq__(self, other):
        if isinstance(other, enum.IntEnum) and type(self) is not type(other):
            return False
        return int.__eq__(self, other)

    def __ne__(self, other):
        if isinstance(other, enum.IntEnum) and type(self) is not type(other):
            return True
        return int.__ne__(self, other)

    def _compare(self, other, operation):
        if isinstance(other, enum.IntEnum) and type(self) is not type(other):
            raise TypeError(
                f"cannot order {type(self).__name__} and {type(other).__name__}"
            )
        if isinstance(other, int):
            return operation(int(self), int(other))
        return NotImplemented

    def __lt__(self, other):
        return self._compare(other, operator.lt)

    def __le__(self, other):
        return self._compare(other, operator.le)

    def __gt__(self, other):
        return self._compare(other, operator.gt)

    def __ge__(self, other):
        return self._compare(other, operator.ge)

    __hash__ = int.__hash__


def make_condition_enum(name: str, members: Mapping[str, int]):
    return _StrictConditionIntEnum(name, members, module="massive_speedup")
