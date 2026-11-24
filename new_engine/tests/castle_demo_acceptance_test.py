#!/usr/bin/env python3
"""CPU negative controls for demo route geometry and launcher construction."""
import math
import json
import copy
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_castle_demo import ROOT, HERO_ROUTE_ID, EXPANSION_ROUTE_ID, FARMS_ROUTE_ID, expansion_report, wing_report, banks_report, torches_report, farms_report, torch_shader_report, scene_report, drawbridge_collision_boxes, training_report, route_clearance, validate_native_report, runtime_alias_rules, validate_runtime_record, frontage_report, market_report, pets_report, hero_report, hero_apron_report, collision_boxes, generated_route
from demo_castle_village import command_for
import demo_castle_village as demo


class CastleDemoAcceptanceTest(unittest.TestCase):
    def item(self, alias="purchased_house_a", x=0, z=0, yaw=0, elevation=0):
        return dict(alias=alias, x=x, z=z, yaw=yaw, scale=1., burial=0.,
                    elevation=elevation, label="fixture")

    def model(self, alias="purchased_house_a", low=(-1, 0, -2), high=(1, 4, 2)):
        return {alias: {"bounds_y_up": [low, high]}}

    def test_entire_crossing_rejected_even_with_endpoints_outside(self):
        with self.assertRaisesRegex(ValueError, "obstructed"):
            route_clearance([self.item()], self.model(), [[-4, 0], [4, 0]], {})

    def test_capsule_clearance_not_only_centerline(self):
        with self.assertRaisesRegex(ValueError, "obstructed"):
            route_clearance([self.item()], self.model(), [[1.4, -5], [1.4, 5]], {})
        result = route_clearance([self.item()], self.model(), [[1.8, -5], [1.8, 5]], {})
        self.assertAlmostEqual(result["minimum_capsule_margin_m"], .43)

    def test_rotated_model_uses_rotated_frame(self):
        item = self.item(yaw=math.pi / 2)
        with self.assertRaisesRegex(ValueError, "obstructed"):
            route_clearance([item], self.model(), [[1.5, -5], [1.5, 5]], {})
        result = route_clearance([item], self.model(), [[2.8, -5], [2.8, 5]], {})
        self.assertAlmostEqual(result["minimum_capsule_margin_m"], .43)

    def test_open_gate_does_not_block_its_real_opening(self):
        alias = "starting_gate"
        model = self.model(alias, (-5, 0, -1), (5, 6, 1))
        result = route_clearance([self.item(alias)], model, [[0, -6], [0, 6]], {})
        self.assertAlmostEqual(result["minimum_capsule_margin_m"], 3.50133 / 2 - .37)
        with self.assertRaisesRegex(ValueError, "obstructed"):
            route_clearance([self.item(alias)], model, [[3, -6], [3, 6]], {})

    def test_overhead_ignored_but_low_cargo_is_solid(self):
        items = [self.item(x=10), self.item("purchased_crate", elevation=3)]
        models = {**self.model(), **self.model("purchased_crate", (-1, 0, -1), (1, 1, 1))}
        route_clearance(items, models, [[0, -5], [0, 5]], {})
        items[1]["elevation"] = 1.4
        with self.assertRaisesRegex(ValueError, "obstructed"):
            route_clearance(items, models, [[0, -5], [0, 5]], {})

    def test_tree_uses_runtime_trunk_not_full_canopy(self):
        result = route_clearance([self.item("broadleaf")],
                                 self.model("broadleaf", (-5, 0, -5), (5, 10, 5)),
                                 [[1, -10], [1, 10]], {})
        self.assertAlmostEqual(result["minimum_capsule_margin_m"], .43)

    def test_real_village_collision_boxes_required(self):
        with self.assertRaisesRegex(ValueError, "Missing actual collision"):
            route_clearance([self.item("village_keep")], self.model("village_keep"), [[4, -5], [4, 5]], {})

    def test_actual_village_door_header_not_body_blocks_route(self):
        alias = "village_keep"
        boxes = {alias: [[-4, -1, -1, 1, 0, 6], [1, 4, -1, 1, 0, 6], [-1, 1, -1, 1, 3, 6]]}
        result = route_clearance([self.item(alias)], self.model(alias, (-4, 0, -1), (4, 6, 1)),
                                 [[0, -5], [0, 5]], boxes)
        self.assertAlmostEqual(result["minimum_capsule_margin_m"], .63)

    def test_launcher_uses_normal_interactive_camera_without_exit_timer(self):
        command = command_for("interactive", "alley", {"route": {"frames_with_warmup": 24000}}, Path("/tmp/demo"))
        self.assertEqual(command[command.index("--scenic") + 1], "castle-alley")
        self.assertNotIn("--frames", command)
        self.assertNotIn("--benchmark", command)
        self.assertIn("--no-hud", command)

    def test_guided_launcher_uses_exact_route_and_bounded_duration(self):
        command = command_for("guided", "arrival", {"route": {"frames_with_warmup": 24000}}, Path("/tmp/demo space"))
        self.assertEqual(command.count("--scenic"), 1)
        self.assertEqual(command[command.index("--scenic") + 1], "castle-demo")
        self.assertEqual(command[command.index("--view") + 1], "river")
        self.assertEqual(command[command.index("--frames") + 1], "24000")
        self.assertEqual(command[command.index("--profile") + 1], "/tmp/demo space/profile.json")
        self.assertNotIn("--headless", command)
        self.assertNotIn("--hidden", command)

    def farm_cli_report(self):
        points = generated_route(ROOT, "castle_farms_route")
        length = sum(math.dist(a, b) for a, b in zip(points, points[1:]))
        return {"route": {"frames_with_warmup": 24000, "length_m": 800, "walking_seconds": 800 / 3.5},
                "village_farms": {"route": {"id": FARMS_ROUTE_ID, "length_m": length,
                    "walking_seconds": length / 3.5, "frames_with_warmup": math.ceil(length / 3.5 * 60) + 300}}}

    def test_farm_launcher_uses_generated_route_duration_and_interactive_camera(self):
        report = self.farm_cli_report()
        for mode in ("interactive", "guided", "benchmark"):
            with self.subTest(mode=mode):
                command = command_for(mode, "farms", report, Path("/tmp/farm demo"), route_name="farms")
                self.assertEqual(command[command.index("--scenic") + 1], "castle-farms")
                self.assertIn("--no-hud", command)
                self.assertNotIn("--hidden", command)
                self.assertNotIn("--headless", command)
                if mode == "interactive":
                    self.assertNotIn("--frames", command)
                    self.assertNotIn("--benchmark", command)
                else:
                    self.assertEqual(command[command.index("--frames") + 1], str(report["village_farms"]["route"]["frames_with_warmup"]))
                    self.assertEqual(command[command.index("--view") + 1], "river")
                    self.assertIn("--benchmark", command)
        launcher = (ROOT.parent / "Launch_Valwood_Farms_Demo.sh").read_text()
        self.assertIn('--route farms --stop farms "$@"', launcher)

    def test_farm_launcher_cli_allows_matching_injected_stop_and_rejects_wrong_stop(self):
        report = self.farm_cli_report()
        for mode in ("guided", "benchmark"):
            output = io.StringIO()
            with self.subTest(mode=mode), patch.object(sys, "argv", ["demo", "--route", "farms", "--stop", "farms", "--mode", mode, "--dry-run"]), \
                    patch.object(demo, "scene_report", return_value=report), patch.object(demo.subprocess, "run") as launch, contextlib.redirect_stdout(output):
                demo.main()
                launch.assert_not_called()
                self.assertIn("--scenic castle-farms", output.getvalue())
                self.assertIn("--frames 3318", output.getvalue())
                self.assertIn("Route: 176.0m", output.getvalue())
        with patch.object(sys, "argv", ["demo", "--route", "district", "--stop", "farms", "--mode", "guided"]), \
                patch.object(demo, "scene_report") as report_call, contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
            demo.main()
        self.assertEqual(error.exception.code, 2)
        report_call.assert_not_called()

    def test_farm_launcher_validates_actual_farm_route_and_mode_budget(self):
        report = self.farm_cli_report()
        for mode in ("guided", "benchmark"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary);(root / "rpg").touch();output = root / "report"
                def capture_only(command, **kwargs):
                    self.assertEqual(command[command.index("--scenic") + 1], "castle-farms")
                    self.assertEqual(command[command.index("--frames") + 1], "3318")
                    (output / "final.bmp.json").write_text(json.dumps({"capture_ok": True, "offscreen": False, "width": 1600, "height": 900}))
                with patch.object(sys, "argv", ["demo", "--route", "farms", "--stop", "farms", "--mode", mode, "--output", str(output)]), \
                        patch.object(demo, "ROOT", root), patch.object(demo, "scene_report", return_value=copy.deepcopy(report)), \
                        patch.object(demo.subprocess, "run", side_effect=capture_only) as launch, \
                        patch.object(demo, "validate_native_report", return_value={"native_route_verified": True}) as validate, contextlib.redirect_stdout(io.StringIO()):
                    demo.main()
                self.assertEqual(launch.call_count, 1)
                validate.assert_called_once()
                self.assertEqual(validate.call_args.kwargs["expected_route"], FARMS_ROUTE_ID)
                self.assertEqual(validate.call_args.kwargs["require_budget"], mode == "benchmark")
                self.assertTrue(json.loads((output / "acceptance.json").read_text())["native_route_verified"])

    def native_fixture(self):
        from check_castle_demo import ROUTE_ID
        scene = {"total_authored_instances": 4500, "source_files": {"one.txt": 2500, "two.txt": 2000},
                 "route": {"length_m": 800}}
        profile = dict(renderer="AMD Radeon RX 5300", samples=14000, frames=14120, dropped_samples=0,
                       mean_fps=59.9, one_percent_low_fps=58.5, p99_ms=17.1, max_ms=20.1,
                       within_45fps_budget_percent=100.)
        world = dict(gl_error=0, failed_tiles=0, travel_loading_frames=0, remaining_texture_entries=0,
                     shadow_map_side=3072, soft_shadow_taps=9,
                     remaining_texture_references=0, authored_instances=4500, authored_source_files=2,
                     authored_sources=[dict(path="/scene/one.txt", instances=2500), dict(path="/scene/two.txt", instances=2000)],
                     benchmark_route=dict(active=True, id=ROUTE_ID, complete=True, length_m=800., return_error_m=0.,
                                          collision_blocked_steps=0, slope_blocked_steps=0, sliding_steps=0, actual_distance_m=800.),
                     village_population=dict(residents=128, knights=16, archers=16, traders=20, travelers=76, pose_assets=76, walking=10),
                     village_pets=dict(pets=12, cats=6, dogs=6, pose_assets=32, pose_batch_capacity=38,
                                       gpu_pose_blending=True, walking=6, idling=6, peak_visible=12),
                     village_animation=dict(gpu_pose_blending=True, pose_batch_capacity=91), texture_cache_unmanaged_uploads=0,
                     village_surfaces=dict(directional_sky=True, ground_history=True, stochastic_paving=True, terrain_sampler_count=16),
                     castle_practicals=dict(enabled=True, count=4, radius_m=20, emitter_y_m=26.895,
                                            masked_to_receiving_hall=True, shadow_maps=False))
        return scene, profile, world

    def run_native_fixture(self, scene, profile, world, **options):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "profile.json"
            path.write_text(json.dumps(profile))
            Path(str(path) + ".world.json").write_text(json.dumps(world))
            # Integrated lighting has its own actual-GL regression. Isolate
            # this tool's new route/count/timing checks from that shared helper.
            with patch("benchmark_cinematic_scenery.verify_integrated_lighting") as lighting:
                result = validate_native_report(scene, path, **options)
                lighting.assert_called_once()
                return result

    def test_native_counts_are_derived_from_all_sources(self):
        scene, profile, world = self.native_fixture()
        result = self.run_native_fixture(scene, profile, world)
        self.assertTrue(result["native_route_verified"] and result["performance_verified"])
        world["authored_sources"][1]["instances"] -= 1
        with self.assertRaisesRegex(ValueError, "source counts"):
            self.run_native_fixture(scene, profile, world)

    def test_incomplete_or_blocked_route_cannot_pass(self):
        for key, value in [("complete", False), ("collision_blocked_steps", 1), ("slope_blocked_steps", 1),
                           ("sliding_steps", 1), ("return_error_m", .1), ("actual_distance_m", 799.)]:
            with self.subTest(key=key):
                scene, profile, world = self.native_fixture()
                world["benchmark_route"][key] = value
                with self.assertRaises(ValueError):
                    self.run_native_fixture(scene, profile, world)

    def test_slow_run_is_reported_without_being_claimed_verified(self):
        scene, profile, world = self.native_fixture()
        profile["one_percent_low_fps"] = 41.
        profile["p99_ms"] = 24.4
        with self.assertRaisesRegex(ValueError, "timing target failed"):
            self.run_native_fixture(scene, profile, world)
        result = self.run_native_fixture(scene, profile, world, require_budget=False)
        self.assertTrue(result["native_route_verified"])
        self.assertFalse(result["performance_verified"])

    def test_software_and_cache_overflow_reports_reject(self):
        scene, profile, world = self.native_fixture()
        profile["renderer"] = "llvmpipe"
        with self.assertRaisesRegex(ValueError, "Native GPU"):
            self.run_native_fixture(scene, profile, world)
        profile["renderer"] = "AMD Radeon RX 5300"
        world["texture_cache_unmanaged_uploads"] = 1
        with self.assertRaisesRegex(ValueError, "cache overflow"):
            self.run_native_fixture(scene, profile, world)

    def test_native_cannot_disable_the_new_lighting_or_material_path(self):
        for group, key in [("village_surfaces", "directional_sky"), ("village_surfaces", "ground_history"),
                           ("village_surfaces", "stochastic_paving"), ("castle_practicals", "enabled")]:
            with self.subTest(group=group, key=key):
                scene, profile, world = self.native_fixture()
                world[group][key] = False
                with self.assertRaises(ValueError):
                    self.run_native_fixture(scene, profile, world)

    def test_native_requires_complete_larger_crowd_and_animated_pets(self):
        for group, key, value in [("village_population", "residents", 64), ("village_population", "traders", 10),
                                  ("village_pets", "cats", 0), ("village_pets", "dogs", 5),
                                  ("village_pets", "gpu_pose_blending", False), ("village_pets", "pose_assets", 31),
                                  ("village_pets", "walking", 0), ("village_pets", "peak_visible", 0)]:
            with self.subTest(group=group, key=key):
                scene, profile, world = self.native_fixture()
                world[group][key] = value
                with self.assertRaises(ValueError):
                    self.run_native_fixture(scene, profile, world)

    def test_cpu_report_rejects_real_runtime_floor_and_scale_limits(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "aliases.cpp"
            path.write_text('{"starting_table", "starting_table", false, .3f, 220, 0, 2, Grounding::Foundation, Collision::Box, 0, .05f},\n'
                            '{"purchased_house_a", "house", false, .1f, 900, 0, 2, Grounding::Foundation, Collision::Box},\n')
            rules = runtime_alias_rules(path)
            validate_runtime_record("starting_table", [.5, .5, 0, 1, 0, .017], rules, "hall table")
            for alias, values in [("starting_table", [.5, .5, 0, 1, 0, .051]),
                                  ("purchased_house_a", [.5, .5, 0, 1, 0, .001]),
                                  ("starting_table", [.5, .5, 0, .149, 0, 0])]:
                with self.assertRaisesRegex(ValueError, "Runtime placement bounds"):
                    validate_runtime_record(alias, values, rules, "invalid fixture")

    def test_final_frontage_recipe_and_actual_house_ownership(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        placements = {item["label"]: item for item in manifest["placements"]}
        def check(metadata):
            return frontage_report(metadata, placements, manifest["demo_districts"], manifest["demo_dressing"]["labels"])
        metadata = manifest["demo_frontages"]
        self.assertEqual(check(metadata)["finished_homes"], 42)
        stale = copy.deepcopy(metadata)
        stale["recipe_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "Frontage recipe"):
            check(stale)
        stale = copy.deepcopy(metadata)
        stale["homes"][0]["labels"][0] = stale["homes"][1]["labels"][0]
        with self.assertRaisesRegex(ValueError, "frontage owner"):
            check(stale)
        stale = copy.deepcopy(metadata)
        stale["preserved_dressing_labels"].pop()
        with self.assertRaisesRegex(ValueError, "preserved furnishing"):
            check(stale)

    def test_market_authorship_source_hash_and_supported_goods(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        placements = {item["label"]: item for item in manifest["placements"]}
        def check(metadata):
            return market_report(metadata, placements, manifest["models"])
        metadata = manifest["demo_market_lively"]
        self.assertEqual(check(metadata)["supported_goods"], 86)
        for change in ("recipe", "support", "ownership", "hash"):
            with self.subTest(change=change):
                bad = copy.deepcopy(metadata)
                if change == "recipe": bad["recipe_sha256"] = "0" * 64
                elif change == "support": bad["surface_supports"][0]["actual_surface_samples_m"][0] += .1
                elif change == "ownership": bad["groups"][0]["labels"][0] = bad["groups"][1]["labels"][0]
                else: bad["source_model_sha256"][next(iter(bad["source_model_sha256"]))] = "0" * 64
                with self.assertRaises(ValueError): check(bad)

    def test_pet_asset_inventory_provenance_and_real_model_hashes(self):
        metadata = json.loads((ROOT / "projects/valwood_world/models/village_pets_manifest.json").read_text())
        self.assertTrue(pets_report(metadata, hash_models=True)["model_hashes_checked"])
        for change in ("species", "provenance", "pose", "texture", "hash"):
            with self.subTest(change=change):
                bad = copy.deepcopy(metadata)
                if change == "species": bad["cats"] = 5
                elif change == "provenance": bad["provenance_sha256"] = "0" * 64
                elif change == "pose": bad["poses"][0]["file"] = bad["poses"][1]["file"]
                elif change == "texture": bad["budget"]["estimated_texture_rgba_mips_mib"] = 64
                else: bad["poses"][0]["sha256"] = "0" * 64
                with self.assertRaises(ValueError): pets_report(bad, hash_models=True)

    def test_hero_street_ownership_route_and_real_source_canopies(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        places = {r["label"]: r for r in manifest["placements"]}
        metadata, grove = manifest["demo_market_hero"], manifest["demo_hero_grove"]
        pieces = collision_boxes(ROOT / "engine/world_village_colliders.inc")
        if "castle_drawbridge" in manifest["models"]:
            pieces["castle_drawbridge"] = drawbridge_collision_boxes(ROOT / "engine/world_castle_drawbridge_colliders.inc")
        def check(m, p=places, g=grove):
            return hero_report(m, g, p, manifest["models"], pieces, roads=manifest["roads"], squares=manifest["squares"])
        self.assertEqual(check(metadata)["route"]["length_m"], 88)
        bad = copy.deepcopy(metadata);bad["recipe_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "recipe"): check(bad)
        bad = copy.deepcopy(metadata);bad["walking_route_xz"][1][0] += 1
        with self.assertRaisesRegex(ValueError, "route"): check(bad)
        bad = copy.deepcopy(metadata);bad["surface_supports"][0]["actual_surface_samples_m"][0] += .1
        with self.assertRaisesRegex(ValueError, "support"): check(bad)
        bad_places = copy.deepcopy(places);bad_places[metadata["source_envelopes"][0]["label"]]["scale"] = .5
        with self.assertRaisesRegex(ValueError, "Full-size"): check(metadata, bad_places)
        bad_grove = copy.deepcopy(grove);bad_grove["global_density_changed"] = True
        with self.assertRaisesRegex(ValueError, "grove budget"): check(metadata, g=bad_grove)

    def test_hero_measured_aprons_and_actual_runtime_paving(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        places = {row["label"]: row for row in manifest["placements"]}
        metadata, roads, squares = manifest["demo_market_hero"], manifest["roads"], manifest["squares"]
        layout = ROOT / "engine/world_village_layout.inc"
        def check(m=metadata, s=squares, r=roads, path=layout):
            return hero_apron_report(m, places, manifest["models"], r, s, path)
        report = check()
        self.assertEqual(report["measured_aprons"], 2)
        self.assertGreater(report["minimum_road_overlap_m"], .40)
        self.assertEqual(report["runtime_paving_squares_verified"], len(squares))
        missing = copy.deepcopy(metadata);missing["ground_aprons"].pop()
        with self.assertRaisesRegex(ValueError, "aprons are missing"): check(m=missing)
        missing_square = [row for row in squares if row["id"] != metadata["ground_aprons"][0]["id"]]
        with self.assertRaisesRegex(ValueError, "squares are missing"): check(s=missing_square)
        short = copy.deepcopy(squares);next(row for row in short if row["id"] == metadata["ground_aprons"][0]["id"])["half"][1] -= .15
        with self.assertRaisesRegex(ValueError, "does not cover measured apron"): check(s=short)
        wrong_parent = copy.deepcopy(metadata);wrong_parent["ground_aprons"][0]["parent"] = wrong_parent["ground_aprons"][1]["parent"]
        with self.assertRaisesRegex(ValueError, "parent differs"): check(m=wrong_parent)
        false_source = copy.deepcopy(metadata);false_source["ground_aprons"][0]["source_world_bounds_xz"][0] += .1
        with self.assertRaisesRegex(ValueError, "source bounds differ"): check(m=false_source)
        wrong_margin = copy.deepcopy(metadata);wrong_margin["ground_aprons"][0]["street_extension_m"] = .5
        with self.assertRaisesRegex(ValueError, "margins differ"): check(m=wrong_margin)
        wrong_material = copy.deepcopy(squares);next(row for row in wrong_material if row["id"] == metadata["ground_aprons"][0]["id"])["paving"] = 0
        with self.assertRaisesRegex(ValueError, "paving material differs"): check(s=wrong_material)
        disconnected = copy.deepcopy(roads);next(row for row in disconnected if row["id"] == "cross_55")["half_width"] -= .2
        with self.assertRaisesRegex(ValueError, "fails to overlap existing road"): check(r=disconnected)
        # Keep the effective manifest correct while independently shrinking
        # only the generated runtime square. A stale header must be rejected.
        stale = copy.deepcopy(squares);next(row for row in stale if row["id"] == metadata["ground_aprons"][0]["id"])["half"][1] -= .15
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "layout.inc"
            rows = ["    {" + ",".join(format(v, ".12g") for v in [*row["center"], *row["half"], 1, 0, row["paving"], 1]) + "}," for row in stale]
            path.write_text("static const VillageRect village_rects[]={\n" + "\n".join(rows) + "\n};\n")
            with self.assertRaisesRegex(ValueError, "Generated runtime paving square differs"): check(path=path)

    def test_hero_native_route_and_detailed_shadows_cannot_be_substituted(self):
        scene, profile, world = self.native_fixture()
        scene["hero_market"] = {"route": {"length_m": 88}}
        world["benchmark_route"].update(id=HERO_ROUTE_ID, length_m=88, actual_distance_m=88)
        self.assertTrue(self.run_native_fixture(scene, profile, world, expected_route=HERO_ROUTE_ID)["native_route_verified"])
        with self.assertRaisesRegex(ValueError, "route"):
            self.run_native_fixture(scene, profile, world)
        world["shadow_map_side"] = 2048
        with self.assertRaisesRegex(ValueError, "shadow quality"):
            self.run_native_fixture(scene, profile, world, expected_route=HERO_ROUTE_ID)

    def test_guided_hero_launch_uses_the_local_route(self):
        report = {"route": {"frames_with_warmup": 17640}, "hero_market": {"route": {"frames_with_warmup": 1809}}}
        command = command_for("guided", "arrival", report, Path("/tmp/hero"), route_name="hero")
        self.assertEqual(command[command.index("--scenic") + 1], "castle-hero-entry")
        self.assertEqual(command[command.index("--frames") + 1], "1809")


    def test_drawbridge_uses_real_rail_boxes_and_open_deck(self):
        alias = "castle_drawbridge"
        item = self.item(alias)
        item["burial"] = .7
        model = self.model(alias, (-7.4, 0, -12.5), (7.4, 5.15, 0))
        pieces = {alias: [[-7.5, -6.65, -12.5, -.35, .4, 5.35], [6.65, 7.5, -12.5, -.35, .4, 5.35]]}
        proof = route_clearance([item], model, [[0, -15], [0, 3]], pieces)
        self.assertAlmostEqual(proof["minimum_capsule_margin_m"], 6.28)
        with self.assertRaisesRegex(ValueError, "Missing actual collision"):
            route_clearance([item], model, [[0, -15], [0, 3]], {})
        with self.assertRaisesRegex(ValueError, "obstructed"):
            route_clearance([item], model, [[7, -15], [7, 3]], pieces)

    def test_new_castle_aliases_are_accepted_by_actual_runtime_contract(self):
        rules = runtime_alias_rules(ROOT / "engine/world_scenery.cpp")
        for alias in ("castle_monument", "castle_training_dummy", "castle_archery_target", "castle_weapon_rack",
                      "castle_yard_bench", "castle_training_fence", "castle_drawbridge"):
            validate_runtime_record(alias, [.487, .32, 0, 1, .7 if alias == "castle_drawbridge" else 0, 0], rules, alias)
        with self.assertRaisesRegex(ValueError, "Runtime placement bounds"):
            validate_runtime_record("castle_drawbridge", [.487, .32, 0, 1, .751, 0], rules, "sunken bridge")

    def test_expansion_authorship_sources_paving_bridge_and_prefix(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        places = {row["label"]: row for row in manifest["placements"]}
        pieces = collision_boxes(ROOT / "engine/world_village_colliders.inc")
        pieces["castle_drawbridge"] = drawbridge_collision_boxes(ROOT / "engine/world_castle_drawbridge_colliders.inc")
        metadata, creek = manifest["castle_expansion"], manifest["castle_creek"]
        def check(m=metadata, c=creek, p=places, models=manifest["models"], squares=manifest["squares"], views=manifest["demo_views"]):
            return expansion_report(m, c, p, models, pieces, roads=manifest["roads"], squares=squares, views=views)
        proof = check()
        self.assertEqual(proof["instances"], 63)
        self.assertEqual(proof["training"]["pose_assets"], 28)
        self.assertAlmostEqual(proof["route"]["length_m"], 848.9417986638169)
        for change in ("recipe", "prefix", "labels", "model", "square", "view", "creek", "interior_water", "bridge", "route"):
            with self.subTest(change=change):
                m, c, p, models, squares, views = (copy.deepcopy(value) for value in
                    (metadata, creek, places, manifest["models"], manifest["squares"], manifest["demo_views"]))
                if change == "recipe": m["recipe_sha256"] = "0" * 64
                elif change == "prefix": p[next(iter(p))]["x"] += .1
                elif change == "labels": m["labels"].pop()
                elif change == "model": models["castle_monument"]["sha256"] = "0" * 64
                elif change == "square": next(row for row in squares if row["id"] == "castle_training_court")["paving"] = 0
                elif change == "view": views = [row for row in views if row["id"] != "castle-statue"]
                elif change == "creek": c["terrain_helper_sha256"] = "0" * 64
                elif change == "interior_water": c["outside_outer_town_walls"] = False
                elif change == "bridge": p["castle_creek_drawbridge"]["burial"] = 0
                elif change == "route": m["route_xz"][1][0] = 10
                with self.assertRaises(ValueError): check(m, c, p, models, squares, views)

    def test_training_requires_actual_full_pose_inventory(self):
        proof = training_report()
        self.assertEqual((proof["actors"], proof["trainees"], proof["instructors"], proof["blend_batches"]), (7, 6, 1, 28))
        source = json.loads((ROOT / "projects/valwood_world/models/castle_training_manifest.json").read_text())
        for field, value in (("pose_count", 23), ("new_image_texture_policies", 1), ("source_sha256", "0" * 64)):
            bad = copy.deepcopy(source);bad[field] = value
            with patch("check_castle_demo.load_json", return_value=bad), self.assertRaises(ValueError):
                training_report()

    def test_wing_canonical_prefix_sources_roads_and_real_supply_contacts(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        places = {r["label"]: r for r in manifest["placements"]}
        pieces = collision_boxes(ROOT / "engine/world_village_colliders.inc")
        pieces["castle_drawbridge"] = drawbridge_collision_boxes(ROOT / "engine/world_castle_drawbridge_colliders.inc")
        metadata = manifest["castle_inner_wing"]
        def check(m=metadata, p=places, models=manifest["models"], roads=manifest["roads"]):
            return wing_report(m, p, models, pieces, roads=roads)
        proof = check()
        self.assertEqual((proof["instances"], proof["full_scale_structures"], proof["real_source_support_rays"]), (16, 2, 10))
        self.assertTrue(proof["canonical_prefix_hash_checked"])
        for change in ("missing", "removed", "prefix", "recipe", "model", "road", "floating", "forged_contact"):
            with self.subTest(change=change):
                m, p, models, roads = (copy.deepcopy(v) for v in (metadata, places, manifest["models"], manifest["roads"]))
                if change == "missing": m = None
                elif change == "removed": del p["castle_inner_barracks"]
                elif change == "prefix": p[next(iter(p))]["z"] += .1
                elif change == "recipe": m["recipe_sha256"] = "0" * 64
                elif change == "model": m["source_model_sha256"]["castle_barracks"] = "0" * 64
                elif change == "road": next(r for r in roads if r["id"] == "castle_barracks_entry")["paving"] = 0
                elif change == "floating": p["castle_inner_barracks_supported_tankard"]["elevation"] += .4
                elif change == "forged_contact":
                    m["surface_supports"][0]["actual_surface_samples_m"] = [1.] * 5
                    m["surface_supports"][0]["ground_elevation_m"] = .882
                    p["castle_inner_barracks_supported_tankard"]["elevation"] = .882
                with self.assertRaises(ValueError): check(m, p, models, roads)

    def test_banks_reproduce_canonical_grounding_and_resident_inventory(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        places = {r["label"]: r for r in manifest["placements"]}
        metadata = manifest["demo_castle_creek_banks"]
        def check(m=metadata, p=places, models=manifest["models"]):
            return banks_report(m, p, models, roads=manifest["roads"])
        proof = check()
        self.assertEqual((proof["instances"], proof["rocks"], proof["ferns"], proof["deadwood"]), (36, 24, 10, 2))
        self.assertTrue(proof["canonical_transforms_independently_reproduced"])
        self.assertGreater(proof["actual_source_vertex_terrain_rays"], 90000)
        for change in ("missing", "removed", "prefix", "recipe", "helper", "model", "density", "floating", "forged_support"):
            with self.subTest(change=change):
                m, p, models = (copy.deepcopy(v) for v in (metadata, places, manifest["models"]))
                if change == "missing": m = None
                elif change == "removed": del p[m["labels"][-1]]
                elif change == "prefix": p["castle_inner_barracks"]["x"] += .1
                elif change == "recipe": m["recipe_sha256"] = "0" * 64
                elif change == "helper": m["terrain_helper_sha256"] = "0" * 64
                elif change == "model": m["source_models"]["rock0"]["sha256"] = "0" * 64
                elif change == "density": m["global_density_changed"] = True
                elif change == "floating":
                    p[m["labels"][0]]["elevation"] = .5
                    m["placements"][0]["elevation"] = .5
                elif change == "forged_support": m["supports"][0]["runtime_base_y"] += .5
                with self.assertRaises(ValueError): check(m, p, models)

    def test_configured_extra_castle_layers_cannot_silently_disappear(self):
        import check_castle_demo
        load = check_castle_demo.load_json
        manifest_path = ROOT / "projects/valwood_world/scenery/castle_village_manifest.json"
        for field in ("castle_expansion", "castle_inner_wing", "demo_castle_creek_banks", "village_torches", "village_farms", "village_reinforcements"):
            missing = load(manifest_path);missing.pop(field)
            def altered(path, missing=missing):
                return missing if Path(path) == manifest_path else load(path)
            with self.subTest(layer=field), patch("check_castle_demo.load_json", side_effect=altered):
                with self.assertRaisesRegex(ValueError, "Configured castle layer metadata is missing"):
                    scene_report()

    def test_native_expansion_requires_full_smooth_training_group(self):
        scene, profile, world = self.native_fixture()
        scene["castle_expansion"] = {"route": {"length_m": 338.9417986638169}}
        world["castle_training"] = dict(actors=7, trainees=6, instructors=1, pose_assets=28,
                                          blend_batches=28, gpu_pose_blending=True, updates=1000)
        self.assertTrue(self.run_native_fixture(scene, profile, world)["performance_verified"])
        for key, value in (("actors", 0), ("trainees", 5), ("instructors", 0), ("pose_assets", 24),
                           ("blend_batches", 24), ("gpu_pose_blending", False), ("updates", 0)):
            with self.subTest(key=key):
                bad = copy.deepcopy(world);bad["castle_training"][key] = value
                with self.assertRaisesRegex(ValueError, "castle training"):
                    self.run_native_fixture(scene, profile, bad)
        world["benchmark_route"].update(id=EXPANSION_ROUTE_ID, length_m=338.9417986638169, actual_distance_m=338.9417986638169)
        self.assertTrue(self.run_native_fixture(scene, profile, world, expected_route=EXPANSION_ROUTE_ID)["native_route_verified"])
        with self.assertRaisesRegex(ValueError, "route"):
            self.run_native_fixture(scene, profile, world)

    def test_torches_preserve_prefix_and_reproduce_actual_mounts_and_emitters(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        places = {r["label"]: r for r in manifest["placements"]}
        metadata = manifest["village_torches"]
        def check(m=metadata, p=places, inventory=manifest["purchased_torches"]):
            return torches_report(m, inventory, p, manifest["models"], roads=manifest["roads"], demo_route=manifest["demo_route_xz"])
        proof = check()
        self.assertEqual((proof["instances"], proof["actual_wall_contact_samples"], proof["generated_emitters_verified"]), (22, 168, 22))
        self.assertTrue(proof["canonical_prefix_hash_checked"] and proof["actual_wall_contacts_reproduced"])
        for change in ("missing", "removed", "prefix", "recipe", "inventory", "cap", "fuel", "plane", "include"):
            with self.subTest(change=change):
                m, p, inventory = (copy.deepcopy(v) for v in (metadata, places, manifest["purchased_torches"]))
                if change == "missing": m = None
                elif change == "removed": del p[m["labels"][-1]]
                elif change == "prefix": p["castle_inner_barracks"]["x"] += .1
                elif change == "recipe": m["recipe_sha256"] = "0" * 64
                elif change == "inventory": inventory["shared_atlas_images"] = 5
                elif change == "cap": m["maximum_near_lights"] = 9
                elif change == "fuel": m["emitters"][0]["position_local_xyz"][1] += .05
                elif change == "plane": m["emitters"][-1]["clip_bounds"] = [-280, -280, 280, 280, 0, 1000]
                elif change == "include": m["generated_include_sha256"] = "0" * 64
                with self.assertRaises(ValueError): check(m, p, inventory)

    def test_native_requires_complete_bounded_torches_and_visible_flames(self):
        scene, profile, world = self.native_fixture()
        scene["village_torches"] = {"instances": 22}
        world["village_torches"] = dict(models=22, emitters=22, light_cap=8, peak_lights=8,
            peak_visible_flames=12, flame_draws=100, main_frames=profile["frames"], reflection_frames=600,
            buffer_bytes=256, failures=0, shared_smooth_flicker=True, wall_plane_and_hall_clip=True, point_shadow_maps=False)
        self.assertTrue(self.run_native_fixture(scene, profile, world)["native_route_verified"])
        for key, value in (("models", 21), ("emitters", 21), ("light_cap", 9), ("peak_lights", 9),
                           ("peak_visible_flames", 0), ("flame_draws", 0), ("main_frames", profile["frames"] - 1),
                           ("failures", 1), ("shared_smooth_flicker", False), ("wall_plane_and_hall_clip", False),
                           ("point_shadow_maps", True)):
            with self.subTest(key=key):
                bad = copy.deepcopy(world);bad["village_torches"][key] = value
                with self.assertRaisesRegex(ValueError, "torch"):
                    self.run_native_fixture(scene, profile, bad)

    def test_torch_shader_runtime_package_rejects_missing_and_stale_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "shaders";source.mkdir()
            runtime = root / "projects/valwood_world/shaders";runtime.mkdir(parents=True)
            for stage in ("vs", "fs"):
                name = f"world_torch_flame.{stage}.glsl"
                (source / name).write_text(f"// {stage} source\n")
                (runtime / name).symlink_to(source / name)
            self.assertEqual(len(torch_shader_report(root)), 2)
            path = runtime / "world_torch_flame.fs.glsl"
            path.unlink()
            with self.assertRaisesRegex(ValueError, "runtime shader.*missing/stale"):
                torch_shader_report(root)
            path.write_text("// stale packaged source\n")
            with self.assertRaisesRegex(ValueError, "runtime shader.*missing/stale"):
                torch_shader_report(root)

    def test_farm_actual_sources_rows_paths_support_and_preserved_town(self):
        manifest = json.loads((ROOT / "projects/valwood_world/scenery/castle_village_manifest.json").read_text())
        places = {row["label"]: row for row in manifest["placements"]}
        pieces = collision_boxes(ROOT / "engine/world_village_colliders.inc")
        pieces["castle_drawbridge"] = drawbridge_collision_boxes(ROOT / "engine/world_castle_drawbridge_colliders.inc")
        metadata = manifest["village_farms"]
        def check(m=metadata, p=places, models=manifest["models"], roads=manifest["roads"]):
            return farms_report(m, p, models, pieces, roads=roads, demo_route=manifest["demo_route_xz"], views=manifest["demo_views"])
        proof = check()
        self.assertEqual((proof["instances"], proof["corn_clusters"], proof["wheat_clusters"], proof["source_support_rays"]), (181, 60, 60, 25))
        self.assertEqual(proof["route"]["length_m"], 176)
        self.assertTrue(proof["canonical_prefix_hash_checked"] and proof["soil_and_rows_generated_match"])
        for change in ("missing", "removed", "prefix", "recipe", "bounds", "road", "phase", "floating", "walk"):
            with self.subTest(change=change):
                m, p, models, roads = (copy.deepcopy(v) for v in (metadata, places, manifest["models"], manifest["roads"]))
                if change == "missing": m = None
                elif change == "removed": del p[m["labels"][-1]]
                elif change == "prefix": p["village_torch_outer_west"]["x"] += .1
                elif change == "recipe": m["recipe_sha256"] = "0" * 64
                elif change == "bounds": models["village_crop_wheat"]["bounds_y_up"][1][0] += .1
                elif change == "road": next(r for r in roads if r["id"] == "farm_approach")["half_width"] = .1
                elif change == "phase": m["cultivated_ground"][1]["row_phase_x_m"] = .25
                elif change == "floating": p["village_farm_corn_cart_harvest_sack"]["elevation"] += .1
                elif change == "walk": m["walking_route_xz"][1][0] += 1
                with self.assertRaises(ValueError): check(m, p, models, roads)

    def test_native_reinforcements_require_complete_rooted_watch_without_forcing_visibility(self):
        scene, profile, world = self.native_fixture()
        scene['village_reinforcements'] = dict(animated_watch_guards=4)
        world['village_watch'] = dict(actors=4, visible=0, peak_visible=0, pose_assets=8, blend_batches=8,
                                      gpu_pose_blending=True, updates=profile['frames'], rooted_stations=True)
        self.assertTrue(self.run_native_fixture(scene, profile, world)['native_route_verified'])
        for key, value in [('actors',3),('pose_assets',7),('blend_batches',4),('gpu_pose_blending',False),
                           ('updates',profile['frames']-1),('rooted_stations',False),('visible',5),('peak_visible',5)]:
            with self.subTest(field=key):
                bad=copy.deepcopy(world);bad['village_watch'][key]=value
                with self.assertRaisesRegex(ValueError,'rooted animated village watch'):self.run_native_fixture(scene, profile, bad)
        bad=copy.deepcopy(world);del bad['village_watch']
        with self.assertRaisesRegex(ValueError,'rooted animated village watch'):self.run_native_fixture(scene, profile, bad)

    def test_native_farm_route_does_not_require_offscreen_pets(self):
        scene, profile, world = self.native_fixture()
        scene["village_farms"] = {"route": {"length_m": 176}}
        world["benchmark_route"].update(id=FARMS_ROUTE_ID, length_m=176, actual_distance_m=176)
        world["village_pets"]["peak_visible"] = 0
        self.assertTrue(self.run_native_fixture(scene, profile, world, expected_route=FARMS_ROUTE_ID)["native_route_verified"])
        world["village_pets"]["pose_assets"] = 0
        with self.assertRaisesRegex(ValueError, "cats and dogs"):
            self.run_native_fixture(scene, profile, world, expected_route=FARMS_ROUTE_ID)


if __name__ == "__main__":
    unittest.main(verbosity=2)
