import importlib.util
from pathlib import Path

import numpy as np
import pytest

source = (
    Path(__file__).resolve().parents[1]
    / "deeplake"
    / "integrations"
    / "mm"
    / "upcast_array.py"
)
spec = importlib.util.spec_from_file_location("upcast_array_under_test", source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def test_uint64_values_in_int64_range_are_preserved():
    values = np.array([0, np.iinfo(np.int64).max], dtype=np.uint64)

    converted = module.upcast_array(values)

    assert converted.dtype == np.int64
    assert converted.tolist() == [0, np.iinfo(np.int64).max]


def test_uint64_values_above_int64_range_raise_instead_of_wrapping():
    values = np.array([np.uint64(np.iinfo(np.int64).max) + np.uint64(1)])

    with pytest.raises(OverflowError, match="cannot be represented as int64"):
        module.upcast_array(values)
