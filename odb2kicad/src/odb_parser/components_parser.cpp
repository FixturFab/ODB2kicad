#include "odb_parser/components_parser.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

bool parseComponents(const std::string& path, OdbComponents& components) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    OdbComponent* current = nullptr;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (startsWith(line, "UNITS=")) {
            components.units = line.substr(6);
            continue;
        }

        // Attribute definition
        if (line[0] == '@') {
            auto sp = line.find(' ');
            if (sp != std::string::npos) {
                int idx = std::stoi(line.substr(1, sp - 1));
                components.attrDefs[idx] = trim(line.substr(sp + 1));
            }
            continue;
        }

        // String definition
        if (line[0] == '&') {
            auto sp = line.find(' ');
            if (sp != std::string::npos) {
                int idx = std::stoi(line.substr(1, sp - 1));
                components.strings[idx] = trim(line.substr(sp + 1));
            }
            continue;
        }

        // CMP record
        if (startsWith(line, "CMP ")) {
            // Separate attrs
            std::string mainPart = line;
            std::string attrPart;
            auto semi = line.find(';');
            if (semi != std::string::npos) {
                mainPart = line.substr(0, semi);
                attrPart = line.substr(semi + 1);
            }

            std::istringstream iss(mainPart.substr(4));
            OdbComponent comp;
            std::string mirrorStr;
            iss >> comp.pkgIdx >> comp.x >> comp.y >> comp.rotation >> mirrorStr
                >> comp.refdes >> comp.footprint;
            comp.mirror = mirrorStr;

            // Parse mount type from attrs
            if (!attrPart.empty()) {
                auto parts = split(attrPart, ',');
                for (auto& part : parts) {
                    part = trim(part);
                    auto eq = part.find('=');
                    if (eq != std::string::npos) {
                        std::string key = part.substr(0, eq);
                        // Only parse numeric attribute keys; skip named attrs
                        try {
                            int idx = std::stoi(key);
                            std::string val = part.substr(eq + 1);
                            // Check if this attr is .comp_mount_type
                            auto it = components.attrDefs.find(idx);
                            if (it != components.attrDefs.end() && it->second == ".comp_mount_type") {
                                try { comp.mountType = std::stoi(val); } catch (...) {}
                            }
                        } catch (const std::invalid_argument&) {
                            // Named attribute from non-KiCad ODB++ — skip
                        }
                    }
                }
            }

            components.components.push_back(comp);
            current = &components.components.back();
            continue;
        }

        // PRP record
        if (startsWith(line, "PRP ") && current) {
            // PRP key 'value'
            auto rest = line.substr(4);
            auto sq1 = rest.find('\'');
            if (sq1 != std::string::npos) {
                std::string key = trim(rest.substr(0, sq1));
                auto sq2 = rest.find('\'', sq1 + 1);
                std::string val = (sq2 != std::string::npos) ? rest.substr(sq1 + 1, sq2 - sq1 - 1) : "";
                current->properties[key] = val;
            }
            continue;
        }

        // TOP record (terminal)
        if (startsWith(line, "TOP ") && current) {
            std::istringstream iss(line.substr(4));
            OdbTerminal term;
            std::string mirrorStr;
            iss >> term.pinIdx >> term.x >> term.y >> term.rotation >> mirrorStr
                >> term.netIdx >> term.subnetIdx >> term.pinNum;
            term.mirror = mirrorStr;
            current->terminals.push_back(term);
            continue;
        }
    }
    return true;
}

} // namespace odb2kicad
