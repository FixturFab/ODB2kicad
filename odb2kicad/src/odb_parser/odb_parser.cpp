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

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design) {
    // 1. Parse misc/info
    std::string infoPath = rootDir + "/misc/info";
    if (fileExists(infoPath)) {
        if (!parseInfo(infoPath, design.info)) {
            std::cerr << "Warning: failed to parse " << infoPath << "\n";
        }
    }

    // 2. Parse matrix/matrix
    std::string matrixPath = rootDir + "/matrix/matrix";
    if (!parseMatrix(matrixPath, design.stepName, design.layers)) {
        std::cerr << "Error: failed to parse " << matrixPath << "\n";
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
            std::cerr << "Error: no step directory found\n";
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

    return true;
}

} // namespace odb2kicad
