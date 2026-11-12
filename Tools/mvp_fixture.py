"""Validate the sample's data and generate a temporary C++ fixture, not gameplay rules."""

import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
MAX_MONEY = (1 << 63) - 1


def _identifier(value, field):
    if not isinstance(value, str) or value == "none" or not re.fullmatch(r"[a-z0-9_-]+", value):
        raise ValueError(f"{field}: expected a lowercase ASCII identifier other than 'none'")
    return value


def _money(value, field):
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= MAX_MONEY:
        raise ValueError(f"{field}: expected an integer in [0, int64 max]")
    return value


def _text(value, field):
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{field}: text must not be empty")
    if any(ord(char) < 32 and char not in "\n\r\t" for char in value):
        raise ValueError(f"{field}: control characters are not supported")
    try:
        value.encode("utf-8")
    except UnicodeEncodeError as error:
        raise ValueError(f"{field}: text must be valid UTF-8") from error
    return value


def load_fixture(economy_path=None, layout_path=None):
    economy_path = Path(economy_path) if economy_path is not None else ROOT / "Data/mvp_economy.json"
    layout_path = Path(layout_path) if layout_path is not None else ROOT / "Tools/Unreal/warehouse_layout.json"
    economy = json.loads(economy_path.read_text(encoding="utf-8"))
    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    warehouse_id = _identifier(economy["warehouse_id"], "warehouse_id")
    starting = _money(economy["starting_funds"], "starting_funds")
    opening = _money(economy["opening_bid"], "opening_bid")
    demo_bid = _money(economy["demo_bid"], "demo_bid")
    if not 0 < opening <= demo_bid <= starting:
        raise ValueError("Sample requires 0 < opening_bid <= demo_bid <= starting_funds")
    if not isinstance(economy["items"], list) or len(economy["items"]) != 10:
        raise ValueError("Sample requires exactly 10 economic item definitions")
    if not isinstance(layout["items"], list) or len(layout["items"]) != 10:
        raise ValueError("Sample requires exactly 10 layout item definitions")

    content = {}
    for item in layout["items"]:
        item_id = _identifier(item["id"], "layout item id")
        if item_id in content:
            raise ValueError(f"Duplicate layout type: {item_id}")
        content[item_id] = item
    joined = []
    seen = set()
    for item in economy["items"]:
        item_id = _identifier(item["type_id"], "economic type_id")
        if item_id in seen:
            raise ValueError(f"Duplicate economic type: {item_id}")
        if item_id not in content:
            raise ValueError(f"No layout content for economic type: {item_id}")
        seen.add(item_id)
        source = content[item_id]
        joined.append({
            "type_id": item_id,
            "instance_id": f"{warehouse_id}-{item_id}-001",
            "name": _text(source["name"], f"{item_id}.name"),
            "description": _text(source["description"], f"{item_id}.description"),
            "reference_value": _money(item["reference_value"], f"{item_id}.reference_value"),
            "appraisal_value": _money(item["appraisal_value"], f"{item_id}.appraisal_value"),
        })
    if seen != set(content):
        raise ValueError("Layout and economic type IDs do not match")
    final_demo_balance = starting - demo_bid + sum(item["appraisal_value"] for item in joined)
    if final_demo_balance > MAX_MONEY:
        raise ValueError("The scripted sample cannot sell all items without overflowing the balance")
    return {
        "warehouse_id": warehouse_id, "starting_funds": starting,
        "opening_bid": opening, "demo_bid": demo_bid, "items": joined,
    }


def write_cpp_fixture(path, fixture):
    def literal(text):
        # The source text was validated before generation. JSON escapes here are also C++ string escapes.
        return json.dumps(text, ensure_ascii=False)

    lines = [
        "#pragma once", '#include "Gameplay/LastStorageSession.h"',
        "namespace LastStorage::Samples {",
        f"inline constexpr Economy::Money StartingFunds = {fixture['starting_funds']}LL;",
        f"inline constexpr Economy::Money DemoBid = {fixture['demo_bid']}LL;",
        "inline Gameplay::WarehouseDefinition MakeMvpWarehouse() {",
        "Gameplay::WarehouseDefinition Warehouse;",
        f"Warehouse.WarehouseId = {literal(fixture['warehouse_id'])};",
        f"Warehouse.OpeningBid = {fixture['opening_bid']}LL;",
    ]
    for item in fixture["items"]:
        definition = ", ".join([
            literal(item["type_id"]), literal(item["name"]), literal(item["description"]),
            f"{item['reference_value']}LL",
        ])
        lines.append("Warehouse.Items.push_back({{" + definition + "}, "
                     + literal(item["instance_id"]) + f", {item['appraisal_value']}LL" + "});")
    lines += ["return Warehouse;", "}", "}", ""]
    Path(path).write_text("\n".join(lines), encoding="utf-8")
