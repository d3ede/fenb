// Offline inspection of bounded diagnostic crops; never reads or writes the game.
import fs from 'node:fs';
import path from 'node:path';
const [capture,output]=process.argv.slice(2);
if(!capture || !output)throw Error('Usage: node tools/inspect-surface-reflections.mjs <capture directory> <ignored output directory>');
if(path.resolve(capture)===path.resolve(output))throw Error('Choose a separate output directory');
const rows=fs.readFileSync(path.join(capture,'frame.jsonl'),'utf8').trim().split(/\r?\n/).filter(Boolean).map(JSON.parse);
const frames=rows.filter(r=>r.kind==='surface-frame');
if(!frames.length || frames.length>3)throw Error('Expected one to three bounded surface frames');
fs.mkdirSync(output,{recursive:true});
function half(value){
    const exponent=(value>>10)&31,fraction=value&1023,sign=value&32768?-1:1;
    return exponent===31?(fraction?NaN:sign*Infinity):sign*(exponent?(1+fraction/1024)*2**(exponent-15):fraction*2**-24);
}
function bitmap(width,height,pixels){
    const b=Buffer.alloc(54+pixels.length);b.write('BM');b.writeUInt32LE(b.length,2);
    b.writeUInt32LE(54,10);b.writeUInt32LE(40,14);b.writeInt32LE(width,18);b.writeInt32LE(-height,22);
    b.writeUInt16LE(1,26);b.writeUInt16LE(32,28);pixels.copy(b,54);return b;
}
function image(file,bytes){
    const record=rows.find(r=>r.kind==='surface-image' && r.file===file);
    if(!record || record.pixelBytes!==bytes || record.width<1 || record.height<1 || record.width*record.height>960*640)
        throw Error('Missing bounded image: '+file);
    const data=fs.readFileSync(path.join(capture,file));
    if(data.length!==record.width*record.height*bytes)throw Error('Invalid image length: '+file);
    return {...record,data};
}
const report=[];
for(const frame of frames){
    const prefix=frame.prefix;if(!/^surface-[0-2]$/.test(prefix))throw Error('Invalid surface frame prefix');
    const before=image(prefix+'-before.argb',4),after=image(prefix+'-after.argb',4);
    const maskFile=frame.maskFile??prefix+'-mask.rgba16f';
    if(maskFile!==prefix+'-mask.rgba16f' && maskFile!==prefix+'-mask.rgba32f')throw Error('Invalid mask filename');
    const mask=image(maskFile,maskFile.endsWith('rgba32f')?16:8),depth=image(prefix+'-depth.r32f',4);
    const channel=(i,c)=>mask.pixelBytes===16?mask.data.readFloatLE(i*16+c*4):half(mask.data.readUInt16LE(i*8+c*2));
    if([after,mask,depth].some(r=>['width','height','x','y','sourceWidth','sourceHeight'].some(k=>r[k]!==before[k])))
        throw Error('Surface crop coordinates differ');
    const uniforms=fs.readFileSync(path.join(capture,prefix+'-ps.fp32'));
    if(uniforms.length<60*16 || uniforms.length%16)throw Error('Invalid surface constants');
    if(frame.uniformRegisters && uniforms.length!==frame.uniformRegisters*16)throw Error('Constant count mismatch');
    const f=i=>uniforms.readFloatLE(i*4),camera=[f(0),f(1),f(2),f(3)],viewport=[f(4),f(5),f(6),f(7)];
    const texel=[f(8),f(9)],n=[f(52),f(53),f(54)],length=Math.hypot(...n);
    if(length)for(let i=0;i<3;++i)n[i]/=length;
    const quantum=uniforms.length>=61*16?f(240):null;
    const transformRoundoff=uniforms.length>=61*16?Math.max(0,f(243)):0;
    const validationAvailable=length>0 && camera[0]>0 && camera[1]>0 && camera[3]<0 &&
        viewport[2]>0 && viewport[3]>0 && texel.every(v=>v>0) && Number.isFinite(quantum) && quantum>0;
    const count=before.width*before.height,delta=Buffer.alloc(count*4),receivers=Buffer.alloc(count*4);
    const material=Buffer.alloc(count*4);
    let changed=0,covered=0,accepted=0,rejected=0,invalid=0,heightMin=Infinity,heightMax=-Infinity;
    for(let i=0;i<count;++i){
        const at=i*4;let maximum=0;
        for(let c=0;c<3;++c){const difference=Math.abs(after.data[at+c]-before.data[at+c]);maximum=Math.max(maximum,difference);delta[at+c]=Math.min(255,difference*8);}
        changed+=maximum>1;delta[at+3]=receivers[at+3]=material[at+3]=255;
        const reflectance=channel(i,0),coverage=channel(i,3);
        const height=channel(i,2),raw=depth.data.readFloatLE(i*4);
        if(![reflectance,coverage,height,raw].every(Number.isFinite)){++invalid;continue;}
        material[at]=material[at+1]=material[at+2]=Math.round(Math.max(0,Math.min(1,reflectance))*255);
        if(reflectance<.001 || coverage<.0001)continue;
        ++covered;heightMin=Math.min(heightMin,height);heightMax=Math.max(heightMax,height);
        if(!validationAvailable){receivers[at+1]=128;receivers[at+2]=255;continue;}
        const x=before.x+i%before.width,y=before.y+Math.floor(i/before.width);
        const ray=[((x/before.sourceWidth-viewport[0])/viewport[2]*2-1)/camera[0],
            (1-(y/before.sourceHeight-viewport[1])/viewport[3]*2)/camera[1],1];
        const center=ray.reduce((sum,v,k)=>sum+v*n[k],0);
        const span=Math.abs(n[0])*texel[0]/(camera[0]*viewport[2])+Math.abs(n[1])*texel[1]/(camera[1]*viewport[3]);
        const z=height/center;
        const za=height/(center-span),zb=height/(center+span),precision=Math.max(.012,z*.0015)+transformRoundoff/Math.abs(center);
        const near=Math.max(.001,Math.min(za,zb)-precision),far=Math.max(near,Math.max(za,zb)+precision);
        const allowance=quantum*.6+.00000012;
        const valid=raw<.999999 && Math.abs(center)>span && z>0 && z<=10000 &&
            raw>=camera[2]+camera[3]/near-allowance && raw<=camera[2]+camera[3]/far+allowance;
        if(valid){++accepted;receivers[at+1]=220;}else{++rejected;receivers[at+2]=255;}
    }
    for(const [name,pixels]of [['before',before.data],['after',after.data],['difference-x8',delta],['material',material],['receiver-validity',receivers]])
        fs.writeFileSync(path.join(output,prefix+'-'+name+'.bmp'),bitmap(before.width,before.height,pixels));
    report.push({frame:frame.frame,width:before.width,height:before.height,cropX:before.x,cropY:before.y,
        uniformRegisters:uniforms.length/16,depthQuantum:quantum,copiedDepthBits:quantum>0?Math.round(Math.log2(1/quantum)):null,
        validationAvailable,changedPixels:changed,reflectiveMaskPixels:covered,acceptedReceiverPixels:accepted,
        rejectedReceiverPixels:rejected,invalidPixels:invalid,receiverEncoding:'camera-relative-height',transformRoundoff,
        heightRange:covered?[heightMin,heightMax]:null});
}
const result={frames:report,note:'Receiver validity reproduces receiver admission only, not SSR hit confidence. Green means accepted, red rejected, amber unavailable, black outside the reflective mask. Differences show the surface pass before atmosphere and grading. Camera movement changes pixel correspondence between frames; image differences alone do not establish a cause of flicker.'};
fs.writeFileSync(path.join(output,'report.json'),JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(result,null,2));
