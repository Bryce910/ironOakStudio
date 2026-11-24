# Seldonne region

This is the 1,280 × 768 metre exploration region around the arrival campsite and riverfront town. The region includes the west woodland loop, rolling meadow route, eastern woodland, northern rocky uplands, creek network, three crossings and a timber quay.

Use the workspace **Play_Seldonne_Region.sh** or **Edit_Seldonne_Region.sh** shortcuts after this build is installed. F11 switches between play and editor; Tab opens the atlas during play, and V switches the camera. The corner minimap follows the player and marks the camp, town, crossings and quay.

The final editable workcopy owns its control files, texture cache and import directories. Existing library asset files are linked; import a new revision rather than editing linked library files in place. The closed source snapshot and QA copies are evidence, not the designer workcopy.

The town has 43 houses, market stalls, household gardens, smith/inn workyards and a loading quay. This pass builds the environment and preserves the nine existing NPCs; it does not add enterable interiors or a new quest chain.

The region uses all 16 authored road/river records and all 256 terrain paint strokes. Adjust existing splines and painted surfaces first; extending these authoring limits is separate engine work. There are 8,076 explicit scenery placements, plus the existing generated ecology.

Windows visual checks use the new project data in the accepted Windows runtime under Wine. Earlier standalone demo archives are not changed by this environment-only pass.
