# Source-v4 structural Undo synchronization

The source cooker, collision payloads, fingerprints and rejection rules are unchanged.

The actual production synchronization method could give a newly re-registered source entity the display pose adjusted against the previously moved scene, then reset it to its durable source/override transform on the next call. The background cook and final renderer preparation therefore saw different geometry after Undo removed the original registration.

New source registrations now use the durable effective pose and immediately complete the same canonical authoring synchronization as subsequent calls. This adds no history actions or extra source identities and retains existing parent links, intended world transforms and visibility.

[The CPU regression](v4-sync/CPU-RESULTS.json) compiles the actual old and corrected production methods with the real authoring, entity graph and History implementations. Its 43 passing checks reproduce the old first/second Y mismatch (30.958831939697266 versus43.298831939697266), verify exact corrected first/second document bytes, Undo/Redo baseline poses, unchanged source identity count, and transformed-parent/unrelated-child/visibility preservation.

The staged collision result also retains its source Capture for diagnostics. A final fingerprint mismatch now prints the first exact differing aliases/models/pose components in decimal and hexadecimal, while preserving the previous live scene. Structural QA stops subsequent phases and input immediately after a failed publication.

Final viewer syntax checking passed. The root owns the next native/Windows actual-input acceptance replay.
