#include "odb_parser/matrix_parser.h"
#include "util/string_utils.h"
#include <fstream>

namespace odb2kicad {

bool parseMatrix(const std::string& path, std::string& stepName, std::vector<OdbLayer>& layers) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    enum { NONE, IN_STEP, IN_LAYER } state = NONE;
    OdbLayer currentLayer;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line == "STEP {") {
            state = IN_STEP;
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
            }
            state = NONE;
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (state == IN_STEP) {
            if (key == "NAME") stepName = val;
        } else if (state == IN_LAYER) {
            if (key == "ROW") currentLayer.row = std::stoi(val);
            else if (key == "CONTEXT") currentLayer.context = val;
            else if (key == "TYPE") currentLayer.type = val;
            else if (key == "NAME") currentLayer.name = val;
            else if (key == "POLARITY") currentLayer.polarity = val;
            else if (key == "START_NAME") currentLayer.startName = val;
            else if (key == "END_NAME") currentLayer.endName = val;
            else if (key == "DIELECTRIC_TYPE") currentLayer.dielectricType = val;
        }
    }
    return true;
}

} // namespace odb2kicad
