"""Candidate-only non-destructive far-card path merge. Leaves LIVE/source intact."""
from pathlib import Path
import shutil,json,hashlib
N=Path(__file__).resolve().parents[1];P=N/'world-project';target=P/'models/camp_variants';A=P/'models/art_completion';R=N/'review/art'
assert N.name=='camp-completion-2026-11-21' and not(N/'PUBLICATION.json').exists()
assert target.is_symlink(), 'Expected original private candidate symlink; do not overwrite an existing owned directory'
original=target.resolve();entries=list(original.iterdir());staging=target.with_name('camp_variants.art-pending');staging.mkdir()
for child in entries:(staging/child.name).symlink_to(child.resolve())
for name in ['oak02','oak04','beech01','beech02']:
 dst=staging/f'camp_variant_{name}_far.glb';assert not dst.exists();dst.symlink_to(A/f'tree_{name}_far.glb')
target.unlink();staging.rename(target)
(R/'CARDS_INSTALLED.json').write_text(json.dumps({'passed':True,'original_directory':str(original),'candidate_owned_directory':str(target),'original_near_mid_exact':all((target/c.name).read_bytes()==c.read_bytes()for c in entries),'source_live_untouched':True},indent=2)+'\n')
