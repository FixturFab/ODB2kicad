#include "odb_parser/odb_parser.h"
#include "odb_parser/info_parser.h"
#include "odb_parser/matrix_parser.h"
#include "odb_parser/stephdr_parser.h"
#include "odb_parser/profile_parser.h"
#include "odb_parser/features_parser.h"
#include "odb_parser/components_parser.h"
#include "odb_parser/eda_parser.h"
#include "odb_parser/netlist_parser.h"
#include "util/string_utils.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>

namespace odb2kicad {

static bool dirExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static std::vector<std::string> listDirs(const std::string& path) {
    std::vector<std::string> result;
    DIR* dir = opendir(path.c_str());
    if (!dir) return result;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string full = path + "/" + name;
        if (dirExists(full)) result.push_back(name);
    }
    closedir(dir);
    return result;
}

bool hasKicadDonutSkew(const OdbInfo& info) {
    const std::string app = "KiCad EDA ";
    if (info.saveApp.compare(0, app.size(), app) != 0) return false;
    // SAVE_APP=KiCad EDA <major>.<minor>...; KiCad 10 writes the inner diameter as 2R - w
    std::string ver = info.saveApp.substr(app.size());
    if (ver.empty() || !std::isdigit((unsigned char)ver[0])) return false;
    return std::atoi(ver.c_str()) <= 9;
}

bool isKicadProducer(const OdbInfo& info) {
    const std::string k = "KiCad";
    return info.odbSource.compare(0, k.size(), k) == 0 || info.saveApp.compare(0, k.size(), k) == 0;
}

void applyProducerQuirks(OdbDesign& design) {
    if (isKicadProducer(design.info)) {
        // KiCad writes a mirrored flash (orient 9) with its counter-clockwise angle (kicad-cli
        // 9.0.9: a bottom pad at 30 deg is "9 30.0"); the spec's angle is clockwise.
        for (auto& [name, feats] : design.layerFeatures)
            for (auto& p : feats.pads)
                if (p.mirror == 9) p.angle = -p.angle;
    }
    if (!hasKicadDonutSkew(design.info)) return;
    // KiCad 9 writes a circle of radius R and stroke w as donut_r(2R + w)x(2R - w/2): the inner
    // diameter is short of 2R - w by w/2, so the decoded ring width (od - id) / 2 is 3w/4. Its
    // od - id is 3w/2, which gives back w; the centreline (od - w) / 2 is then R again.
    // donut_r appears nowhere else in that exporter.
    for (auto& [name, feats] : design.layerFeatures) {
        for (auto& sym : feats.symbols) {
            if (sym.donut && sym.shape == OdbSymbol::ROUND)
                sym.lineWidth = sym.lineWidth * 4.0 / 3.0;
        }
    }
}

// File units -> mm factor: the file's UNITS line, else misc/info's, else INCH (the spec's default)
static double unitFactor(const std::string& fileUnits, const std::string& jobUnits) {
    std::string u = toUpper(trim(fileUnits.empty() ? jobUnits : fileUnits));
    return (u == "MM") ? 1.0 : 25.4;
}

void normalizeUnits(OdbDesign& design) {
    if (design.unitsNormalized) return;
    const std::string& job = design.info.units;
    auto scaleContours = [](std::vector<OdbSurface>& surfaces, double k) {
        for (auto& srf : surfaces)
            for (auto& c : srf.contours)
                for (auto& p : c.points) { p.x *= k; p.y *= k; p.xc *= k; p.yc *= k; }
    };
    auto scaleFeatures = [&](OdbFeatures& f) {
        double k = unitFactor(f.units, job);
        for (auto& sym : f.symbols) {
            // standard symbol sizes are mils (INCH) or microns (MM), decoded / 1000
            double ks = sym.unitOverride == 'I' ? 25.4 : sym.unitOverride == 'M' ? 1.0 : k;
            sym.width *= ks; sym.height *= ks; sym.diameter *= ks;
            sym.cornerRadius *= ks; sym.lineWidth *= ks;
            for (auto& ring : sym.outline)
                for (auto& [x, y] : ring) { x *= ks; y *= ks; }
            sym.unitOverride = 0;
        }
        for (auto& p : f.pads) { p.x *= k; p.y *= k; }
        for (auto& l : f.lines) { l.x1 *= k; l.y1 *= k; l.x2 *= k; l.y2 *= k; }
        for (auto& a : f.arcs) { a.xs *= k; a.ys *= k; a.xe *= k; a.ye *= k; a.xc *= k; a.yc *= k; }
        scaleContours(f.surfaces, k);
        f.units = "MM";
    };
    for (auto& [name, f] : design.layerFeatures) scaleFeatures(f);
    for (auto& [name, f] : design.userSymbols) scaleFeatures(f);  // each in its own UNITS
    {
        double k = unitFactor(design.profile.units, job);
        scaleContours(design.profile.surfaces, k);
        design.profile.units = "MM";
    }
    for (auto* comps : {&design.topComponents, &design.botComponents}) {
        double k = unitFactor(comps->units, job);
        for (auto& c : comps->components) {
            c.x *= k; c.y *= k;
            for (auto& t : c.terminals) { t.x *= k; t.y *= k; }
        }
        comps->units = "MM";
    }
    {
        double k = unitFactor(design.edaData.units, job);
        for (auto& pkg : design.edaData.packages) {
            for (auto& pin : pkg.pins) { pin.x *= k; pin.y *= k; }
            for (auto& ring : pkg.outlines)
                for (auto& [x, y] : ring) { x *= k; y *= k; }
        }
        design.edaData.units = "MM";
    }
    {
        double k = unitFactor(design.netlist.units, job);
        for (auto& pt : design.netlist.points) { pt.x *= k; pt.y *= k; pt.padW *= k; pt.padH *= k; }
        design.netlist.units = "MM";
    }
    design.unitsNormalized = true;
}

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design) {
    std::string error;
    if (parseOdbDirectory(rootDir, design, error)) return true;
    std::cerr << "Error: " << error << "\n";
    return false;
}

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design, std::string& error) {
    if (!dirExists(rootDir)) {
        error = "no-input: " + rootDir + " is not a folder (an archive must be unpacked first)";
        return false;
    }
    // 1. Parse misc/info
    std::string infoPath = rootDir + "/misc/info";
    if (fileExists(infoPath)) {
        if (!parseInfo(infoPath, design.info)) {
            std::cerr << "Warning: failed to parse " << infoPath << "\n";
        }
    }

    // 2. Parse matrix/matrix
    std::string matrixPath = rootDir + "/matrix/matrix";
    if (!fileExists(matrixPath)) {
        error = "not-odb: no matrix/matrix file; the input is not an ODB++ job folder (or the archive "
                "holds it under another name)";
        return false;
    }
    if (!parseMatrix(matrixPath, design.stepName, design.layers)) {
        error = "bad-matrix: matrix/matrix could not be read";
        return false;
    }
    if (design.layers.empty()) {
        error = "bad-matrix: matrix/matrix lists no LAYER records";
        return false;
    }

    // Find step name (default to lowercase of parsed name)
    std::string stepNameLower = toLower(design.stepName);
    std::string stepsDir = rootDir + "/steps";

    // Try to find the actual step directory (case-insensitive)
    std::string actualStepDir;
    auto stepDirs = listDirs(stepsDir);
    for (auto& d : stepDirs) {
        if (toLower(d) == stepNameLower) {
            actualStepDir = stepsDir + "/" + d;
            break;
        }
    }
    if (actualStepDir.empty()) {
        // Fallback: use first step dir
        if (!stepDirs.empty()) {
            actualStepDir = stepsDir + "/" + stepDirs[0];
        } else {
            error = "no-step: no step folder under steps/ (the matrix names step '" + design.stepName + "')";
            return false;
        }
    }

    // 3. Parse stephdr
    std::string stephdrPath = actualStepDir + "/stephdr";
    if (fileExists(stephdrPath)) {
        parseStepHeader(stephdrPath, design.stepHeader);
    }

    // 4. Parse profile
    std::string profilePath = actualStepDir + "/profile";
    if (fileExists(profilePath)) {
        if (!parseProfile(profilePath, design.profile)) {
            std::cerr << "Warning: failed to parse profile\n";
        }
    }

    // 5. Parse layer features
    std::string layersDir = actualStepDir + "/layers";
    auto layerDirs = listDirs(layersDir);
    for (auto& layerName : layerDirs) {
        std::string featPath = layersDir + "/" + layerName + "/features";
        if (fileExists(featPath)) {
            OdbFeatures feats;
            if (parseFeatures(featPath, feats)) {
                design.layerFeatures[toLower(layerName)] = std::move(feats);
            }
        }
    }

    // 5b. User symbols: symbols/<name>/features, drawn wherever a layer flashes <name>
    std::string symbolsDir = rootDir + "/symbols";
    for (auto& symName : listDirs(symbolsDir)) {
        std::string featPath = symbolsDir + "/" + symName + "/features";
        OdbFeatures feats;
        if (fileExists(featPath) && parseFeatures(featPath, feats))
            design.userSymbols[toLower(symName)] = std::move(feats);
    }

    if (design.layerFeatures.empty()) {
        error = "no-layers: no layer has a features file under " + actualStepDir.substr(rootDir.size() + 1) +
                "/layers (" + std::to_string(design.layers.size()) + " layers in the matrix)";
        return false;
    }

    // 6. Parse components (top and bottom)
    std::string compTopPath = layersDir + "/comp_+_top/components";
    if (fileExists(compTopPath)) {
        parseComponents(compTopPath, design.topComponents);
    }
    std::string compBotPath = layersDir + "/comp_+_bot/components";
    if (fileExists(compBotPath)) {
        parseComponents(compBotPath, design.botComponents);
    }

    // 7. Parse eda/data
    std::string edaPath = actualStepDir + "/eda/data";
    if (fileExists(edaPath)) {
        parseEdaData(edaPath, design.edaData);
    }

    // 8. Parse netlist
    std::string netlistPath = actualStepDir + "/netlists/cadnet/netlist";
    if (fileExists(netlistPath)) {
        parseNetlist(netlistPath, design.netlist);
    }

    applyProducerQuirks(design);
    normalizeUnits(design);
    return true;
}

} // namespace odb2kicad
