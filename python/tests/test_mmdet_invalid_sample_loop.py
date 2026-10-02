import ast
import typing
from pathlib import Path


class InvalidImageError(Exception):
    pass


def _load_dataset_class():
    source = (
        Path(__file__).resolve().parents[1]
        / "deeplake"
        / "integrations"
        / "mmdet"
        / "mmdet_dataset_.py"
    )
    parsed = ast.parse(source.read_text(encoding="utf-8"))
    class_node = next(
        node
        for node in parsed.body
        if isinstance(node, ast.ClassDef) and node.name == "MMDetTorchDataset"
    )
    isolated = ast.Module(body=[class_node], type_ignores=[])
    ast.fix_missing_locations(isolated)
    namespace = {
        "Dataset": object,
        "Optional": typing.Optional,
        "Sequence": typing.Sequence,
        "Callable": typing.Callable,
        "InvalidImageError": InvalidImageError,
    }
    exec(compile(isolated, str(source), "exec"), namespace)
    return namespace["MMDetTorchDataset"]


def test_initial_consecutive_invalid_images_advance_to_next_valid(monkeypatch):
    dataset_class = _load_dataset_class()

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
                raise InvalidImageError("decode failed")
            return {"image": index}

    source = Dataset()
    wrapped = dataset_class(source)

    assert wrapped[0] == {"image": 2}
    assert source.read_indices == [0, 1, 2]
    assert wrapped.last_successful_index == 2
