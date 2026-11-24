Asset browser UI review — implementation, awaiting integrated runtime acceptance

Owned source: build/asset-browser-2026-11-06/source-v1/engine/world_studio_designer_assets.inc

Implemented behavior:
- Search supports the catalog's all-word match on human labels, collection, category, source, alias, and tags.
- Category and collection filters combine with Favorites and Recent; Reset filters clears all restrictions.
- Sort by readable name with natural numeric ordering, category/subcategory, or original asset ID.
- Selection uses the original alias and changes no document data. Card ImGui IDs use that alias.
- Recent is the bounded existing recently-selected list, ordered most recently selected first. Existing workspace persistence for favorites/recent remains unchanged; new collection/sort/recent-only filter choices are session local.
- Visible cards and selected details request bounded actual model thumbnails. Icons with truthful thumbnail status remain until the real preview becomes available.
- Grid cards adapt to available height. Below170px available browser height or650px width, a single compact Search / Filters / Place-or-Cancel / count toolbar preserves card space; every filter, view choice, reset and preview refresh remains accessible in the Filters popup. Very short browsers use46px list rows without a footer.
- A wide, tall-enough browser has a selected-model detail pane with a larger real preview, source collection, category, and known dimensions.
- Place in scene always arms a click-to-place preview. It cannot commit immediately at a stale previous ground ray. Escape cancels; selecting a different asset cancels the prior armed asset.
- Drag-to-ground placement retains the original payload, exact bounds preview, placement grounding, and Undo path. Double-click opens the existing full asset inspector.
- Character scenery is labeled as scenery; the selected character offers the existing NPC / Navigation tool.

Root runtime checks to perform:
1. In the normal 224px existing browser, type a multi-word query without losing focus. Review actual thumbnail labels and card alignment.
2. Expand browser to300px; verify selected detail image is of the same model as the card.
3. Combine category+collection+search+favorite/recent filters; Reset filters must reveal the full current catalog.
4. Sort numbered variants (2 before10) and switch grid/list, preserving selected alias.
5. Click Place in scene while a stale viewport ground hit exists. Draft placement count must remain unchanged until a new ground click. Escape must leave count unchanged.
6. Drag a real model to ground, inspect placed object, Undo/Redo, Save, reopen. Check automatic collision handling remains unchanged.
7. Double-click a card; verify the actual selected asset opens in the existing inspector rather than placing.
8. Resize browser minimum112px at960x640; a full46px list row must be visible below the single compact toolbar. Open Filters and verify category, collection, sorting, grid/list, Favorites, Recent, Reset filters and Refresh previews remain accessible. Normal224px browser keeps its full toolbar.

No GPU app was launched by this UI agent. No frozen source, canonical source, installed runtime, user scene, or save was edited.
