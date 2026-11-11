import fs from 'node:fs';
import path from 'node:path';
import { Resvg } from '../artifacts/logo-renderer/node_modules/@resvg/resvg-js/index.js';

const folder=path.resolve(process.argv[2] ?? 'assets/branding/daylight');
const outputs=[];
for(const name of fs.readdirSync(folder).filter(n=>n.endsWith('.svg') && !n.includes('Figma-import') && (n.startsWith('fenb-') || n.includes('preview')))) {
 const source=fs.readFileSync(path.join(folder,name),'utf8');
 const preview=name.includes('preview');
 const renderer=new Resvg(source,{fitTo:preview?{mode:'width',value:1400}:{mode:'zoom',value:2},font:{loadSystemFonts:true}});
 const result=renderer.render();
 const output=path.join(folder,name.replace(/\.svg$/,'.png'));
 fs.writeFileSync(output,result.asPng());
 let transparentPixels=0;
 const pixels=result.pixels;
 for(let i=3;i<pixels.length;i+=4) if(pixels[i]===0) transparentPixels++;
 if(!preview && transparentPixels===0) throw new Error(`${name}: no transparent pixels`);
 if(!preview && /<(?:image|text)\b|<rect\b/.test(source)) throw new Error(`${name}: unexpected non-path content`);
 outputs.push({file:path.basename(output),width:result.width,height:result.height,transparentPercent:Math.round(transparentPixels/(result.width*result.height)*100)});
}
console.log(JSON.stringify(outputs,null,2));
