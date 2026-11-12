"""Checks the ten-type sample's content and price inputs before C++ generation."""

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("mvp_fixture", ROOT / "Tools/mvp_fixture.py")
FIXTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURE)


class MvpFixtureTests(unittest.TestCase):
    def setUp(self):
        self.economy = json.loads((ROOT / "Data/mvp_economy.json").read_text(encoding="utf-8"))
        self.layout = json.loads((ROOT / "Tools/Unreal/warehouse_layout.json").read_text(encoding="utf-8"))

    def load(self):
        with tempfile.TemporaryDirectory() as folder:
            economy = Path(folder) / "economy.json"
            layout = Path(folder) / "layout.json"
            economy.write_text(json.dumps(self.economy, ensure_ascii=False), encoding="utf-8")
            layout.write_text(json.dumps(self.layout, ensure_ascii=True), encoding="utf-8")
            return FIXTURE.load_fixture(economy, layout)

    def test_ten_types_and_demo_accounting(self):
        data = FIXTURE.load_fixture()
        self.assertEqual(len(data["items"]), 10)
        self.assertEqual(len({item["instance_id"] for item in data["items"]}), 10)
        total = sum(item["appraisal_value"] for item in data["items"])
        self.assertEqual(total, 1750)
        self.assertEqual(data["starting_funds"] - data["demo_bid"] + total, 2450)

    def test_duplicate_economic_type_is_rejected(self):
        self.economy["items"][1] = copy.deepcopy(self.economy["items"][0])
        with self.assertRaisesRegex(ValueError, "Duplicate economic"):
            self.load()

    def test_unknown_type_is_rejected(self):
        self.economy["items"][0]["type_id"] = "unknown"
        with self.assertRaisesRegex(ValueError, "No layout content"):
            self.load()

    def test_wrong_item_count_is_rejected(self):
        self.economy["items"].pop()
        with self.assertRaisesRegex(ValueError, "exactly 10"):
            self.load()

    def test_prices_must_be_nonnegative_int64(self):
        for invalid in (-1, True, 1.5, FIXTURE.MAX_MONEY + 1):
            with self.subTest(value=invalid):
                self.economy["items"][0]["appraisal_value"] = invalid
                with self.assertRaisesRegex(ValueError, "integer"):
                    self.load()

    def test_demo_must_be_affordable(self):
        self.economy["demo_bid"] = self.economy["starting_funds"] + 1
        with self.assertRaisesRegex(ValueError, "opening_bid"):
            self.load()

    def test_demo_balance_overflow_is_rejected(self):
        self.economy["items"][0]["appraisal_value"] = FIXTURE.MAX_MONEY
        with self.assertRaisesRegex(ValueError, "overflowing"):
            self.load()

    def test_duplicate_layout_type_is_rejected(self):
        self.layout["items"][1] = copy.deepcopy(self.layout["items"][0])
        with self.assertRaisesRegex(ValueError, "Duplicate layout"):
            self.load()

    def test_empty_name_is_rejected(self):
        self.layout["items"][0]["name"] = "  "
        with self.assertRaisesRegex(ValueError, "text must not be empty"):
            self.load()

    def test_control_character_in_text_is_rejected(self):
        self.layout["items"][0]["description"] = "bad\0text"
        with self.assertRaisesRegex(ValueError, "control characters"):
            self.load()

    def test_unpaired_surrogate_in_text_is_rejected(self):
        self.layout["items"][0]["name"] = "bad\ud800text"
        with self.assertRaisesRegex(ValueError, "valid UTF-8"):
            self.load()


if __name__ == "__main__":
    unittest.main()
