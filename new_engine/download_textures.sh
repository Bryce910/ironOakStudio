#!/bin/bash
# ─────────────────────────────────────────────────────────────────
# Poly Haven terrain texture downloader
# Run this from the same folder as your game executable.
# Requires: curl
# ─────────────────────────────────────────────────────────────────

BASE="https://dl.polyhaven.org/file/ph-assets/Textures"

# Resolution: 1k = fast download, fine for terrain tiling
# 2k = sharper up close (recommended), 4k = best quality / larger files
RES="2k"

dl() {
    local slug="$1"   # polyhaven asset ID
    local type="$2"   # diff / nor_gl / rough / ao
    local ext="$3"    # jpg or png
    local outfile="$4"

    local url="${BASE}/${slug}/${RES}/${slug}_${RES}_${type}.${ext}"
    echo "  Downloading: $outfile"
    curl -L --silent --show-error -o "$outfile" "$url" || \
        echo "  [WARN] failed: $url"
}

echo "=== Downloading terrain albedo maps ==="
dl "brown_mud_leaves_01"  diff    jpg  terrain_sand.png     # warm sandy/earthy ground
dl "brown_mud_leaves_01"  diff    jpg  terrain_dirt.png     # (reuse; rename below)
dl "aerial_grass_rock_01" diff    jpg  terrain_grass.png
dl "rock_ground_02"       diff    jpg  terrain_rock.png
dl "snow_field_aerial_01" diff    jpg  terrain_snow.png

echo ""
echo "=== Downloading terrain normal maps ==="
dl "brown_mud_leaves_01"  nor_gl  jpg  terrain_sand_nor.png
dl "brown_mud_leaves_01"  nor_gl  jpg  terrain_dirt_nor.png
dl "aerial_grass_rock_01" nor_gl  jpg  terrain_grass_nor.png
dl "rock_ground_02"       nor_gl  jpg  terrain_rock_nor.png
dl "snow_field_aerial_01" nor_gl  jpg  terrain_snow_nor.png

echo ""
echo "=== Downloading roughness maps ==="
dl "brown_mud_leaves_01"  rough   jpg  terrain_sand_rgh.png
dl "brown_mud_leaves_01"  rough   jpg  terrain_dirt_rgh.png
dl "aerial_grass_rock_01" rough   jpg  terrain_grass_rgh.png
dl "rock_ground_02"       rough   jpg  terrain_rock_rgh.png
dl "snow_field_aerial_01" rough   jpg  terrain_snow_rgh.png

echo ""
echo "=== Downloading AO maps ==="
dl "brown_mud_leaves_01"  ao      jpg  terrain_sand_ao.png
dl "brown_mud_leaves_01"  ao      jpg  terrain_dirt_ao.png
dl "aerial_grass_rock_01" ao      jpg  terrain_grass_ao.png
dl "rock_ground_02"       ao      jpg  terrain_rock_ao.png
dl "snow_field_aerial_01" ao      jpg  terrain_snow_ao.png

echo ""
echo "Done. Place all .png files next to your game executable."
