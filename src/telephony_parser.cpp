#include "telephony_parser.h"

static std::string getStr(const json& c,
                          std::initializer_list<const char*> keys,
                          const std::string& def = "") {
    for (const char* k : keys) {
        if (!c.contains(k)) continue;
        const auto& v = c[k];
        try {
            if (v.is_string())         return v.get<std::string>();
            if (v.is_number_integer()) return std::to_string(v.get<int>());
            if (v.is_number_float())   return std::to_string(v.get<double>());
        } catch (...) {}
    }
    return def;
}

static int getInt(const json& c,
                  std::initializer_list<const char*> keys, int def = 0) {
    for (const char* k : keys) {
        if (!c.contains(k)) continue;
        const auto& v = c[k];
        try {
            if (v.is_number_integer()) return v.get<int>();
            if (v.is_number_float())   return static_cast<int>(v.get<double>());
            if (v.is_string()) {
                std::string s = v.get<std::string>();
                if (!s.empty()) return std::stoi(s);
            }
        } catch (...) {}
    }
    return def;
}

static bool getBool(const json& c,
                    std::initializer_list<const char*> keys, bool def = false) {
    for (const char* k : keys) {
        if (!c.contains(k)) continue;
        const auto& v = c[k];
        try {
            if (v.is_boolean())        return v.get<bool>();
            if (v.is_number_integer()) return v.get<int>() != 0;
        } catch (...) {}
    }
    return def;
}

CellRecord TelephonyParser::parseCellJson(const json& c) {
    CellRecord r;

    r.isRegistered = getBool(c, {"isRegistered"});
    r.timingAdvance = getInt(c, {"timingAdvance", "Timing Advance"});
    r.pci = getInt(c, {"pci", "PCI"}, -1);
    r.mcc = getStr(c, {"mcc", "MCC"});
    r.mnc = getStr(c, {"mnc", "MNC"});

    std::string type;
    try {
        if (c.contains("type") && c["type"].is_string()) type = c["type"].get<std::string>();
        if (c.contains("Type") && c["Type"].is_string()) type = c["Type"].get<std::string>();
    } catch (...) {}

    if (type == "LTE" || type == "CellInfoLte") {
        r.standard = "LTE";
        r.rsrp = getInt(c, {"rsrp", "RSRP"}, -140);
        r.rssi = getInt(c, {"rssi", "RSSI"}, 0);
        r.rsrq = getInt(c, {"rsrq", "RSRQ"}, 0);
        r.sinr = getInt(c, {"sinr", "rssnr", "RSSNR"}, 0);
        r.earfcn = getInt(c, {"earfcn", "EARFCN"}, -1);
        r.band = getInt(c, {"band", "Band"}, -1);
        r.tac = getStr(c, {"tac", "TAC"});
        r.cellId = getStr(c, {"cellId", "CellIdentity"});
        r.signalStrength = r.rsrp;
    } else if (type == "GSM" || type == "CellInfoGsm") {
        r.standard = "GSM";
        r.arfcn = getInt(c, {"arfcn", "ARFCN"}, -1);
        r.bsic = getInt(c, {"bsic", "BSIC"}, -1);
        r.dbm = getInt(c, {"dbm", "DBM"}, -140);
        r.rssi = getInt(c, {"rssi", "RSSI"}, 0);
        r.lac = getStr(c, {"lac", "LAC"});
        r.cellId = getStr(c, {"cellId", "CellIdentity"});
        r.signalStrength = r.dbm;
    } else if (type == "NR" || type == "5G_NR" || type == "CellInfoNr") {
        r.standard = "5G_NR";
        r.nrarfcn = getInt(c, {"nrarfcn", "NRARFCN"}, -1);
        r.band = getInt(c, {"band", "Band"}, -1);
        r.tac = getStr(c, {"tac", "TAC"});
        r.nci = getStr(c, {"nci", "NCI"});
        r.ssRsrp = getInt(c, {"ssRsrp", "SSRsrp"}, -140);
        r.ssRsrq = getInt(c, {"ssRsrq", "SSRsrq"}, 0);
        r.ssSinr = getInt(c, {"ssSinr", "SSSinr"}, 0);
        r.rsrp = r.ssRsrp;
        r.sinr = r.ssSinr;
        r.signalStrength = r.ssRsrp;
    }
    return r;
}

std::vector<CellRecord> TelephonyParser::parseTelephonyArray(const json& telephony) {
    std::vector<CellRecord> cells;
    if (telephony.is_array()) {
        for (auto& c : telephony) cells.push_back(parseCellJson(c));
    } else if (telephony.is_object()) {
        cells.push_back(parseCellJson(telephony));
    }
    return cells;
}