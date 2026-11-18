#include "odb2kicad_lib.h"
#include "odb_parser/odb_parser.h"
#include "writer/kicad_writer.h"
#include "writer/panel.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <cctype>
#include <exception>
#include <sstream>

namespace odb2kicad {

// The board as KiCad text, in the file format the options ask for (0: what the board needs)
static std::string writeBoard(KicadPcb pcb, const ConvertOptions& options) {
    pcb.fileFormat = options.kicadFormat;
    std::ostringstream oss;
    writeKicadPcb(oss, pcb);
    return oss.str();
}

// What a conversion carries from step to step: the job's folder, and each placed step as a
// board once it is built (a panel repeats it many times; nested panels place panels)
struct StepBuilder {
    std::string root;
    const ConvertOptions& options;
    struct Built {
        bool ok = false;
        KicadPcb pcb;
        double pivotX = 0, pivotY = 0;      // the step's datum, KiCad frame
        std::vector<std::string> warnings;  // its own report's warnings
    };
    std::map<std::string, Built> cache;     // by step name, lower case
    std::vector<std::string> open;          // the panels being built (a step may not place itself)

    StepBuilder(const std::string& r, const ConvertOptions& o) : root(r), options(o) {}

    // The step as a board, its placed steps included; lines about document layers and the
    // placements go to the design's report
    KicadPcb build(OdbDesign& design) {
        design.documentFeatureLimit = options.documentFeatureLimit;
        reportDocumentLayers(design);
        KicadPcb pcb = transformToKicad(design, &design.report);
        auto& repeats = design.stepHeader.repeats;
        if (repeats.empty()) return pcb;
        if (!options.placeSteps) {
            design.report.push_back("warning: step " + design.stepName + " places " + std::to_string(repeats.size()) +
                                    " STEP-REPEAT block(s) of other steps (a panel); nested steps are not drawn, convert them on their own");
            return pcb;
        }
        // the panel's own profile is the board edge: the placed boards' outlines become drawings
        // (their rout lines); a panel without a profile is the placed boards, each its own outline
        const bool frame = hasBoardEdge(pcb);
        const std::string edge = frame ? "Dwgs.User" : "Edge.Cuts";
        open.push_back(toLowerName(design.stepName));
        int instance = 0;
        for (auto& r : repeats) {
            std::string what = "STEP-REPEAT of step " + (r.name.empty() ? std::string("(no NAME)") : r.name);
            if (r.nx < 1 || r.ny < 1) {
                design.report.push_back("warning: " + what + " places it " + std::to_string(r.nx) + " x " + std::to_string(r.ny) +
                                        " times: nothing is drawn");
                continue;
            }
            if (r.mirror || r.flip) {
                design.report.push_back("warning: " + what + " is " + (r.flip ? "flipped (bottom side up)" : "mirrored") +
                                        ": not drawn (the converter places steps top side up, unmirrored)");
                continue;
            }
            Built* child = get(r.name, design, what);
            if (!child) continue;
            const int first = instance + 1;
            for (int j = 0; j < r.ny; j++)
                for (int i = 0; i < r.nx; i++) {
                    PanelPlacement at;
                    at.pivotX = child->pivotX;
                    at.pivotY = child->pivotY;
                    at.atX = r.x + i * r.dx;
                    at.atY = -(r.y + j * r.dy);
                    at.angle = r.angle;
                    at.instance = ++instance;
                    at.netPrefix = "Board_" + std::to_string(instance) + "-";
                    at.edgeLayer = edge;
                    placeBoardInPanel(pcb, child->pcb, at);
                }
            char buf[200];
            snprintf(buf, sizeof(buf), "%d x %d at (%g, %g) mm, every (%g, %g) mm, turned %g deg", r.nx, r.ny, r.x, r.y, r.dx, r.dy,
                     r.angle);
            design.report.push_back("info: panel " + design.stepName + ": step " + r.name + " placed " + buf + " (copies " +
                                    std::to_string(first) + "-" + std::to_string(instance) + "; nets Board_<n>-<net>, outlines on " +
                                    edge + ")");
            for (auto& w : child->warnings) design.report.push_back("warning: [placed step " + r.name + "]" + w.substr(8));
            child->warnings.clear();  // once per panel is enough
        }
        open.pop_back();
        return pcb;
    }

  private:
    static std::string toLowerName(std::string s) {
        for (auto& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    Built* get(const std::string& name, OdbDesign& panel, const std::string& what) {
        std::string key = toLowerName(name);
        if (std::find(open.begin(), open.end(), key) != open.end()) {
            panel.report.push_back("warning: " + what + " places a step inside itself: not drawn");
            return nullptr;
        }
        auto it = cache.find(key);
        if (it == cache.end()) {
            Built b;
            OdbDesign d;
            std::string why;
            if (name.empty() || !parseOdbStep(root, name, d, why)) {
                panel.report.push_back("warning: " + what + " cannot be read" + (why.empty() ? std::string() : " (" + why + ")") +
                                       ": not drawn");
                cache[key] = b;
                return nullptr;
            }
            b.pcb = build(d);
            // the datum, moved with the step by its origin like everything in it
            b.pivotX = d.stepHeader.xDatum + d.stepHeader.appliedX;
            b.pivotY = -(d.stepHeader.yDatum + d.stepHeader.appliedY);
            for (auto& l : d.report)
                if (l.compare(0, 8, "warning:") == 0) b.warnings.push_back(l);
            b.ok = true;
            it = cache.emplace(key, std::move(b)).first;
        }
        if (!it->second.ok) {
            panel.report.push_back("warning: " + what + " cannot be read: not drawn");
            return nullptr;
        }
        return &it->second;
    }
};

ConvertResult convert(const std::string& odbDirPath, const ConvertOptions& options) {
    ConvertResult result;
    std::string why;
    result.success = false;
    try {
        if (options.allSteps) {
            std::vector<OdbDesign> designs;
            if (!parseOdbJob(odbDirPath, designs, why)) {
                result.error = why;
                return result;
            }
            result.stepNames = listOdbSteps(odbDirPath);
            result.jobName = designs.front().info.jobName;
            StepBuilder builder(odbDirPath, options);
            for (auto& d : designs) {
                StepOutput out;
                out.step = d.stepName;
                out.kicad_pcb = writeBoard(builder.build(d), options);
                out.report = d.report;
                result.steps.push_back(std::move(out));
            }
            result.step = result.steps.front().step;
            result.report = result.steps.front().report;
            result.kicad_pcb = result.steps.front().kicad_pcb;
        } else {
            OdbDesign design;
            if (!parseOdbStep(odbDirPath, options.step, design, why)) {
                result.error = why;
                return result;
            }
            result.stepNames = listOdbSteps(odbDirPath);
            result.jobName = design.info.jobName;
            result.step = design.stepName;
            StepBuilder builder(odbDirPath, options);
            result.kicad_pcb = writeBoard(builder.build(design), options);
            result.report = design.report;
        }
    } catch (const std::exception& e) {  // native builds; the WASM build aborts instead (see wasm/src)
        result.error = std::string("internal: the converter failed: ") + e.what();
        return result;
    }
    result.success = true;
    return result;
}

std::string stepFileName(const std::string& jobName, const std::string& stepName) {
    auto clean = [](const std::string& in, const char* fallback) {
        std::string out;
        for (char c : in)
            out += (std::isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '+') ? c : '_';
        while (!out.empty() && out.front() == '.') out.erase(out.begin());  // no hidden files, no ".."
        return out.empty() ? std::string(fallback) : out;
    };
    return clean(jobName, "job") + "__" + clean(stepName, "step") + ".kicad_pcb";
}

} // namespace odb2kicad
