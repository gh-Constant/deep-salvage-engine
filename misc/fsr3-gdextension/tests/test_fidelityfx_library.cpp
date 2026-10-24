#include "src/fidelityfx_library.h"
#include <cassert>
#include <iostream>
#include <fstream>
int main(int argc,char **argv) {
    assert(argc==5);
    fidelityfx::Library library;
    assert(!library.open("relative.dll"));
    assert(!library.open(std::filesystem::absolute("missing-provider.dll")));
    assert(!library.open(std::filesystem::absolute(argv[3])));
    assert(library.error().find("five required")!=std::string::npos);
    assert(!library.open(std::filesystem::absolute(argv[4])));
    assert(library.error().find("Windows PE")!=std::string::npos);
    std::vector<fidelityfx::Provider> versions;
    assert(library.open(std::filesystem::absolute(argv[1])));
    assert(library.versions(versions));assert(versions.size()==1);
    assert(versions[0].name=="3.1.4");assert(versions[0].id==0x10001);
    library.close();
    assert(library.open(std::filesystem::absolute(argv[2])));
    assert(library.versions(versions));assert(versions.size()==1);
    assert(versions[0].name=="4.1.1");assert(versions[0].id==0xff001234);
    library.close();
    assert(!library.is_open() && !library.query && !library.create_context && !library.dispatch);
    std::cout<<"FidelityFX module loading, export validation, PE rejection and provider upgrades PASS\n";
}
