import fs from 'node:fs';
import assert from 'node:assert/strict';
import {parseGrassModel} from '../tools/import-grass-model.mjs';
const source=fs.readFileSync('assets/grass/universe/env_grass.obj','utf8'),model=parseGrassModel(source);
const lines=source.split(/\r?\n/),read=kind=>lines.filter(l=>l.startsWith(kind+' ')).map(l=>l.trim().split(/\s+/).slice(1).map(Number));
const positions=read('v'),uvs=read('vt'),normals=read('vn');
assert.equal(model.vertices.length,720);assert.equal(model.indices.length,2160);
for(let i=0;i<720;i++){
 assert.deepEqual(model.vertices[i].slice(0,3),positions[i],'Do not center, lift or regenerate model positions');
 assert.deepEqual(model.vertices[i].slice(3,6),normals[i],'Keep supplied normals');
 assert.deepEqual(model.vertices[i].slice(6,8),uvs[i],'Do not repack or crop the atlas UVs');
}
assert(model.bounds[1][0]<0&&model.bounds[1][1]>0,'The original pivot has below-ground geometry');
const triangles=new Map(),winding=t=>[t.join(','),[t[1],t[2],t[0]].join(','),[t[2],t[0],t[1]].join(',')].sort()[0];
for(let i=0;i<model.indices.length;i+=3){const t=model.indices.slice(i,i+3),key=[...t].sort((a,b)=>a-b).join(',');if(!triangles.has(key))triangles.set(key,[]);triangles.get(key).push(t);}
assert.equal(triangles.size,360);for(const pair of triangles.values()){assert.equal(pair.length,2);assert.equal(winding(pair[0]),winding([pair[1][0],pair[1][2],pair[1][1]]));}
for(const name of ['env_grass.png','env_grass-et01.png']){const b=fs.readFileSync('assets/grass/universe/'+name);assert.equal(b.readUInt32BE(16),256);assert.equal(b.readUInt32BE(20),128);assert.equal(b[25],6,'RGBA atlas required');}
console.log('PASS: supplied positions, pivot, normals, OBJ UVs, both triangle windings and both RGBA atlases preserved.');
