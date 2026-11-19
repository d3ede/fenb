import fs from 'node:fs';
import crypto from 'node:crypto';
import {pathToFileURL} from 'node:url';

export function parseGrassModel(text){
 const positions=[],uvs=[],normals=[],vertices=[],indices=[],tuples=new Map();
 const finite=(parts,count)=>{const values=parts.slice(0,count).map(Number);if(values.length!==count||values.some(x=>!Number.isFinite(x)))throw Error('Invalid OBJ coordinate');return values;};
 for(const line of text.split(/\r?\n/)){const [kind,...parts]=line.trim().split(/\s+/);
  if(kind==='v')positions.push(finite(parts,3));else if(kind==='vt')uvs.push(finite(parts,2));else if(kind==='vn')normals.push(finite(parts,3));
  else if(kind==='f'){
   if(parts.length!==3)throw Error('The supplied grass must remain triangulated');
   for(const tuple of parts){let index=tuples.get(tuple);if(index===undefined){const refs=tuple.split('/').map(x=>Number(x)-1);
    if(refs.length!==3||refs.some(x=>!Number.isInteger(x)||x<0)||!positions[refs[0]]||!uvs[refs[1]]||!normals[refs[2]])throw Error('Invalid OBJ index');
    index=vertices.length;tuples.set(tuple,index);vertices.push([...positions[refs[0]],...normals[refs[2]],...uvs[refs[1]]]);}
    indices.push(index);}
  }
 }
 if(vertices.length!==720||indices.length!==2160)throw Error('Unexpected supplied model topology');
 for(let first=0;first<vertices.length;first+=4){
  const quad=vertices.slice(first,first+4),bottom=[...quad].sort((a,b)=>a[1]-b[1]).slice(0,2);
  const anchor=[(bottom[0][0]+bottom[1][0])/2,(bottom[0][2]+bottom[1][2])/2];
  for(const vertex of quad)vertex.push(...anchor);
 }
 for(let i=0;i<indices.length;i+=3)if(new Set(indices.slice(i,i+3).map(x=>Math.floor(x/4))).size!==1)throw Error('Cross-quad topology; explicit anchors required');
 const bounds=[0,1,2].map(axis=>[Math.min(...positions.map(p=>p[axis])),Math.max(...positions.map(p=>p[axis]))]);
 return {vertices,indices,bounds};
}
export function generateGrassModel(input,output){
 const source=fs.readFileSync(input),model=parseGrassModel(source.toString('utf8'));
 const number=x=>{const f=Math.fround(x);return `${f===0?'0.0':f.toPrecision(9)}f`;};
 let text=`// Generated from the supplied OBJ by tools/import-grass-model.mjs; do not edit.\n// SHA-256: ${crypto.createHash('sha256').update(source).digest('hex')}\n#pragma once\nnamespace grass_model {\nstruct Vertex { float x,y,z,nx,ny,nz,u,v,rootX,rootZ; };\ninline constexpr Vertex Vertices[]={\n`;
 for(const vertex of model.vertices)text+=`{${vertex.map(number).join(',')}},\n`;
 text+='};\ninline constexpr unsigned short Indices[]={\n';
 for(let i=0;i<model.indices.length;i+=24)text+=model.indices.slice(i,i+24).join(',')+',\n';
 text+=`};\ninline constexpr float MaxHeight=${number(model.bounds[1][1])};\n}\n`;
 if(!fs.existsSync(output)||fs.readFileSync(output,'utf8')!==text){const temporary=output+'.tmp.'+process.pid;fs.writeFileSync(temporary,text);fs.renameSync(temporary,output);}
 console.log(`Imported original grass: ${model.vertices.length} vertices, ${model.indices.length/3} triangles, pivot and OBJ UVs unchanged.`);
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href)generateGrassModel(process.argv[2]??'assets/grass/universe/env_grass.obj',process.argv[3]??'src/generated/grass_model.hpp');
