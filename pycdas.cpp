#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "ASTree.h"
#include "pyc_module.h"
#include "pyc_numeric.h"
#include "bytecode.h"

#include <emscripten.h>

#ifdef WIN32
#  define PATHSEP '\\'
#else
#  define PATHSEP '/'
#endif

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    const char* decompile(const char* filename, uint8_t* ptr, int length) {
        PycModule mod;
        std::ostringstream output;
        std::ostream& pyc_output = output;

        try {
            mod.loadFromBuffer(ptr, length);
        } catch (const std::exception& ex) {
            pyc_output << "Error loading file " << filename << ": " << ex.what() << "\n";
            const char* result = strdup(output.str().c_str());
            return result;
        }

        if (!mod.isValid()) {
            pyc_output << "Could not load file " << filename << "\n";
            const char* result = strdup(output.str().c_str());
            return result;
        }

        const char* dispname = strrchr(filename, PATHSEP);
        dispname = (dispname == NULL) ? filename : dispname + 1;
        formatted_print(pyc_output, "%s (Python %d.%d%s)\n", dispname,
                        mod.majorVer(), mod.minorVer(),
                        (mod.majorVer() < 3 && mod.isUnicode()) ? " -U" : "");

        try {
            output_object(mod.code().try_cast<PycObject>(), &mod, 0, 0, pyc_output);
        } catch (const std::exception& ex) {
            pyc_output << "Error decompyling " << filename << ": " << ex.what() << "\n";
            const char* result = strdup(output.str().c_str());
            return result;
        }

        const char* result = strdup(output.str().c_str());
        return result;
    }
}

int main(int argc, char* argv[])
{
    const char* infile = nullptr;
    bool marshalled = false;
    const char* version = nullptr;
    unsigned disasm_flags = 0;
    std::ostream* pyc_output = &std::cout;
    std::ofstream out_file;

    for (int arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "-o") == 0) {
            if (arg + 1 < argc) {
                const char* filename = argv[++arg];
                out_file.open(filename, std::ios_base::out);
                if (out_file.fail()) {
                    fprintf(stderr, "Error opening file '%s' for writing\n",
                            filename);
                    return 1;
                }
                pyc_output = &out_file;
            } else {
                fputs("Option '-o' requires a filename\n", stderr);
                return 1;
            }
        } else if (strcmp(argv[arg], "-c") == 0) {
            marshalled = true;
        } else if (strcmp(argv[arg], "-v") == 0) {
            if (arg + 1 < argc) {
                version = argv[++arg];
            } else {
                fputs("Option '-v' requires a version\n", stderr);
                return 1;
            }
        } else if (strcmp(argv[arg], "--pycode-extra") == 0) {
            disasm_flags |= Pyc::DISASM_PYCODE_VERBOSE;
        } else if (strcmp(argv[arg], "--show-caches") == 0) {
            disasm_flags |= Pyc::DISASM_SHOW_CACHES;
        } else if (strcmp(argv[arg], "--help") == 0 || strcmp(argv[arg], "-h") == 0) {
            fprintf(stderr, "Usage:  %s [options] input.pyc\n\n", argv[0]);
            fputs("Options:\n", stderr);
            fputs("  -o <filename>  Write output to <filename> (default: stdout)\n", stderr);
            fputs("  -c             Specify loading a compiled code object. Requires the version to be set\n", stderr);
            fputs("  -v <x.y>       Specify a Python version for loading a compiled code object\n", stderr);
            fputs("  --pycode-extra Show extra fields in PyCode object dumps\n", stderr);
            fputs("  --show-caches  Don't suprress CACHE instructions in Python 3.11+ disassembly\n", stderr);
            fputs("  --help         Show this help text and then exit\n", stderr);
            return 0;
        } else if (argv[arg][0] == '-') {
            fprintf(stderr, "Error: Unrecognized argument %s\n", argv[arg]);
            return 1;
        } else {
            infile = argv[arg];
        }
    }

    if (!infile) {
        fputs("No input file specified\n", stderr);
        return 1;
    }

    PycModule mod;
    if (!marshalled) {
        try {
            mod.loadFromFile(infile);
        } catch (std::exception &ex) {
            fprintf(stderr, "Error disassembling %s: %s\n", infile, ex.what());
            return 1;
        }
    } else {
        if (!version) {
            fputs("Opening raw code objects requires a version to be specified\n", stderr);
            return 1;
        }
        std::string s(version);
        auto dot = s.find('.');
        if (dot == std::string::npos || dot == s.size()-1) {
            fputs("Unable to parse version string (use the format x.y)\n", stderr);
            return 1;
        }
        int major = std::stoi(s.substr(0, dot));
        int minor = std::stoi(s.substr(dot+1, s.size()));
        mod.loadFromMarshalledFile(infile, major, minor);
    }
    const char* dispname = strrchr(infile, PATHSEP);
    dispname = (dispname == NULL) ? infile : dispname + 1;
    formatted_print(*pyc_output, "%s (Python %d.%d%s)\n", dispname,
                    mod.majorVer(), mod.minorVer(),
                    (mod.majorVer() < 3 && mod.isUnicode()) ? " -U" : "");
    try {
        output_object(mod.code().try_cast<PycObject>(), &mod, 0, disasm_flags,
                      *pyc_output);
    } catch (std::exception& ex) {
        fprintf(stderr, "Error disassembling %s: %s\n", infile, ex.what());
        return 1;
    }

    return 0;
}
