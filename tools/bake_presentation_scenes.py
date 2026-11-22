"""Prepare fixed scenes and record their complete, validated resource closure."""
from pathlib import Path
import argparse,hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def compiler_signature():
 files=[]
 for p in (ROOT/'modules/neon_background').rglob('*'):
  if p.suffix in {'.cpp','.h'}:files.append(p)
 files.append(ROOT/'tools/neon_scene_bake.cpp')
 for p in (ROOT/'assets/presentation/neon').rglob('*'):
  if p.suffix in {'.grpreset','.tsv','.bin','.constants'}:files.append(p)
 digest=hashlib.sha256()
 for p in sorted(files):digest.update(p.relative_to(ROOT).as_posix().encode()+b'\0'+p.read_bytes()+b'\0')
 return digest.hexdigest()
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--baker',type=Path,required=True)
 args=parser.parse_args();baker=args.baker.resolve()
 presentation=ROOT/'assets/presentation';output=presentation/'compiled';output.mkdir(exist_ok=True)
 entries=[]
 for scene in sorted(presentation.glob('**/*.neongles')):
  if output in scene.parents:continue
  relative=scene.relative_to(presentation);harness=scene.with_suffix('.nhml')
  name=relative.with_suffix('.neonb').as_posix();target=output/name;target.parent.mkdir(parents=True,exist_ok=True)
  command=[str(baker),str(presentation/'neon'),str(scene),str(target)]
  if harness.exists():command.append(str(harness))
  subprocess.run(command,check=True)
  metadata=json.loads(target.with_suffix(target.suffix+'.json').read_text(encoding='utf-8'))
  resources={name:digest(presentation/'neon'/name) for name in metadata['resources']}
  entry={'source':relative.as_posix(),'source_sha256':digest(scene),'file':name,'sha256':digest(target),'resources':resources,'exports':metadata['exports']}
  if harness.exists():entry.update(harness=harness.relative_to(presentation).as_posix(),harness_sha256=digest(harness))
  entries.append(entry)
 manifest={'format':1,'compiler_signature':compiler_signature(),'scenes':entries}
 (output/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
 print('Prepared',len(entries),'scenes;',len(set(r for e in entries for r in e['resources'])),'shared resources.')
if __name__=='__main__':main()
