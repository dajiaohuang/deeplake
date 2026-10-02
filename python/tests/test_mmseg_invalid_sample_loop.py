import importlib.util
import sys
import types
from pathlib import Path

import pytest


class InvalidImageError(Exception):
    pass


class InvalidSegmentError(Exception):
    pass


def _load_dataset_class(monkeypatch):
    torch = types.ModuleType("torch")
    torch.__path__ = []
    torch_utils = types.ModuleType("torch.utils")
    torch_utils.__path__ = []
    torch_data = types.ModuleType("torch.utils.data")
    torch_data.Dataset = object
    monkeypatch.setitem(sys.modules, "torch", torch)
    monkeypatch.setitem(sys.modules, "torch.utils", torch_utils)
    monkeypatch.setitem(sys.modules, "torch.utils.data", torch_data)

    prettytable = types.ModuleType("prettytable")
    prettytable.PrettyTable = object
    monkeypatch.setitem(sys.modules, "prettytable", prettytable)

    mmcv = types.ModuleType("mmcv")
    mmcv.__path__ = []
    mmcv_utils = types.ModuleType("mmcv.utils")
    mmcv_utils.print_log = lambda *args, **kwargs: None
    monkeypatch.setitem(sys.modules, "mmcv", mmcv)
    monkeypatch.setitem(sys.modules, "mmcv.utils", mmcv_utils)

    mmseg = types.ModuleType("mmseg")
    mmseg.__path__ = []
    mmseg_core = types.ModuleType("mmseg.core")
    mmseg_core.eval_metrics = lambda *args, **kwargs: None
    mmseg_core.intersect_and_union = lambda *args, **kwargs: None
    mmseg_core.pre_eval_to_metrics = lambda *args, **kwargs: None
    monkeypatch.setitem(sys.modules, "mmseg", mmseg)
    monkeypatch.setitem(sys.modules, "mmseg.core", mmseg_core)

    for name in (
        "deeplake",
        "deeplake.integrations",
        "deeplake.integrations.mm",
        "deeplake.integrations.mmseg",
    ):
        package = types.ModuleType(name)
        package.__path__ = []
        monkeypatch.setitem(sys.modules, name, package)

    exceptions = types.ModuleType("deeplake.integrations.mm.exceptions")
    exceptions.InvalidImageError = InvalidImageError
    exceptions.InvalidSegmentError = InvalidSegmentError
    monkeypatch.setitem(sys.modules, exceptions.__name__, exceptions)
    upcast = types.ModuleType("deeplake.integrations.mm.upcast_array")
    upcast.upcast_array = lambda value: value
    monkeypatch.setitem(sys.modules, upcast.__name__, upcast)

    source = (
        Path(__file__).resolve().parents[1]
        / "deeplake"
        / "integrations"
        / "mmseg"
        / "mmseg_dataset_.py"
    )
    spec = importlib.util.spec_from_file_location("mmseg_dataset_under_test", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.MMSegTorchDataset


def test_initial_consecutive_invalid_samples_advance_to_next_valid(monkeypatch):
    dataset_class = _load_dataset_class(monkeypatch)

    class Column:
        name = "image"

    class Schema:
        columns = [Column()]

    class Dataset:
        schema = Schema()

        def __init__(self):
            self.read_indices = []

        def __len__(self):
            return 3

        def __getitem__(self, index):
            self.read_indices.append(index)
            if index in (0, 1):
                raise InvalidImageError("image", "decode failed")
            return {"image": index}

    source = Dataset()
    wrapped = dataset_class(source)

    assert wrapped[0] == {"image": 2}
    assert source.read_indices == [0, 1, 2]
