const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),cp=require('node:child_process'),assert=require('node:assert/strict');
const [exe,tables,fixture]=process.argv.slice(2),original=fs.readFileSync(fixture,'utf8');
const changed=original.replace('zap-normal\ty\t40066666','zap-normal\ty\t40066667');
assert.notEqual(changed,original);
const dir=fs.mkdtempSync(path.join(os.tmpdir(),'grr-classic-negative-')),file=path.join(dir,'one-bit.tsv');
try{
 fs.writeFileSync(file,changed);
 const result=cp.spawnSync(exe,[tables,file],{encoding:'utf8',windowsHide:true});
 if(result.error)throw result.error;
 assert.equal(result.status,1);assert.match(result.stderr,/Original Classic mismatch: zap-normal\/y/);
 console.log('One-bit change in original Classic capture rejected at the expected field');
}finally{fs.unlinkSync(file);fs.rmdirSync(dir);}
