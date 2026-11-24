# Cinematic castle lighting

This pass changes the authored afternoon light, adds steady warm indirect light
inside the Copper Lantern, and replaces the former tiny highlight halo with
filtered scene-linear HDR bloom. It retains the custom C++/OpenGL engine,
existing models/materials, geometry, population and saved graphics settings.

## Lighting behavior

The castle opening uses hour16.65, bearing−18°, sun strength1.12, sky fill1.25,
exposure1.20 and village-grade strength0.22. The lower sun creates longer,
more directional architectural shadows. Reduced fill gives the stone and
wood more shape; some fill was restored after the first native comparison
made covered streets too dark. These remain the existing editable lighting
controls. Moving through a doorway does not change the camera exposure.
The same sun illuminates terrain, models, water, clouds, fog and shadow maps.

The tavern adds a low-frequency single diffuse-return estimate from the actual
hearth28 and hanging-lamp18 mean source intensities, approximate1044m² room
surface area, and mean interior reflectance. Irradiance/PI is approximately
(0.06697318,0.02650728,0.00528736). It is steady, independent of flame flicker
and the camera's nearest-eight torch selection. The term enters the existing
indirect attachment, so ambient occlusion still grounds furniture and actors.

Its world-space volume follows the actual20×16m shell and pitched roof. A
clipping surface80mm between the encoded roof underside and exterior excludes
the exterior roof. A height-dependent wall feather preserves thin upper-gable
inner faces while remaining zero outside. Actual near/mid floor, wall, gable
and ceiling geometry receives full return; exterior roof probes receive zero.

Bloom extracts highlights in exposed linear units (threshold1.25, soft knee0.5),
then returns scene-linear radiance. A normalized13-sample downsample and
separable nine-tap horizontal/vertical blur operate on two340×192RGBA16F images
at the saved1360×765 internal resolution:1,044,480bytes total. The post shader
adds0.035times the filtered highlights before one exposure, filmic tone map,
exact sRGB conversion and the existing LUT. Ordinary surfaces below threshold
retain their contrast. No full-resolution bloom target or per-frame readback
is added; stable-size frames reuse both images.

The bloom and shared post-composite draw are exercised during ordinary loading
before the frame clock. Existing four-direction water/reflection preparation
also remains active. The normal post GPU timer includes extraction, both blur
passes and composition; their running cost is not excluded from measurements.

## Verification

- `bloom-gl-tests.log`: actual GL threshold/knee, chromatic HDR, exposure,
  localized121-pixel impulse spread, normalized energy, NaN/Inf/firefly
  containment, odd dimensions, reuse, feedback rejection, injected allocation
  failure preserving old targets and complete cleanup.
- `post-gl-tests.log`: actual production post shader against independent
  HDR/exposure/filmic/sRGB references, including non-unit exposure, chromatic
  highlights, exact disabled/black bloom equality, LUT, temporal and vignette
  regressions.
- `tavern-return-tests.log`: finite/atomic CPU configuration, actual shader
  room/roof/door bounds,2001-sample smooth crossings,10km coordinate rebasing,
  AO/metal/disabled behavior and actual encoded near/mid interior/exterior
  surface witnesses.
- `foliage-regression.log`: existing production foliage tests remain passing.
- `look-study/`: six native fixed-time captures compare the old light and the
  first lower-sun proposal. The final slightly brighter fill is documented
  above. These images make no FPS claim.
- `captures-packaged/`: final fixed-time native covered-street, tavern and
  exterior captures, with full scene/settings/lighting validation.
- `build-final-packaged.log`: final executable build. Legacy unrelated build
  warnings remain; new lighting modules/fixtures compile with warnings as errors.

The first `captures/` attempt failed initialization because the project runtime
was missing links to the two new bloom shaders. That failed evidence is kept.
The links now match the existing project shader deployment pattern. The helper
checks authored/runtime shader identities before launch and fingerprints both
actual project shaders and authored sources, including the new files.

## Native performance

`tools/check_cinematic_lighting.py --native` runs the complete585.072m walk
first, then covered streets, dusk tavern and the market. It uses native Radeon
RX5300 rendering and the saved Custom/manual85% profile:1600×900 display,
1360×765 internal rendering,2K sun shadow map,half-resolution reflections,
contact shadows,fog and clouds. No quality reduction or preference write is
performed. All measured intervals after the declared120-frame warmup remain
in the mean/1% low gate and raw CSVs. Full model/source hashing and asset
preparation run only while the renderer is idle.

Final results and exact executable/input hashes are in `native/`. A passing
45FPS mean/1% low gate is distinct from a guarantee that every frame exceeds
45FPS; isolated slower frames are reported rather than discarded.

## Open

Run `/home/computer/Desktop/final_engine/Launch_Valwood_Cinematic_Lighting.sh`
to open the covered castle street. WASD walks, right mouse looks, H shows tools,
and F10 opens graphics settings. The existing tavern and castle launchers also
use the updated light when opened in this project.

### Final measured results

| Case | Mean FPS | 1% low FPS | Maximum frame ms |
| --- | ---: | ---: | ---: |
| route | 59.95 | 58.08 | 22.321 |
| hero | 59.95 | 58.46 | 18.858 |
| tavern | 59.95 | 58.36 | 20.291 |
| market | 59.95 | 58.19 | 20.205 |

All four correctness and mean/1% low gates passed on the same final executable. Across 15,240 measured frames, 1 exceeded22.222ms. The route reaches all29waypoints without blocked/sliding/slope steps or loading holds; first creek visibility remains frame392 at16.67708ms. The rawCSV retains the isolated outlier.

All520 model files, all30 scenery files and all7 protected source/project/settings inputs remain unchanged. Native captures and profiles have identical frozen inputs. See `final-verification.json` for hashes and complete evidence.
