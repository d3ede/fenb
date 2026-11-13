// Offline inspection only: no hooks, game writes or production rendering overhead.
import fs from 'node:fs';
import path from 'node:path';
const capture=process.argv[2],output=process.argv[3];
if(!capture||!output)throw Error('Usage: node tools/inspect-water-reflections.mjs <capture directory> <output directory>');
const rows=fs.readFileSync(path.join(capture,'frame.jsonl'),'utf8').trim().split(/\r?\n/).filter(Boolean).map(JSON.parse);
const image=rows.find(r=>r.kind==='water-image'&&r.file==='water-reflection.argb');
if(!image||image.pixelBytes!==4||image.width<1||image.height<1||image.width*image.height>8*1024*1024)
    throw Error('Missing bounded ARGB8 reflection image');
const source=fs.readFileSync(path.join(capture,image.file));
if(source.length!==image.width*image.height*4)throw Error('Invalid reflection payload size');
const draws=rows.filter(r=>r.kind==='water-replay');
const premultiplied=draws.some(r=>r.reflectionEncoding==='premultiplied-rgba');
const histogram=Array(16).fill(0);
function bitmap(mode){
    const b=Buffer.alloc(54+source.length);b.write('BM');b.writeUInt32LE(b.length,2);
    b.writeUInt32LE(54,10);b.writeUInt32LE(40,14);b.writeInt32LE(image.width,18);
    b.writeInt32LE(-image.height,22);b.writeUInt16LE(1,26);b.writeUInt16LE(32,28);
    for(let i=0;i<source.length;i+=4){
        const a=source[i+3];
        for(let c=0;c<3;++c)b[54+i+c]=mode==='confidence'?a:
            premultiplied?(a?Math.min(255,Math.round(source[i+c]*255/a)):0):source[i+c];
        b[54+i+3]=255;
        if(mode==='confidence')histogram[Math.min(15,a>>4)]++;
    }
    return b;
}
fs.mkdirSync(output,{recursive:true});
fs.writeFileSync(path.join(output,'confidence.bmp'),bitmap('confidence'));
fs.writeFileSync(path.join(output,'reflection-color.bmp'),bitmap('color'));
const formats={70:'D16_LOCKABLE',71:'D32',75:'D24S8',77:'D24X8',79:'D24X4S4',80:'D16'};
const depthFormats=[...new Set(draws.map(r=>r.nativeDepthFormat).filter(v=>v>0))];
const precision=[];
for(const draw of draws){
    if(!/^water-\d+$/.test(draw.prefix))throw Error('Invalid constant prefix');
    const file=path.join(capture,draw.prefix+'-ps.fp32');
    if(!fs.existsSync(file))continue;
    const ps=fs.readFileSync(file);if(ps.length<336)continue;
    const quantum=ps.readFloatLE(71*4)*Math.abs(ps.readFloatLE(63*4))*.5;
    if(Number.isFinite(quantum)&&quantum>0)precision.push(Math.round(Math.log2(1/quantum)));
}
const report={capture:path.resolve(capture),width:image.width,height:image.height,premultiplied,
    nativeDepthFormats:depthFormats.map(v=>({value:v,name:formats[v]??'driver-specific'})),confidenceHistogram:histogram,
    copiedDepthBitsFromShaderConstants:[...new Set(precision)],
    note:'Black means no accepted object reflection, including pixels outside water. Confidence alone does not identify the rejection reason. Off-screen geometry cannot be recovered from these buffers.'};
fs.writeFileSync(path.join(output,'report.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report,null,2));
