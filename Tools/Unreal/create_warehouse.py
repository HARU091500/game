"""UE Editor Python: create a NEW warehouse blockout; never replace a map.

Read docs/WINDOWS_QUICKSTART.md first. Run with runpy.run_path and supply
GAME_MODE_ASSET_PATH through init_globals. Item presentation uses a Blueprint
child of LastStorageInspectableItem. This script does not edit defaults,
Blueprints, input mappings, or an existing level. No third-party assets are used.
"""

import json
import math
from pathlib import Path


# Copy Reference on YOUR First Person GameMode BP gives the real object path.
# Example only (template paths vary): /Game/FirstPerson/Blueprints/BP_FirstPersonGameMode
GAME_MODE_ASSET_PATH = globals().get("GAME_MODE_ASSET_PATH", "")
ITEM_BLUEPRINT_ASSET_PATH = globals().get("ITEM_BLUEPRINT_ASSET_PATH", "/Game/LastStorage/Blueprints/BP_InspectableItem")
ITEM_SCALE = 0.65  # Engine cube is 100 cm; 65 cm placeholder per item.


def _vector(value, field):
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError(f"{field}: expected three coordinates in cm")
    if any(isinstance(x, bool) or not isinstance(x, (int, float)) or not math.isfinite(x) for x in value):
        raise ValueError(f"{field}: coordinates must be finite numbers")
    return value


def load_layout(path):
    """Validate portable layout data before loading UE or mutating the editor."""
    with Path(path).open(encoding="utf-8") as stream:
        layout = json.load(stream)
    map_path = layout["map_path"]
    if not isinstance(map_path, str) or not map_path.startswith("/Game/LastStorage/Maps/"):
        raise ValueError("map_path must be below /Game/LastStorage/Maps/")
    if any(not part or not part.replace("_", "").isalnum() for part in map_path.split("/")[1:]):
        raise ValueError("map_path contains an invalid package segment")
    room = layout["room_cm"]
    for field in ("width", "depth", "height", "wall_thickness"):
        number = room[field]
        if isinstance(number, bool) or not isinstance(number, (int, float)) or not math.isfinite(number) or number <= 0:
            raise ValueError(f"room_cm.{field} must be a positive finite number")
    if room["width"] < 1000 or room["depth"] < 800 or room["height"] < 250:
        raise ValueError("room is too small for this MVP blockout")
    player = _vector(layout["player_start_cm"], "player_start_cm")
    if abs(player[0]) + 55 >= room["width"] / 2 or abs(player[1]) + 55 >= room["depth"] / 2:
        raise ValueError("PlayerStart must be inside the room with capsule clearance")
    if player[2] < 100 or player[2] + 100 >= room["height"]:
        raise ValueError("PlayerStart needs floor/ceiling clearance for a standard capsule")
    items = layout["items"]
    if not isinstance(items, list) or len(items) != 10:
        raise ValueError("This MVP requires exactly 10 item definitions")
    seen_ids = set()
    locations = []
    half_item = 100 * ITEM_SCALE / 2
    for index, item in enumerate(items):
        item_id = item["id"]
        if not isinstance(item_id, str) or not item_id or not item_id.replace("_", "").isascii() or not item_id.replace("_", "").isalnum():
            raise ValueError(f"items[{index}].id must be an ASCII identifier")
        if item_id.lower() == "none":
            raise ValueError("item id 'None' is reserved for Unreal's unset FName")
        if item_id.lower() in seen_ids:
            raise ValueError(f"duplicate item id: {item_id}")
        seen_ids.add(item_id.lower())
        for field in ("name", "description"):
            if not isinstance(item[field], str) or not item[field].strip():
                raise ValueError(f"items[{index}].{field} must not be empty")
        position = _vector(item["location_cm"], f"items[{index}].location_cm")
        if abs(position[0]) + half_item >= room["width"] / 2 or abs(position[1]) + half_item >= room["depth"] / 2:
            raise ValueError(f"item {item_id} intersects a wall")
        if position[2] - half_item < 0 or position[2] + half_item >= room["height"]:
            raise ValueError(f"item {item_id} intersects floor or ceiling")
        for previous in locations:
            if all(abs(position[axis] - previous[axis]) < half_item * 2 for axis in range(3)):
                raise ValueError(f"item {item_id} overlaps another item")
        if abs(position[0] - player[0]) < half_item + 55 and abs(position[1] - player[1]) < half_item + 55:
            raise ValueError(f"item {item_id} overlaps the player start")
        locations.append(position)
    return layout


def create_warehouse(game_mode_asset_path, item_blueprint_asset_path=ITEM_BLUEPRINT_ASSET_PATH):
    layout = load_layout(Path(__file__).with_name("warehouse_layout.json"))
    if not game_mode_asset_path or not game_mode_asset_path.startswith("/Game/"):
        raise ValueError("Set GAME_MODE_ASSET_PATH to your existing First Person GameMode Blueprint object path.")
    if not item_blueprint_asset_path or not item_blueprint_asset_path.startswith("/Game/"):
        raise ValueError("Set ITEM_BLUEPRINT_ASSET_PATH to a Blueprint child of LastStorageInspectableItem.")

    import unreal  # Only available inside UE Editor, not system Python.

    engine_version = unreal.SystemLibrary.get_engine_version()
    if engine_version.split(".", 1)[0] != "5":
        raise RuntimeError(f"This project requires UE5; detected {engine_version}")
    unreal.log(f"LAST STORAGE: running warehouse creation on Unreal Engine {engine_version}; PIE validation remains required.")
    if unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError("Unsaved packages exist. Save All, then rerun. No map was created.")
    map_path = layout["map_path"]
    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    map_file = Path(content_dir) / (map_path[len("/Game/"):] + ".umap")
    if unreal.EditorAssetLibrary.does_asset_exist(map_path) or map_file.exists():
        raise FileExistsError(f"Map already exists; refusing to replace it: {map_path}")
    game_mode = unreal.EditorAssetLibrary.load_blueprint_class(game_mode_asset_path)
    if game_mode is None:
        raise ValueError(f"GameMode Blueprint not found: {game_mode_asset_path}")
    if not isinstance(unreal.get_default_object(game_mode), unreal.GameModeBase):
        raise ValueError("The selected Blueprint is not a GameModeBase subclass")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    native_item_class = getattr(unreal, "LastStorageInspectableItem", None)
    if cube is None or native_item_class is None:
        raise RuntimeError("Enable/build LastStorage and check the Engine BasicShapes Cube before rerunning.")
    item_class = unreal.EditorAssetLibrary.load_blueprint_class(item_blueprint_asset_path)
    if item_class is None or not isinstance(unreal.get_default_object(item_class), native_item_class):
        raise ValueError("Create BP_InspectableItem as a Blueprint child of LastStorageInspectableItem first.")

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.new_level(map_path):
        raise RuntimeError(f"Could not create new map: {map_path}")
    # From this point, an exception can leave a partial NEW level. Nothing is
    # automatically deleted; inspect it and use a different map path to retry.
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)

    def spawn(actor_class, label, position, rotation=None):
        actor = actors.spawn_actor_from_class(actor_class, unreal.Vector(*position), rotation or unreal.Rotator())
        if actor is None:
            raise RuntimeError(f"Failed to spawn {label}; partial new map retained for inspection")
        actor.set_actor_label(label)
        return actor

    def block(label, position, scale):
        actor = spawn(unreal.StaticMeshActor, label, position)
        component = actor.static_mesh_component
        component.set_static_mesh(cube)
        component.set_collision_profile_name("BlockAll")
        actor.set_actor_scale3d(unreal.Vector(*scale))

    room = layout["room_cm"]
    width, depth, height, thickness = (room[key] for key in ("width", "depth", "height", "wall_thickness"))
    block("Warehouse_Floor", (0, 0, -thickness / 2), ((width + 2 * thickness) / 100, (depth + 2 * thickness) / 100, thickness / 100))
    block("Warehouse_Ceiling", (0, 0, height + thickness / 2), ((width + 2 * thickness) / 100, (depth + 2 * thickness) / 100, thickness / 100))
    for sign, side in ((-1, "West"), (1, "East")):
        block(f"Warehouse_{side}", (sign * (width + thickness) / 2, 0, height / 2), (thickness / 100, (depth + 2 * thickness) / 100, height / 100))
    for sign, side in ((-1, "South"), (1, "North")):
        block(f"Warehouse_{side}", (0, sign * (depth + thickness) / 2, height / 2), (width / 100, thickness / 100, height / 100))
    spawn(unreal.PlayerStart, "Warehouse_PlayerStart", layout["player_start_cm"], unreal.Rotator(0, 90, 0))
    for index, (x, y) in enumerate(((-300, -200), (300, -200), (-300, 250), (300, 250))):
        light = spawn(unreal.PointLight, f"Warehouse_Light_{index + 1}", (x, y, height - 60))
        light.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.point_light_component.set_editor_property("intensity", 4000.0)
        light.point_light_component.set_editor_property("attenuation_radius", 1000.0)
    for definition in layout["items"]:
        item = spawn(item_class, f"Item_{definition['id']}", definition["location_cm"])
        item.set_actor_scale3d(unreal.Vector(ITEM_SCALE, ITEM_SCALE, ITEM_SCALE))
        item.set_editor_property("item_id", unreal.Name(definition["id"]))
        item.set_editor_property("item_name", unreal.Text(definition["name"]))
        item.set_editor_property("description", unreal.Text(definition["description"]))
    if not levels.save_current_level():
        raise RuntimeError("New level could not be saved. Save manually; do not rerun over the existing asset.")
    unreal.log(f"LAST STORAGE: saved {map_path}, 1 room / 10 item definitions. PIE checks are still required.")


if __name__ == "__main__":
    create_warehouse(GAME_MODE_ASSET_PATH)
