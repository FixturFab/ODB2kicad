#include "odb_parser/odb_parser.h"
#include "odb_parser/info_parser.h"
#include "odb_parser/matrix_parser.h"
#include "odb_parser/stephdr_parser.h"
#include "odb_parser/profile_parser.h"
#include "odb_parser/features_parser.h"
#include "odb_parser/components_parser.h"
#include "odb_parser/eda_parser.h"
#include "odb_parser/netlist_parser.h"
#include "odb_parser/tools_parser.h"
#include "odb_parser/attrlist_parser.h"
#include "odb_parser/font_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
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

bool isOdxXml(const std::string& path) {
    auto opened = openOdbFile(path);
    if (!opened) return false;
    std::istream& file = *opened;
    char buf[200];
    file.read(buf, sizeof(buf));
    std::string head(buf, (size_t)file.gcount());
    return head.find('<') != std::string::npos && head.find('>') != std::string::npos &&
           head.find("xml") != std::string::npos && head.find("ODX") != std::string::npos;
}

void checkJob(OdbDesign& design) {
    // version: the reader's grammar is 8.x; an older job is read with it and said so
    auto& info = design.info;
    std::string who;
    if (!info.odbSource.empty()) who += ", source " + info.odbSource;
    if (!info.saveApp.empty()) who += ", saved by " + info.saveApp;
    if (info.versionMajor < 0) {
        design.report.push_back("info: ODB++ version not given (misc/info has no ODB_VERSION_MAJOR)" + who);
    } else {
        std::string v = std::to_string(info.versionMajor) + "." + std::to_string(std::max(info.versionMinor, 0));
        design.report.push_back("info: ODB++ version " + v + who);
        if (info.versionMajor < 8)
            design.report.push_back("warning: the job is ODB++ version " + v + "; it is read as version 8, which may lose data");
    }
    // matrix: every row keeps its number (gaps are legal); a TYPE outside the standard thirteen
    // is drawn as a DOCUMENT layer, a drill span that names no copper row is a through span
    static const char* kTypes[] = {"SIGNAL", "POWER_GROUND", "MIXED", "SOLDER_MASK", "SILK_SCREEN", "SOLDER_PASTE", "DRILL",
                                   "ROUT", "DOCUMENT", "MASK", "DIELECTRIC", "CONDUCTIVE_PASTE", "COMPONENT"};
    std::vector<int> rows;
    for (auto& l : design.layers) {
        rows.push_back(l.row);
        std::string t = toUpper(l.type);
        bool known = false;
        for (auto* k : kTypes) known = known || t == k;
        if (!known) {
            design.report.push_back("warning: layer " + l.name + ": unknown type " + (l.type.empty() ? "(none)" : l.type) +
                                    " is changed to DOCUMENT");
            l.writtenType = l.type;
            l.type = "DOCUMENT";
        }
    }
    for (auto& l : design.layers) {
        if (toUpper(l.type) != "DRILL") continue;
        for (auto* end : {&l.startName, &l.endName}) {
            if (end->empty()) continue;
            bool copper = false;
            for (auto& o : design.layers) {
                std::string t = toUpper(o.type);
                copper = copper || (toLower(o.name) == toLower(*end) && (t == "SIGNAL" || t == "POWER_GROUND" || t == "MIXED"));
            }
            if (!copper) {
                design.report.push_back("warning: drill layer " + l.name + ": span end '" + *end +
                                        "' is not a copper layer of the matrix; drilled through instead");
                l.startName.clear();
                l.endName.clear();
                break;
            }
        }
    }
    std::sort(rows.begin(), rows.end());
    std::string gaps;
    for (size_t i = 1; i < rows.size(); i++)
        for (int r = rows[i - 1] + 1; r < rows[i]; r++) gaps += (gaps.empty() ? "" : ", ") + std::to_string(r);
    if (!rows.empty())
        design.report.push_back("info: matrix: " + std::to_string(design.steps.size()) + " step(s), " + std::to_string(rows.size()) +
                                " layer rows (" + std::to_string(rows.front()) + " to " + std::to_string(rows.back()) +
                                (gaps.empty() ? "" : ", no row " + gaps) + ")");
}

static std::vector<std::string> listFiles(const std::string& path) {
    std::vector<std::string> result;
    DIR* dir = opendir(path.c_str());
    if (!dir) return result;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (fileExists(path + "/" + name)) result.push_back(name);
    }
    closedir(dir);
    std::sort(result.begin(), result.end());
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
            for (auto* shapes : {&sym.outlineAdd, &sym.outlineCut})
                for (auto& shape : *shapes)
                    for (auto& ring : shape)
                        for (auto& [x, y] : ring) { x *= ks; y *= ks; }
            sym.unitOverride = 0;
        }
        for (auto& p : f.pads) { p.x *= k; p.y *= k; }
        for (auto& l : f.lines) { l.x1 *= k; l.y1 *= k; l.x2 *= k; l.y2 *= k; }
        for (auto& a : f.arcs) { a.xs *= k; a.ys *= k; a.xe *= k; a.ye *= k; a.xc *= k; a.yc *= k; }
        for (auto& t : f.texts) { t.x *= k; t.y *= k; t.xsize *= k; t.ysize *= k; }
        scaleContours(f.surfaces, k);
        f.units = "MM";
    };
    for (auto& [name, f] : design.layerFeatures) scaleFeatures(f);
    for (auto& [name, f] : design.userSymbols) scaleFeatures(f);  // each in its own UNITS
    for (auto& [name, t] : design.layerTools) {
        double k = unitFactor(t.units, job) / 1000.0;  // mils or microns -> mm
        t.thickness *= k;
        for (auto& tool : t.tools) {
            tool.minTol *= k; tool.maxTol *= k; tool.finishSize *= k; tool.drillSize *= k;
        }
        t.units = "MM";
    }
    {
        double k = unitFactor(design.profile.units, job);
        scaleContours(design.profile.surfaces, k);
        design.profile.units = "MM";
    }
    std::vector<OdbComponents*> componentSets = {&design.topComponents, &design.botComponents};
    for (auto& cl : design.otherComponents) componentSets.push_back(&cl.components);
    for (auto* comps : componentSets) {
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
            for (auto& pin : pkg.pins) {
                pin.x *= k; pin.y *= k;
                for (auto& ring : pin.outlines)
                    for (auto& [x, y] : ring) { x *= k; y *= k; }
            }
            for (auto& ring : pkg.outlines)
                for (auto& [x, y] : ring) { x *= k; y *= k; }
        }
        design.edaData.units = "MM";
    }
    {
        double k = unitFactor(design.netlist.units, job);
        for (auto& pt : design.netlist.points) { pt.x *= k; pt.y *= k; pt.padW *= k; pt.padH *= k; pt.radius *= k; }
        design.netlist.units = "MM";
    }
    {
        double k = unitFactor(design.stepHeader.units, job);
        auto& h = design.stepHeader;
        h.xOrigin *= k; h.yOrigin *= k; h.xDatum *= k; h.yDatum *= k;
        for (auto& r : h.repeats) { r.x *= k; r.y *= k; r.dx *= k; r.dy *= k; }
        h.units = "MM";
    }
    drawTexts(design);
    applyLayerOutputTransforms(design);
    applyStepOrigin(design);
    design.unitsNormalized = true;
}

void reportDocumentLayers(OdbDesign& design) {
    if (isKicadProducer(design.info) || design.documentFeatureLimit < 0) return;
    for (auto& l : design.layers) {
        if (toUpper(l.type) != "DOCUMENT") continue;
        auto fit = design.layerFeatures.find(toLower(l.name));
        if (fit == design.layerFeatures.end()) continue;
        auto& f = fit->second;
        long long count = (long long)f.pads.size() + (long long)f.lines.size() + (long long)f.arcs.size() + (long long)f.surfaces.size();
        if (count == 0 || count <= design.documentFeatureLimit) continue;
        design.report.push_back("warning: document layer " + l.name + " (" + std::to_string(count) + " features) is not drawn: over the limit of " +
                                std::to_string(design.documentFeatureLimit) + " features per document layer (CLI: --document-layers all)");
    }
}

void drawTexts(OdbDesign& design) {
    for (auto& [layerName, f] : design.layerFeatures) {
        if (f.barcodes > 0)
            design.report.push_back("warning: layer " + layerName + ": " + std::to_string(f.barcodes) +
                                    " barcode record(s) are not converted");
        int undirected = 0;
        for (auto& a : f.arcs) if (!a.directionGiven) undirected++;
        if (undirected > 0)
            design.report.push_back("info: layer " + layerName + ": " + std::to_string(undirected) +
                                    " arc(s) carry no direction token and are read counter-clockwise (centre on the left of travel)");
        if (f.texts.empty()) continue;
        std::map<long long, int> widthSymbol;  // stroke width (nm) -> symbol index
        std::map<std::string, int> noFont;
        std::string missing;
        int drawn = 0, strokes = 0, polygons = 0;
        for (size_t ti = 0; ti < f.texts.size(); ti++) {
            OdbText& t = f.texts[ti];
            if (t.strokes > 0) continue;  // already drawn
            // dynamic text: the job, step and layer names
            for (auto& [var, value] : {std::pair<std::string, std::string>{"$$JOB", design.info.jobName},
                                       {"$$STEP", design.stepName}, {"$$LAYER", layerName}}) {
                std::string up = toUpper(t.text);
                for (auto at = up.find(var); at != std::string::npos; at = up.find(var)) {
                    t.text.replace(at, var.size(), value);
                    up = toUpper(t.text);
                    if (toUpper(value).find(var) != std::string::npos) break;
                }
            }
            auto fit = design.fonts.find(toLower(t.font));
            if (fit == design.fonts.end()) { noFont[t.font]++; continue; }
            std::vector<TextPolygon> filled;
            for (auto& s : textStrokes(t, fit->second, &missing, &filled)) {
                long long key = std::llround(s.width * 1e6);
                auto wit = widthSymbol.find(key);
                if (wit == widthSymbol.end()) {
                    OdbSymbol sym;
                    sym.name = "text-stroke";
                    sym.shape = OdbSymbol::ROUND;
                    sym.diameter = sym.width = sym.height = s.width;
                    f.symbols.push_back(sym);
                    wit = widthSymbol.emplace(key, (int)f.symbols.size() - 1).first;
                }
                OdbLine l;
                l.featIdx = t.featIdx;      // the text's place in file order (negative text clears what came before it)
                l.textIdx = (int)ti;
                l.x1 = s.x1; l.y1 = s.y1; l.x2 = s.x2; l.y2 = s.y2;
                l.symIdx = wit->second;
                l.polarity = s.positive ? "P" : "N";
                l.attrs = t.attrs;
                f.lines.push_back(l);
                t.strokes++;
            }
            // filled glyphs (POLYGON): surfaces at the text's place in file order
            for (auto& poly : filled) {
                OdbSurface srf;
                srf.featIdx = t.featIdx;
                srf.polarity = poly.positive ? "P" : "N";
                OdbContour c;
                for (auto& [x, y] : poly.points) { OdbPoint pt; pt.x = x; pt.y = y; c.points.push_back(pt); }
                if (c.points.front().x != c.points.back().x || c.points.front().y != c.points.back().y)
                    c.points.push_back(c.points.front());
                srf.contours.push_back(std::move(c));
                f.surfaces.push_back(std::move(srf));
                t.strokes++;
                polygons++;
            }
            if (t.strokes > 0) { drawn++; strokes += t.strokes; }
        }
        if (drawn > 0)
            design.report.push_back("info: layer " + layerName + ": " + std::to_string(drawn) + " text record(s) drawn as " +
                                    std::to_string(strokes - polygons) + " font strokes" +
                                    (polygons > 0 ? " and " + std::to_string(polygons) + " filled glyph outlines" : ""));
        for (auto& [font, n] : noFont)
            design.report.push_back("warning: layer " + layerName + ": " + std::to_string(n) + " text record(s) not drawn, the job has no font '" +
                                    font + "' under fonts/");
        if (!missing.empty())
            design.report.push_back("warning: layer " + layerName + ": the font has no character for '" + missing + "'; left blank");
    }
}

static std::string fmtNum(double v, int digits = 4) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", digits, v);
    std::string s = buf;
    if (s.find('.') != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

void computeBoardThickness(OdbDesign& design) {
    const std::string& job = design.info.units;
    const double mmPerOz = 0.0348;  // 1 oz/ft2 of copper is 1.37 mil thick
    double dielectric = 0, copper = 0, mask = 0;
    int dielectricRows = 0;
    for (auto& l : design.layers) {
        if (toUpper(l.context) == "MISC") continue;
        auto it = design.layerAttrs.find(toLower(l.name));
        if (it == design.layerAttrs.end()) continue;
        const OdbAttrList& a = it->second;
        double own = std::max(0.0, a.number(".layer_dielectric")) * unitFactor(a.units, job);
        std::string type = toUpper(l.type);
        if (type == "DIELECTRIC") {
            dielectric += own;
            if (own > 0) dielectricRows++;
        } else if (type == "SIGNAL" || type == "POWER_GROUND" || type == "MIXED") {
            double oz = a.number(".copper_weight");
            copper += oz > 0 ? oz * mmPerOz : own;
        } else if (type == "SOLDER_MASK") {
            mask += own;
        }
    }
    design.boardThickness = 0;
    // An explicit .board_thickness wins: the step's attrlist, then the job's, then a layer's
    {
        std::vector<std::pair<std::string, const OdbAttrList*>> places = {
            {"steps/" + toLower(design.stepName) + "/attrlist", &design.stepAttrs}, {"misc/attrlist", &design.jobAttrs}};
        for (auto& l : design.layers) {
            auto it = design.layerAttrs.find(toLower(l.name));
            if (it != design.layerAttrs.end()) places.push_back({"layers/" + toLower(l.name) + "/attrlist", &it->second});
        }
        for (auto& [where, a] : places) {
            double v = 0;
            for (const char* key : {".board_thickness", "board_thickness"})
                if (v <= 0) v = a->number(key);
            if (v <= 0) continue;
            design.boardThickness = std::round(v * unitFactor(a->units, job) * 1e4) / 1e4;
            design.thicknessSource = "board_thickness";
            design.report.push_back("info: board thickness " + fmtNum(design.boardThickness) + " mm from .board_thickness of " + where);
            return;
        }
    }
    if (dielectricRows > 0) {
        design.boardThickness = std::round((dielectric + copper + mask) * 1000.0) / 1000.0;
        design.thicknessSource = "attrlist";
        design.report.push_back("info: board thickness " + fmtNum(design.boardThickness) + " mm from the layer attrlists (" +
                                std::to_string(dielectricRows) + " dielectric rows " + fmtNum(dielectric) + " mm, copper " +
                                fmtNum(copper) + " mm, solder mask " + fmtNum(mask) + " mm)");
        return;
    }
    double drilled = 0;
    std::string from;
    for (auto& [name, t] : design.layerTools)
        if (t.thickness > drilled) { drilled = t.thickness; from = name; }
    if (drilled > 0) {
        design.boardThickness = std::round(drilled * 1e4) / 1e4;
        design.thicknessSource = "tools";
        design.report.push_back("info: board thickness " + fmtNum(design.boardThickness) + " mm from THICKNESS of layers/" + from +
                                "/tools (no .board_thickness, no dielectric row has .layer_dielectric)");
        return;
    }
    design.thicknessSource = "default";
    design.report.push_back("info: board thickness 1.6 mm by default (no .board_thickness, no .layer_dielectric on a dielectric row, no drill tools THICKNESS)");
}

void applyLayerOutputTransforms(OdbDesign& design) {
    for (auto& [name, f] : design.layerFeatures) {
        auto it = design.layerAttrs.find(name);
        if (it == design.layerAttrs.end()) continue;
        const OdbAttrList& a = it->second;
        double sx = a.number(".out_x_scale", 1.0), sy = a.number(".out_y_scale", 1.0);
        if (sx <= 0) sx = 1.0;
        if (sy <= 0) sy = 1.0;
        const bool mirror = a.flag(".out_mirror");
        const double angle = a.number(".out_angle", 0.0);  // degrees, clockwise like every ODB++ angle
        const bool scaled = std::abs(sx - 1.0) > 1e-9 || std::abs(sy - 1.0) > 1e-9;
        const bool turned = std::abs(std::fmod(angle, 360.0)) > 1e-9;
        if (!scaled && !mirror && !turned) continue;
        const double rad = angle * 3.14159265358979323846 / 180.0, ca = std::cos(rad), sa = std::sin(rad);
        auto move = [&](double& x, double& y) {
            double px = x * sx, py = y * sy;
            if (mirror) px = -px;
            x = px * ca + py * sa;   // clockwise, y up
            y = -px * sa + py * ca;
        };
        // sizes follow an even scale; an uneven one moves the features and leaves their symbols
        const bool even = std::abs(sx - sy) <= 1e-9;
        if (scaled && even) {
            for (auto& sym : f.symbols) {
                sym.width *= sx; sym.height *= sx; sym.diameter *= sx; sym.cornerRadius *= sx; sym.lineWidth *= sx;
                for (auto& ring : sym.outline)
                    for (auto& [x, y] : ring) { x *= sx; y *= sx; }
                for (auto* shapes : {&sym.outlineAdd, &sym.outlineCut})
                    for (auto& shape : *shapes)
                        for (auto& ring : shape)
                            for (auto& [x, y] : ring) { x *= sx; y *= sx; }
            }
        }
        for (auto& p : f.pads) {
            move(p.x, p.y);
            if (!mirror && !turned) continue;
            // the flash as (mirrored?, clockwise angle), whatever orient code it was written with
            bool m = p.mirror == 9 || (p.mirror >= 4 && p.mirror <= 7);
            double ang = (p.mirror == 8 || p.mirror == 9) ? p.angle : 90.0 * (p.mirror % 4);
            if (mirror) { m = !m; ang = -ang; }   // mirroring a turned shape turns it the other way
            ang = std::fmod(ang + angle, 360.0);
            if (ang < 0) ang += 360.0;
            p.mirror = m ? 9 : 8;
            p.angle = ang;
        }
        for (auto& l : f.lines) { move(l.x1, l.y1); move(l.x2, l.y2); }
        for (auto& t : f.texts) {  // the record follows its strokes (which are in `lines`)
            move(t.x, t.y);
            if (scaled) { t.xsize *= sx; t.ysize *= sy; }
            if (mirror) { t.mirror = !t.mirror; t.angle = -t.angle; }
            t.angle = std::fmod(t.angle + angle, 360.0);
            if (t.angle < 0) t.angle += 360.0;
        }
        for (auto& arc : f.arcs) {
            move(arc.xs, arc.ys); move(arc.xe, arc.ye); move(arc.xc, arc.yc);
            if (mirror) arc.clockwise = !arc.clockwise;
        }
        for (auto& srf : f.surfaces)
            for (auto& c : srf.contours)
                for (auto& p : c.points) {
                    move(p.x, p.y);
                    if (p.arc) { move(p.xc, p.yc); if (mirror) p.cw = !p.cw; }
                }
        design.report.push_back("info: layer " + name + " drawn with its output transform (.out_x_scale " + fmtNum(sx, 6) +
                                ", .out_y_scale " + fmtNum(sy, 6) + ", .out_mirror " + (mirror ? "yes" : "no") +
                                ", .out_angle " + fmtNum(angle) + ")" +
                                (scaled && !even ? "; the scale is uneven, so symbols keep their size" : ""));
    }
}

void applyStepOrigin(OdbDesign& design) {
    const double ox = design.stepHeader.xOrigin, oy = design.stepHeader.yOrigin;
    if (ox == 0 && oy == 0) return;
    auto moveContours = [&](std::vector<OdbSurface>& surfaces) {
        for (auto& srf : surfaces)
            for (auto& c : srf.contours)
                for (auto& p : c.points) { p.x += ox; p.y += oy; if (p.arc) { p.xc += ox; p.yc += oy; } }
    };
    for (auto& [name, f] : design.layerFeatures) {
        for (auto& p : f.pads) { p.x += ox; p.y += oy; }
        for (auto& l : f.lines) { l.x1 += ox; l.y1 += oy; l.x2 += ox; l.y2 += oy; }
        for (auto& a : f.arcs) { a.xs += ox; a.ys += oy; a.xe += ox; a.ye += oy; a.xc += ox; a.yc += oy; }
        for (auto& t : f.texts) { t.x += ox; t.y += oy; }
        moveContours(f.surfaces);
    }
    moveContours(design.profile.surfaces);
    std::vector<OdbComponents*> componentSets = {&design.topComponents, &design.botComponents};
    for (auto& cl : design.otherComponents) componentSets.push_back(&cl.components);
    for (auto* comps : componentSets)
        for (auto& c : comps->components) {
            c.x += ox; c.y += oy;
            for (auto& t : c.terminals) { t.x += ox; t.y += oy; }  // TOP records are board coordinates
        }
    for (auto& pt : design.netlist.points) { pt.x += ox; pt.y += oy; }
    for (auto& r : design.stepHeader.repeats) { r.x += ox; r.y += oy; }  // where the placed steps go
    // applied: a second call must not move the step again
    design.stepHeader.xOrigin = design.stepHeader.yOrigin = 0;
    design.stepHeader.originApplied = true;
    design.stepHeader.appliedX = ox; design.stepHeader.appliedY = oy;
}

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design) {
    std::string error;
    if (parseOdbDirectory(rootDir, design, error)) return true;
    std::cerr << "Error: " << error << "\n";
    return false;
}

// Job-level files, shared by every step: misc/info, misc/attrlist, matrix/matrix, symbols/*
static bool readJobCommon(const std::string& rootDir, OdbDesign& design, std::string& error) {
    if (!dirExists(rootDir)) {
        error = "no-input: " + rootDir + " is not a folder (an archive must be unpacked first)";
        return false;
    }
    // 1. Parse misc/info
    std::string infoPath = rootDir + "/misc/info";
    if (odbFileExists(infoPath)) {
        if (!parseInfo(infoPath, design.info)) {
            std::cerr << "Warning: failed to parse " << infoPath << "\n";
        }
    }
    if (design.info.jobName.empty()) {  // older jobs keep the name in misc/job_name
        std::ifstream jn(rootDir + "/misc/job_name");
        std::string name;
        if (jn.is_open() && std::getline(jn, name)) design.info.jobName = trim(name);
    }
    if (odbFileExists(rootDir + "/misc/attrlist")) parseAttrList(rootDir + "/misc/attrlist", design.jobAttrs);

    // 2. Parse matrix/matrix
    std::string matrixPath = rootDir + "/matrix/matrix";
    if (!odbFileExists(matrixPath)) {
        error = "not-odb: no matrix/matrix file; the input is not an ODB++ job folder (or the archive "
                "holds it under another name)";
        return false;
    }
    if (isOdxXml(matrixPath)) {
        error = "bad-matrix: matrix/matrix is ODB++ XML (ODX); only the text form of ODB++ is read";
        return false;
    }
    if (!parseMatrix(matrixPath, design.steps, design.layers)) {
        error = "bad-matrix: matrix/matrix could not be read";
        return false;
    }
    if (design.layers.empty()) {
        error = "bad-matrix: matrix/matrix lists no LAYER records";
        return false;
    }
    if (!design.steps.empty()) design.stepName = design.steps.front().name;
    checkJob(design);

    // User symbols: symbols/<name>/features, drawn wherever a layer flashes <name>
    std::string symbolsDir = rootDir + "/symbols";
    for (auto& symName : listDirs(symbolsDir)) {
        std::string featPath = symbolsDir + "/" + symName + "/features";
        OdbFeatures feats;
        if (odbFileExists(featPath) && parseFeatures(featPath, feats))
            design.userSymbols[toLower(symName)] = std::move(feats);
    }
    return true;
}

// The step folders of a job, in the order they are converted: the matrix's STEP records first
// (by COL, each matched to its folder without regard to case), then folders the matrix does not
// name, by name. .first is the step's name (the matrix's, else the folder's), .second the folder.
static std::vector<std::pair<std::string, std::string>> stepFolders(const std::string& rootDir,
                                                                    const std::vector<OdbMatrixStep>& matrixSteps) {
    std::vector<std::pair<std::string, std::string>> out;
    auto dirs = listDirs(rootDir + "/steps");
    std::sort(dirs.begin(), dirs.end());
    std::vector<bool> used(dirs.size(), false);
    std::vector<OdbMatrixStep> ordered = matrixSteps;
    std::stable_sort(ordered.begin(), ordered.end(), [](const OdbMatrixStep& a, const OdbMatrixStep& b) { return a.col < b.col; });
    for (auto& st : ordered) {
        for (size_t i = 0; i < dirs.size(); i++) {
            if (used[i] || toLower(dirs[i]) != toLower(st.name)) continue;
            used[i] = true;
            out.push_back({st.name, dirs[i]});
            break;
        }
    }
    for (size_t i = 0; i < dirs.size(); i++)
        if (!used[i]) out.push_back({dirs[i], dirs[i]});
    return out;
}

// One step of a job whose common files are already in `design`
static bool readStep(const std::string& rootDir, const std::string& stepDirName, OdbDesign& design, std::string& error) {
    std::string actualStepDir = rootDir + "/steps/" + stepDirName;

    // 3. Parse stephdr
    std::string stephdrPath = actualStepDir + "/stephdr";
    if (odbFileExists(actualStepDir + "/attrlist")) parseAttrList(actualStepDir + "/attrlist", design.stepAttrs);
    if (odbFileExists(stephdrPath)) {
        parseStepHeader(stephdrPath, design.stepHeader);
    }

    // 4. Parse profile
    std::string profilePath = actualStepDir + "/profile";
    if (odbFileExists(profilePath)) {
        if (!parseProfile(profilePath, design.profile)) {
            std::cerr << "Warning: failed to parse profile\n";
        }
    }

    // 5. Parse layer features
    std::string layersDir = actualStepDir + "/layers";
    auto layerDirs = listDirs(layersDir);
    std::sort(layerDirs.begin(), layerDirs.end());
    for (auto& layerName : layerDirs) {
        std::string featPath = layersDir + "/" + layerName + "/features";
        if (odbFileExists(featPath)) {
            OdbFeatures feats;
            if (parseFeatures(featPath, feats)) {
                design.layerFeatures[toLower(layerName)] = std::move(feats);
            }
        }
        std::string attrPath = layersDir + "/" + layerName + "/attrlist";
        if (odbFileExists(attrPath)) {
            OdbAttrList attrs;
            if (parseAttrList(attrPath, attrs)) design.layerAttrs[toLower(layerName)] = std::move(attrs);
        }
        // drill (and rout) layers: the tools table, when the layer has one
        std::string toolsPath = layersDir + "/" + layerName + "/tools";
        if (odbFileExists(toolsPath)) {
            OdbDrillTools tools;
            if (parseDrillTools(toolsPath, tools)) design.layerTools[toLower(layerName)] = std::move(tools);
        }
    }

    // Fonts: only the ones a text of this step names (fonts/<name>, any case)
    for (auto& [lname, f] : design.layerFeatures) {
        for (auto& t : f.texts) {
            std::string want = toLower(t.font);
            if (design.fonts.count(want)) continue;
            for (auto entry : listFiles(rootDir + "/fonts")) {
                std::string lower = toLower(entry);
                if (lower.size() > 2 && lower.compare(lower.size() - 2, 2, ".z") == 0) {  // a compressed font
                    lower.resize(lower.size() - 2);
                    entry.resize(entry.size() - 2);
                }
                if (lower != want) continue;
                OdbFont font;
                std::vector<int> badLines;
                bool ok = parseFont(rootDir + "/fonts/" + entry, font, &badLines);
                if (!badLines.empty())
                    design.report.push_back("warning: error while reading font file - fonts/" + entry + ", line - " +
                                            std::to_string(badLines.front()) +
                                            (badLines.size() > 1 ? " (and " + std::to_string(badLines.size() - 1) + " more); those lines are skipped"
                                                                 : "; the line is skipped"));
                if (ok) design.fonts[want] = std::move(font);
                else design.report.push_back("warning: fonts/" + entry + " has no XSIZE / YSIZE: not usable as a font");
                break;
            }
        }
    }

    // a panel frame with no layer data of its own still holds the steps it places
    if (design.layerFeatures.empty() && design.stepHeader.repeats.empty()) {
        error = "no-layers: no layer has a features file under " + actualStepDir.substr(rootDir.size() + 1) +
                "/layers (" + std::to_string(design.layers.size()) + " layers in the matrix)";
        return false;
    }

    // 6. Parse components: comp_+_top and comp_+_bot, and any other COMPONENT row of the matrix
    // (a rigid-flex job mounts parts on inner faces: comp_+_left / comp_+_right, ...). Such a row
    // is a top side when it lies above the first copper row, else a bottom side.
    auto layerFolder = [&](const std::string& lower) -> std::string {
        for (auto& d : layerDirs)
            if (toLower(d) == lower) return layersDir + "/" + d;
        return "";
    };
    auto readComponents = [&](const std::string& lower, OdbComponents& into) {
        std::string folder = layerFolder(lower);
        if (folder.empty() || !odbFileExists(folder + "/components")) return false;
        return parseComponents(folder + "/components", into);
    };
    readComponents("comp_+_top", design.topComponents);
    readComponents("comp_+_bot", design.botComponents);
    {
        int firstCopper = -1;
        for (auto& l : design.layers) {
            std::string t = toUpper(l.type);
            if (toUpper(l.context) != "MISC" && (t == "SIGNAL" || t == "POWER_GROUND" || t == "MIXED") &&
                (firstCopper < 0 || l.row < firstCopper))
                firstCopper = l.row;
        }
        for (auto& l : design.layers) {
            std::string lower = toLower(l.name);
            if (toUpper(l.type) != "COMPONENT" || lower == "comp_+_top" || lower == "comp_+_bot") continue;
            OdbComponentLayer cl;
            cl.name = lower;
            cl.bottom = firstCopper >= 0 && l.row > firstCopper;
            if (!readComponents(lower, cl.components)) continue;
            design.report.push_back("info: components of layer " + lower + " (" + std::to_string(cl.components.components.size()) +
                                    ") are placed on the " + (cl.bottom ? "bottom" : "top") + " side");
            design.otherComponents.push_back(std::move(cl));
        }
    }

    // 7. Parse eda/data
    std::string edaPath = actualStepDir + "/eda/data";
    if (odbFileExists(edaPath)) {
        parseEdaData(edaPath, design.edaData);
    }

    // 8. Parse netlist: cadnet (the CAD system's own), else the first other netlists/<src>
    std::string netlistPath = actualStepDir + "/netlists/cadnet/netlist";
    if (!odbFileExists(netlistPath)) {
        auto sources = listDirs(actualStepDir + "/netlists");
        std::sort(sources.begin(), sources.end());
        for (auto& src : sources) {
            std::string p = actualStepDir + "/netlists/" + src + "/netlist";
            if (odbFileExists(p)) { netlistPath = p; break; }
        }
    }
    if (odbFileExists(netlistPath)) {
        parseNetlist(netlistPath, design.netlist);
    }

    applyProducerQuirks(design);
    normalizeUnits(design);
    computeBoardThickness(design);
    return true;
}

std::vector<std::string> listOdbSteps(const std::string& rootDir) {
    std::vector<std::string> names;
    std::vector<OdbMatrixStep> steps;
    std::vector<OdbLayer> layers;
    parseMatrix(rootDir + "/matrix/matrix", steps, layers);
    for (auto& [name, dir] : stepFolders(rootDir, steps)) names.push_back(name);
    return names;
}

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design, std::string& error) {
    return parseOdbStep(rootDir, "", design, error);
}

bool parseOdbStep(const std::string& rootDir, const std::string& stepName, OdbDesign& design, std::string& error) {
    if (!readJobCommon(rootDir, design, error)) return false;
    auto folders = stepFolders(rootDir, design.steps);
    if (folders.empty()) {
        error = "no-step: no step folder under steps/ (the matrix names step '" + design.stepName + "')";
        return false;
    }
    // the named step, else the first: the matrix's first STEP that has a folder, else the first folder
    size_t pick = 0;
    if (!stepName.empty()) {
        pick = folders.size();
        for (size_t i = 0; i < folders.size(); i++)
            if (toLower(folders[i].first) == toLower(stepName) || toLower(folders[i].second) == toLower(stepName)) { pick = i; break; }
        if (pick == folders.size()) {
            std::string have;
            for (auto& f : folders) have += (have.empty() ? "" : ", ") + f.first;
            error = "no-step: the job has no step '" + stepName + "' (it has: " + have + ")";
            return false;
        }
    }
    design.stepName = folders[pick].first;
    if (folders.size() > 1) {
        std::string all;
        for (auto& f : folders) all += (all.empty() ? "" : ", ") + f.first;
        design.report.push_back("info: the job has " + std::to_string(folders.size()) + " steps (" + all + "); this is step " +
                                design.stepName);
    }
    return readStep(rootDir, folders[pick].second, design, error);
}

bool parseOdbJob(const std::string& rootDir, std::vector<OdbDesign>& designs, std::string& error) {
    OdbDesign common;
    if (!readJobCommon(rootDir, common, error)) return false;
    auto folders = stepFolders(rootDir, common.steps);
    if (folders.empty()) {
        error = "no-step: no step folder under steps/ (the matrix names step '" + common.stepName + "')";
        return false;
    }
    std::vector<std::string> skipped;
    std::string firstError;
    for (auto& [name, dir] : folders) {
        OdbDesign d = common;
        d.stepName = name;
        std::string why;
        if (readStep(rootDir, dir, d, why)) {
            designs.push_back(std::move(d));
        } else {
            if (firstError.empty()) firstError = why;
            skipped.push_back("warning: step " + name + " was not converted (" + why + ")");
        }
    }
    if (designs.empty()) {
        error = firstError;
        return false;
    }
    // an empty step (a panel frame with no layer data) is skipped, and the job still converts
    designs.front().report.insert(designs.front().report.end(), skipped.begin(), skipped.end());
    return true;
}

} // namespace odb2kicad
