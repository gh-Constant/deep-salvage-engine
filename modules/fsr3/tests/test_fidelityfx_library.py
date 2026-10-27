#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1];build=root.parents[1]/'bin/fsr3-library-tests';build.mkdir(parents=True,exist_ok=True)
fixture=build/'provider.cpp'
fixture.write_text('''#include "thirdparty/amd-fidelityfx-api/ffx_api.h"
extern "C" ffxReturnCode_t ffxCreateContext(ffxContext*,ffxCreateContextDescHeader*,const ffxAllocationCallbacks*) {return 0;}
extern "C" ffxReturnCode_t ffxDestroyContext(ffxContext*,const ffxAllocationCallbacks*) {return 0;}
extern "C" ffxReturnCode_t ffxConfigure(ffxContext*,const ffxConfigureDescHeader*) {return 0;}
#ifndef OMIT_DISPATCH
extern "C" ffxReturnCode_t ffxDispatch(ffxContext*,const ffxDispatchDescHeader*) {return 0;}
#endif
extern "C" ffxReturnCode_t ffxQuery(ffxContext*,ffxQueryDescHeader* header) {
 auto *query=reinterpret_cast<ffxQueryDescGetVersions*>(header);
 if(*query->outputCount && query->versionIds) query->versionIds[0]=PROVIDER_ID;
 if(*query->outputCount && query->versionNames) query->versionNames[0]=PROVIDER_NAME;
 *query->outputCount=1;return 0;
}
''')
for name,version,identifier,extra in [('old','3.1.4','0x10001',[]),('new','4.1.1','0xff001234',[]),('bad','3.1.4','0x10001',['-DOMIT_DISPATCH'])]:
 subprocess.run(['g++','-std=c++17','-shared','-fPIC','-I',str(root),'-I',str(root.parents[1]),f'-DPROVIDER_NAME="{version}"',f'-DPROVIDER_ID={identifier}',*extra,str(fixture),'-o',str(build/f'{name}.so')],check=True)
pe=build/'windows.dll';pe.write_bytes(b'MZ\0\0')
exe=build/'test-loader'
subprocess.run(['g++','-std=c++17','-I',str(root),'-I',str(root.parents[1]),str(root/'tests/test_fidelityfx_library.cpp'),str(root/'fidelityfx_library.cpp'),'-o',str(exe),'-ldl'],check=True)
subprocess.run([str(exe),str(build/'old.so'),str(build/'new.so'),str(build/'bad.so'),str(pe)],check=True)
