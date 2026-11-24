#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,time
N=Path(__file__).resolve().parents[1]
R=N/'runtime-native-v1'; T=N/'tools'; O=T/'preflight-objects';O.mkdir(exist_ok=True)
j=json.loads((R/'BUILD_RECEIPT.json').read_text());link=list(j['link']);source=T/'preflight_scene.cpp';own=O/'preflight_scene.cpp.o';renamed=O/'runtime_main-renamed.o';out=T/'preflight_scene'
main=R/'native-objects/main.c.o';subprocess.run(['objcopy','--redefine-sym','main=valwood_preflight_unused_runtime_main',str(main),str(renamed)],check=True)
compile=[link[0],'-std=c++17','-O2','-I'+str(N/'source-v1'),'-c',str(source),'-o',str(own)];subprocess.run(compile,check=True)
link=[str(renamed)if x==str(main)else x for x in link];link[1:1]=[str(own)];link[link.index('-o')+1]=str(out);link+=['-Wl,--wrap=world_model_load'];subprocess.run(link,check=True)
record={'compile':compile,'link':link,'original_main_unchanged':hashlib.sha256(main.read_bytes()).hexdigest(),'renamed_copy':str(renamed),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'runtime_receipt_binary_sha256':j['binary_sha256'],'object_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in map(Path,[x for x in link if x.endswith('.o')])},'scope':'CLI linking owned108 production object closure; originalmain object unchanged; wrapper only stops--records-only modeafteractualparser.'};(N/'review/preflight').mkdir(exist_ok=True);(N/'review/preflight/BUILD.json').write_text(json.dumps(record,indent=2)+'\n');print(out)
