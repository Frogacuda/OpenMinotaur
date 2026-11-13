const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),cp=require('node:child_process'),assert=require('node:assert/strict');
const [exe,fixture]=process.argv.slice(2),lines=fs.readFileSync(fixture,'utf8').split(/\r?\n/);
let rows=0;
for(let i=0;i<lines.length;i++)if(lines[i]&&!lines[i].startsWith('#')){
 if(++rows===2){const fields=lines[i].split(' ');fields[3]=(BigInt('0x'+fields[3])^1n).toString(16).padStart(16,'0');lines[i]=fields.join(' ');break;}
}
assert.equal(rows,2);
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'grr-negative-'));
const file=path.join(dir,'one-bit-change.tsv');
try{
 fs.writeFileSync(file,lines.join('\n'));
 const result=cp.spawnSync(exe,[file],{encoding:'utf8',windowsHide:true});
 if(result.error)throw result.error;
 assert.equal(result.status,1);assert.match(result.stderr,/First divergence at age 1: x/);
 console.log('One-bit original-position mutation correctly rejected at first divergence');
}finally{
 if(path.dirname(file)!==dir||path.dirname(dir)!==os.tmpdir())throw Error('Unexpected cleanup path');
 fs.unlinkSync(file);fs.rmdirSync(dir);
}
