#include "odb2kicad_lib.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static void usage() {
    std::cerr << "Usage: odb2kicad [--report] [--step <name>] [--no-panels] [--document-layers <all|none|N>] [--kicad <8|9>] <odb-directory> [output.kicad_pcb]\n";
    std::cerr << "  If output is omitted, writes to stdout.\n";
    std::cerr << "  A job with several steps: the first step goes to the output (or stdout), every\n";
    std::cerr << "  other step to <job>__<step>.kicad_pcb beside the output.\n";
    std::cerr << "  --step <name>  convert that step only\n";
    std::cerr << "  --no-panels    a panel step (STEP-REPEAT) without the steps it places; by default\n";
    std::cerr << "                 it holds a copy of each, nets named Board_<n>-<net>\n";
    std::cerr << "  --document-layers <all|none|N>  draw document layers (assembly drawings, process\n";
    std::cerr << "                 maps) of any size, never, or up to N features each (default 100000)\n";
    std::cerr << "  --kicad <8|9>  file format: 9 sizes vias and through-hole pads per copper layer\n";
    std::cerr << "                 (padstacks), 8 gives each one size (the largest). Default: 9 when\n";
    std::cerr << "                 the board needs it, else 8\n";
    std::cerr << "  --report       print what the reader chose and assumed (stderr)\n";
}

static void printReport(const std::vector<std::string>& report, bool full, const std::string& step) {
    for (auto& line : report) {
        std::string tag = step.empty() ? "" : " [" + step + "]";
        if (line.compare(0, 8, "warning:") == 0) std::cerr << "Warning" << tag << ":" << line.substr(8) << "\n";
        else if (full) std::cerr << line << "\n";
    }
}

static bool writeFile(const std::string& path, const std::string& text) {
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        std::cerr << "Error: cannot open output file: " << path << "\n";
        return false;
    }
    outFile << text;
    std::cerr << "Written: " << path << "\n";
    return true;
}

int main(int argc, char* argv[]) {
    bool fullReport = false;
    odb2kicad::ConvertOptions options;
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--report") fullReport = true;
        else if (a == "--no-panels") options.placeSteps = false;
        else if (a == "--step" && i + 1 < argc) options.step = argv[++i];
        else if (a.compare(0, 7, "--step=") == 0) options.step = a.substr(7);
        else if (a == "--document-layers" && i + 1 < argc) {
            std::string v = argv[++i];
            options.documentFeatureLimit = v == "all" ? -1 : v == "none" ? 0 : std::atoll(v.c_str());
        }
        else if (a == "--kicad" && i + 1 < argc) {
            std::string v = argv[++i];
            if (v != "8" && v != "9") { usage(); return 1; }
            options.kicadFormat = std::atoi(v.c_str());
        }
        else args.push_back(a);
    }
    if (args.empty()) {
        usage();
        return 1;
    }
    const bool toFile = args.size() >= 2;
    // every step only when there is a place to put the others
    options.allSteps = toFile && options.step.empty();

    auto result = odb2kicad::convert(args[0], options);
    if (!result.success) {
        std::cerr << "Error: " << result.error << "\n";
        return 1;
    }
    printReport(result.report, fullReport, "");

    if (!toFile) {
        std::cout << result.kicad_pcb;
        if (options.step.empty() && result.stepNames.size() > 1) {
            std::cerr << "Note: the job has " << result.stepNames.size() << " steps; this is " << result.step
                      << ". Give an output file to get the others too, or --step <name>:";
            for (auto& n : result.stepNames) std::cerr << " " << n;
            std::cerr << "\n";
        }
        return 0;
    }
    if (!writeFile(args[1], result.kicad_pcb)) return 1;
    // the other steps, beside the first one's file
    std::string dir = args[1];
    auto slash = dir.find_last_of("/\\");
    dir = slash == std::string::npos ? "" : dir.substr(0, slash + 1);
    for (size_t i = 1; i < result.steps.size(); i++) {
        printReport(result.steps[i].report, fullReport, result.steps[i].step);
        if (!writeFile(dir + odb2kicad::stepFileName(result.jobName, result.steps[i].step), result.steps[i].kicad_pcb)) return 1;
    }
    return 0;
}
