// Generate a timing-only native fixture from the checked regression scene.
// No client files are written; a client path only supplies original artwork.
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
const baseline=process.argv.includes('--baseline')?process.argv[process.argv.indexOf('--baseline')+1]:'src';
const assets=process.argv.includes('--assets')?process.argv[process.argv.indexOf('--assets')+1]:null;
let s=fs.readFileSync('tests/grass_test.cpp','utf8').replace('Width=1000,Height=700','Width=1920,Height=1080');
s=s.replace('    auto render=[&](bool on){',`    Com<IDirect3DQuery9> t0,t1,freq,disjoint;double gpuMs=0,cpuMs=0;bool profile=false;
    CHECK(SUCCEEDED(d->CreateQuery(D3DQUERYTYPE_TIMESTAMP,t0.out()))&&SUCCEEDED(d->CreateQuery(D3DQUERYTYPE_TIMESTAMP,t1.out()))&&SUCCEEDED(d->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ,freq.out()))&&SUCCEEDED(d->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT,disjoint.out())));
    LARGE_INTEGER cpuFrequency{};QueryPerformanceFrequency(&cpuFrequency);
    auto render=[&](bool on){`);
s=s.replace('d->BeginScene();const SurfaceDraw',`LARGE_INTEGER cpuStart{},cpuEnd{};QueryPerformanceCounter(&cpuStart);
        d->BeginScene();if(profile){disjoint->Issue(D3DISSUE_BEGIN);t0->Issue(D3DISSUE_END);}const SurfaceDraw`);
s=s.replace('d->EndScene();return Read(d);',`if(profile){t1->Issue(D3DISSUE_END);freq->Issue(D3DISSUE_END);disjoint->Issue(D3DISSUE_END);}d->EndScene();QueryPerformanceCounter(&cpuEnd);cpuMs=double(cpuEnd.QuadPart-cpuStart.QuadPart)*1000/cpuFrequency.QuadPart;
        if(profile){UINT64 a=0,b=0,f=0;BOOL changed=TRUE;bool ready=false;const ULONGLONG deadline=GetTickCount64()+5000;
            while(GetTickCount64()<deadline){ready=disjoint->GetData(&changed,4,D3DGETDATA_FLUSH)==S_OK&&t0->GetData(&a,8,D3DGETDATA_FLUSH)==S_OK&&t1->GetData(&b,8,D3DGETDATA_FLUSH)==S_OK&&freq->GetData(&f,8,D3DGETDATA_FLUSH)==S_OK;if(ready)break;Sleep(1);}
            gpuMs=ready&&!changed&&f&&b>=a?double(b-a)*1000/f:-1;}
        return Read(d);`);
const hook='    if(proxy){auto result=render(true);';
s=s.replace(hook,`    {D3DADAPTER_IDENTIFIER9 adapter{};root->GetAdapterIdentifier(0,0,&adapter);printf("GPU: %s; 1920x1080; no MSAA; original ground, fixed camera and light\\n",adapter.Description);
        for(int workload=0;workload<2;++workload)for(int quality=0;quality<3;++quality){settings.grassQuality=float(quality);settings.grassDensity=workload?3.f:1.25f;extraDraws=workload?2800:0;distantTiles=workload;
            for(int frame=0;frame<5;++frame)render(true);profile=true;std::vector<double> cpu,gpu;
            for(int frame=0;frame<24;++frame){render(true);CHECK(gpuMs>=0);cpu.push_back(cpuMs);gpu.push_back(gpuMs);}profile=false;std::sort(cpu.begin(),cpu.end());std::sort(gpu.begin(),gpu.end());
            printf("workload=%s quality=%d density=%.2f instances=%u cpu_median_ms=%.4f cpu_p90_ms=%.4f gpu_median_ms=%.4f gpu_p90_ms=%.4f\\n",workload?"dense-with-2800-distant-passes":"field",quality,settings.grassDensity,GrassFXFixture::count(grass),cpu[12],cpu[21],gpu[12],gpu[21]);}
        return stateErrors?1:0;}
`+hook);
fs.mkdirSync('build',{recursive:true});fs.writeFileSync('build/grass-timing.cpp',s);
const toolchain=fs.readdirSync('.tools').filter(n=>/^llvm-mingw-.*-ucrt-x86_64$/.test(n)).sort().at(-1);
if(!toolchain)throw Error('Portable LLVM-MinGW is missing');
const cxx=path.resolve('.tools',toolchain,'bin/i686-w64-mingw32-clang++.exe');
const exe=path.resolve('build/grass-timing.exe');
const compile=spawnSync(cxx,['-std=c++20','-O2','-static','-I'+baseline,'-Isrc','build/grass-timing.cpp',path.join(baseline,'grass.cpp'),'-ld3d9','-ldxguid','-lbcrypt','-lgdiplus','-o',exe],{stdio:'inherit'});
if(compile.status)process.exit(compile.status??1);
if(process.argv.includes('--compile-only'))process.exit(0);
const result=spawnSync(exe,assets?[assets]:[],{stdio:'inherit'});process.exit(result.status??1);
