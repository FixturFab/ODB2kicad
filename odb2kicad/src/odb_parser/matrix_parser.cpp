#include "odb_parser/matrix_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <cstdlib>
#include <fstream>

namespace odb2kicad {

bool parseMatrix(const std::string& path, std::string& stepName, std::vector<OdbLayer>& layers) {
    std::vector<OdbMatrixStep> steps;
    if (!parseMatrix(path, steps, layers)) return false;
    if (!steps.empty()) stepName = steps.front().name;
    return true;
}

bool parseMatrix(const std::string& path, std::vector<OdbMatrixStep>& steps, std::vector<OdbLayer>& layers) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;

    std::string line;
    enum { NONE, IN_STEP, IN_LAYER } state = NONE;
    OdbLayer currentLayer;
    OdbMatrixStep currentStep;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line == "STEP {") {
            state = IN_STEP;
            currentStep = OdbMatrixStep();
            continue;
        }
        if (line == "LAYER {") {
            state = IN_LAYER;
            currentLayer = OdbLayer();
            continue;
        }
        if (line == "}") {
            if (state == IN_LAYER) {
                layers.push_back(currentLayer);
            } else if (state == IN_STEP && !currentStep.name.empty()) {
                steps.push_back(currentStep);
            }
            state = NONE;
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (state == IN_STEP) {
            if (key == "NAME") currentStep.name = val;
            else if (key == "COL") parseInt(val, currentStep.col);  // atoi is undefined past INT_MAX; a bad number keeps 0
        } else if (state == IN_LAYER) {
            if (key == "ROW") parseInt(val, currentLayer.row);  // rows keep their numbers: gaps are legal
            else if (key == "CONTEXT") currentLayer.context = val;
            else if (key == "TYPE") currentLayer.type = val;
            else if (key == "NAME") currentLayer.name = val;
            else if (key == "POLARITY") currentLayer.polarity = val;
            else if (key == "START_NAME") currentLayer.startName = val;
            else if (key == "END_NAME") currentLayer.endName = val;
            else if (key == "DIELECTRIC_TYPE") currentLayer.dielectricType = val;
            else if (key == "OLD_NAME") currentLayer.oldName = val;
            else if (key == "COLOR") currentLayer.color = val;
            else if (key == "ID") currentLayer.id = val;
        }
    }
    return true;
}

} // namespace odb2kicad
