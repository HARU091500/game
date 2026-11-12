"""Portable data/preflight tests only. These do NOT execute Unreal or prove PIE works."""

import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Tools" / "Unreal" / "create_warehouse.py"
SPEC = importlib.util.spec_from_file_location("create_warehouse", SCRIPT)
WAREHOUSE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WAREHOUSE)
LAYOUT = SCRIPT.with_name("warehouse_layout.json")


class LayoutTests(unittest.TestCase):
    def setUp(self):
        self.layout = json.loads(LAYOUT.read_text(encoding="utf-8"))

    def validate(self, layout):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "layout.json"
            path.write_text(json.dumps(layout, ensure_ascii=False), encoding="utf-8")
            return WAREHOUSE.load_layout(path)

    def test_repository_layout_has_ten_distinct_types(self):
        result = WAREHOUSE.load_layout(LAYOUT)
        self.assertEqual(len(result["items"]), 10)
        self.assertEqual(len({item["id"].lower() for item in result["items"]}), 10)
        self.assertEqual(len({item["name"] for item in result["items"]}), 10)

    def test_wrong_item_count_is_rejected(self):
        self.layout["items"].pop()
        with self.assertRaisesRegex(ValueError, "exactly 10"):
            self.validate(self.layout)

    def test_case_insensitive_duplicate_id_is_rejected(self):
        self.layout["items"][1]["id"] = self.layout["items"][0]["id"].upper()
        with self.assertRaisesRegex(ValueError, "duplicate item id"):
            self.validate(self.layout)

    def test_none_fname_is_rejected(self):
        self.layout["items"][0]["id"] = "None"
        with self.assertRaises(ValueError):
            self.validate(self.layout)

    def test_empty_description_is_rejected(self):
        self.layout["items"][0]["description"] = "  "
        with self.assertRaisesRegex(ValueError, "description"):
            self.validate(self.layout)

    def test_wall_intersection_is_rejected(self):
        self.layout["items"][0]["location_cm"][0] = self.layout["room_cm"]["width"] / 2
        with self.assertRaisesRegex(ValueError, "wall"):
            self.validate(self.layout)

    def test_item_overlap_is_rejected(self):
        self.layout["items"][1]["location_cm"] = copy.deepcopy(self.layout["items"][0]["location_cm"])
        with self.assertRaisesRegex(ValueError, "overlaps another item"):
            self.validate(self.layout)

    def test_player_overlap_is_rejected(self):
        self.layout["items"][0]["location_cm"][:2] = self.layout["player_start_cm"][:2]
        with self.assertRaisesRegex(ValueError, "overlaps the player"):
            self.validate(self.layout)

    def test_nonfinite_coordinate_is_rejected(self):
        self.layout["items"][0]["location_cm"][0] = float("inf")
        with self.assertRaisesRegex(ValueError, "finite"):
            self.validate(self.layout)

    def test_floor_intersection_is_rejected(self):
        self.layout["items"][0]["location_cm"][2] = 0
        with self.assertRaisesRegex(ValueError, "floor or ceiling"):
            self.validate(self.layout)

    def test_player_ceiling_clearance_is_required(self):
        self.layout["player_start_cm"][2] = self.layout["room_cm"]["height"]
        with self.assertRaisesRegex(ValueError, "clearance"):
            self.validate(self.layout)

    def test_parent_path_is_rejected(self):
        self.layout["map_path"] = "/Game/LastStorage/Maps/../ExistingMap"
        with self.assertRaisesRegex(ValueError, "package segment"):
            self.validate(self.layout)


class EditorPreflightTests(unittest.TestCase):
    """An API stub verifies abort-before-mutation paths, not Unreal API compatibility."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.subsystem = Mock()
        self.unreal = types.SimpleNamespace(
            SystemLibrary=types.SimpleNamespace(get_engine_version=Mock(return_value="5.6.1-test")),
            log=Mock(),
            EditorLoadingAndSavingUtils=types.SimpleNamespace(
                get_dirty_content_packages=Mock(return_value=[]),
                get_dirty_map_packages=Mock(return_value=[]),
            ),
            EditorAssetLibrary=types.SimpleNamespace(
                does_asset_exist=Mock(return_value=False),
                load_blueprint_class=Mock(return_value=None),
            ),
            Paths=types.SimpleNamespace(
                project_content_dir=Mock(return_value=self.folder.name),
                convert_relative_path_to_full=lambda path: str(Path(path).resolve()),
            ),
            get_editor_subsystem=Mock(return_value=self.subsystem),
        )

    def run_preflight(self):
        with patch.dict(sys.modules, {"unreal": self.unreal}):
            WAREHOUSE.create_warehouse("/Game/FirstPerson/Blueprints/TestGameMode")

    def test_dirty_map_aborts_before_level_creation(self):
        self.unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages.return_value = [object()]
        with self.assertRaisesRegex(RuntimeError, "Unsaved"):
            self.run_preflight()
        self.unreal.get_editor_subsystem.assert_not_called()

    def test_registered_existing_map_is_never_replaced(self):
        self.unreal.EditorAssetLibrary.does_asset_exist.return_value = True
        with self.assertRaises(FileExistsError):
            self.run_preflight()
        self.unreal.get_editor_subsystem.assert_not_called()

    def test_unregistered_existing_map_file_is_never_replaced(self):
        existing = Path(self.folder.name) / "LastStorage/Maps/L_Warehouse.umap"
        existing.parent.mkdir(parents=True)
        existing.write_bytes(b"existing-user-file")
        with self.assertRaises(FileExistsError):
            self.run_preflight()
        self.assertEqual(existing.read_bytes(), b"existing-user-file")
        self.unreal.get_editor_subsystem.assert_not_called()

    def test_missing_game_mode_aborts_before_level_creation(self):
        with self.assertRaisesRegex(ValueError, "GameMode Blueprint not found"):
            self.run_preflight()
        self.unreal.get_editor_subsystem.assert_not_called()


if __name__ == "__main__":
    unittest.main()
