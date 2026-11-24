# Windows build verification — 1 November 2026

This is a native Windows x64 executable of Valwood's existing custom engine. It uses the same OpenGL renderer, scenes, shaders, terrain, characters and Studio tools as Linux. It is an unsigned development delivery for artist onboarding.

Completed: full editor cross-compilation; Windows native schema validation under Wine; Windows save/overwrite/reopen with actual transforms; Unicode/space paths; unsaved recovery and explicit save; coordinated checkpoints and interrupted journal rollback; exclusive project locking; bounded Python workers and argument quoting; importing an existing purchased static GLB with bundled Python. Native Windows Python also passed workspace creation, saved-file return export, review staging, seed preservation, existing-output rejection and file-conflict rejection. Linux designer/recovery regression checks and the Linux build also pass.

The portable package includes the runtime, application DLLs, embedded Python, native schema validator, materialized project assets, artist exchange tools, guides and provenance. Its manifest verifies file hashes and lengths. PE import inspection checks the bundled application dependency closure; GPU drivers and Windows system libraries remain machine prerequisites.

The complete 7.8 GB project was also verified and copied into a separate artist workspace using the delivered Windows Python action: 1,656 supported authoring/runtime baseline files. The Windows Production wrapper validated all 1,661 project runtime files with the native schema validator and reported 37 packaged PE binaries. No source archives, personal saves or recovery drafts were included.

The rendered launch test is **unverified**: this session has no accessible desktop display, and Wine could not load its window driver even in a separate virtual-display test. No successful Windows screenshot or interactive walkthrough is claimed. A Windows main-stack reserve issue found during startup was fixed; the built executable reaches SDL initialization, but this host cannot complete the windowed test.

Native Windows GPU rendering, mouse input and a sustained 45–60 FPS walkthrough must still be checked on the artist's Windows hardware. Wine runs on this Linux host are compatibility checks, not Windows hardware or performance certification. The owner previously encountered a Linux X11/Mesa presentation stall; that is separate from Windows acceptance. Follow the provided acceptance checklist and retain logs and raw timing sidecars.

The recovery tests cover process interruption, not every filesystem, device failure or power-loss scenario. Windows directory durability differs from POSIX directory fsync. Keep normal project backups and one author per scene assignment. Asset licenses are unchanged.
