import fs from 'node:fs';
import path from 'node:path';
const file=process.argv[2];if(!file)throw Error('Pass the frame.jsonl path');
const records=fs.readFileSync(file,'utf8').trim().split(/\r?\n/).filter(Boolean).map(s=>JSON.parse(s));
const catalog=JSON.parse(fs.readFileSync('artifacts/pipeline/client-inventory.json','utf8')).assets;
const draws=records.filter(r=>r.kind==='draw'),textures=records.filter(r=>r.kind==='texture'),shaders=records.filter(r=>r.kind==='shader');
const groups=new Map();
for(const t of textures){
 t.matches=catalog.filter(a=>a.dds&&t.firstMipSHA256&&(a.dds.firstMipSHA256===t.firstMipSHA256||a.dds.hashVariants?.some(v=>v.sha256===t.firstMipSHA256))&&a.dds.width===t.width&&a.dds.height===t.height&&a.dds.format===t.format).map(a=>a.path);
 for(const match of t.matches){const hits=draws.filter(d=>d.textures.includes(t.id));groups.set(match,{texture:t.id,draws:hits.length,frames:[...new Set(hits.map(d=>d.frame))],examples:hits.slice(0,3)});}
}
const counts=(key)=>Object.fromEntries([...draws.reduce((m,d)=>{let value=JSON.stringify(key(d));m.set(value,(m.get(value)||0)+1);return m;},new Map())]);
const report={source:file,header:records.find(r=>r.kind==='header'),footer:records.find(r=>r.kind==='footer'),draws:draws.length,
 fixedFunction:draws.filter(d=>d.vs===0&&d.ps===0).length,programmableVS:draws.filter(d=>d.vs>0).length,programmablePS:draws.filter(d=>d.ps>0).length,
 fvf:counts(d=>d.fvf),depth:counts(d=>d.depth),targets:counts(d=>d.target),textures,shaders,assetMatches:Object.fromEntries(groups)};
fs.writeFileSync(path.join(path.dirname(file),'analysis.json'),JSON.stringify(report,null,2));
console.log(JSON.stringify({source:file,draws:report.draws,footer:report.footer,fixedFunction:report.fixedFunction,programmableVS:report.programmableVS,programmablePS:report.programmablePS,matchedAssets:[...groups.keys()],depth:report.depth},null,2));
