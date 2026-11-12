import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const directory=path.resolve('assets/branding/header-preview');
const sample=new Resvg('<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"><rect width="1" height="1" fill="#ff8040" opacity=".5"/></svg>').render().pixels;
const premultiplied=sample[0]<200;
for(const [name,width] of [['symbol',160],['horizontal',480]]){
 const source=fs.readFileSync(path.join(directory,name+'.svg'),'utf8');
 const first=new Resvg(source).render();
 let left=first.width,top=first.height,right=-1,bottom=-1;
 const px=first.pixels;
 for(let y=0;y<first.height;y++)for(let x=0;x<first.width;x++)if(px[(y*first.width+x)*4+3]>2){left=Math.min(left,x);top=Math.min(top,y);right=Math.max(right,x);bottom=Math.max(bottom,y);}
 if(right<left)throw new Error(`Empty artwork: ${name}`);
 left=Math.max(0,left-8);top=Math.max(0,top-8);right=Math.min(first.width,right+9);bottom=Math.min(first.height,bottom+9);
 const height=Math.round(width*(bottom-top)/(right-left));
 const cropped=source.replace(/<svg[^>]*>/,`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="${left} ${top} ${right-left} ${bottom-top}" fill="none">`);
 const render=new Resvg(cropped).render();
 const rgba=render.pixels,data=Buffer.alloc(8+rgba.length);
 data.writeUInt32LE(render.width,0);data.writeUInt32LE(render.height,4);
 for(let p=0;p<rgba.length;p+=4){
  const a=rgba[p+3],factor=premultiplied&&a?255/a:1;
  data[p+8]=Math.min(255,Math.round(rgba[p+2]*factor));
  data[p+9]=Math.min(255,Math.round(rgba[p+1]*factor));
  data[p+10]=Math.min(255,Math.round(rgba[p]*factor));data[p+11]=a;
 }
 fs.writeFileSync(path.join(directory,name+'.bgra'),data);
 console.log(JSON.stringify({name,width:render.width,height:render.height,premultipliedInput:premultiplied,bytes:data.length}));
}
