# Valwood walking demo

Open [Launch_Valwood_Demo.sh](/home/computer/Desktop/final_engine/Launch_Valwood_Demo.sh). It starts at Bakers arcade in afternoon light, at 1600 × 900 with the tools hidden. Your saved graphics settings are loaded from [graphics.settings](/home/computer/Desktop/final_engine/new_engine/projects/valwood_world/graphics.settings). The launcher uses the existing world launcher and its single-session lock.

## Controls

| Control | Action |
| --- | --- |
| W / A / S / D | Walk forward, left, backward and right |
| Hold right mouse button | Look around; release to use the pointer |
| Hold left Shift | Sprint |
| Space | Jump |
| F9 | Move to the next showcase stop |
| F10 | Open or close graphics and performance settings |
| H | Show or hide tools, including the FPS readout |
| Tab | Open or close the world map |
| Escape | Close graphics/map panels and release mouse look |
| F | Switch between walking and flight preview |
| Q / E in flight | Descend / ascend |

To exit, show tools with **H** and choose **Exit world**, or close the window. Escape releases the pointer; it does not quit. Graphics changes are saved only when you apply them through the graphics panel.

## Eight-stop tour

You begin at stop 1. Each press of **F9** advances one stop; after the reflecting pool it returns to Bakers arcade. Wait for the destination to finish streaming, then explore on foot.

1. **Bakers arcade** — inspect connected shop fronts, passage details and paving.
2. **Finished West High Street** — walk the residential street and examine its edges and material detail.
3. **The traders square** — inspect market stalls, supplies, people and pets.
4. **Copper Lantern interior** — inspect the tavern furnishings, seated residents and indoor lighting.
5. **Outer creek planting** — inspect water, wet banks and vegetation outside the town wall.
6. **New garden and storehouse** — inspect the planted courtyard and purchased environmental assets.
7. **Purchased rainforest undergrowth** — inspect ground plants, tree layers, mist and foliage transitions beneath a darker, overcast canopy.
8. **Rainforest reflecting pool** — inspect the shoreline, cloudy-sky water reflections, humid haze and shaded vegetation.

Walk slowly past walls and plants, then turn the camera to check surface stability. Compare shaded streets with open sunlight and the tavern interior. Listen for changes in paving, timber and soil footsteps, shelter, market activity, creek water and woodland ambience. Audio mutes when the game loses focus.

## Validation evidence

The [jungle atmosphere report](performance/jungle-atmosphere/README.md) records
the current cloudy, misty lighting, water-fallback checks, wildlife loading
change and native frame-time evidence. Startup/destination loading can prepare
a nearby wildlife bank within its existing residency envelope; ordinary
walking retains the original enter/exit hysteresis.

The complete rainforest return walk covered **432.633 m** with zero collision,
slope or sliding blocks, zero return error and zero mid-walk loading frames.
Its [results and full CSV](performance/jungle-atmosphere/walk-preloaded/results.json)
retain all measured intervals: **56.13 FPS average, 44.19 FPS slowest 1%,
32.306 ms worst frame and 42 frames over 22.222 ms**. Route correctness passed;
the strict 45 FPS slowest-1% goal did not.

A later pool run also missed that goal at 36.40 FPS slowest 1%. An isolated
repeat reached 46.11 FPS slowest 1%; this is non-reproduction of those severe
stalls, not a proven fix. Their cause remains unknown. The report preserves
both runs. The final complete walk after the empty-draw cleanup averaged
56.16 FPS, with 44.65 FPS slowest 1%, a 25.735 ms worst frame and 37 isolated
frames over 22.222 ms. It completed all 432.633 m with zero mid-walk loading
or collision blocks, but its strict 45 FPS gate failed. The current build is
not established to pass every 45 FPS gate.

The [earlier finishing report](performance/demo-finish/FINISH_REPORT.md)
records eleven full-suite cases and five fresh-shader-cache repeats that
passed the average/slowest-1% 45 FPS gates. Those sixteen cases describe an
older build and do not substitute for the latest jungle tests. No absolute
minimum-FPS guarantee or commercial AAA visual-parity claim is made.

The implemented audio checks and animation review are documented in the
[audio report](performance/demo-finish/audio/README.md); instance-buffer
validation is described in the [streaming report](performance/demo-finish/instance-streaming.md).
