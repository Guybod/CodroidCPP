#include "codroid/client.hpp"

namespace Codroid {

bool CodroidClient::ConnectRemoteAndSwitchOn(const std::string& ip, int port, std::string local_ip) {
    if (!connect(ip, port, std::move(local_ip)))
        return false;
    Response r = switchOn(1);
    return r.error_msg.empty();
}

std::map<std::string, Variable> CodroidClient::GetGlobalVarsCatalog(int id) {
    Response r = getGlobalVars(id);
    if (!r.error_msg.empty())
        throw CodroidException("GetGlobalVars failed: " + r.error_msg);
    if (!r.db.is_object())
        throw CodroidException("GetGlobalVars: db is not a JSON object");

    std::map<std::string, Variable> out;
    for (auto it = r.db.begin(); it != r.db.end(); ++it) {
        const std::string& name = it.key();
        const json& v = it.value();
        Variable var;
        if (v.contains("val") && !v["val"].is_null()) {
            if (v["val"].is_string())
                var.val = v["val"].get<std::string>();
            else
                var.val = v["val"].dump();
        }
        if (v.contains("nm") && !v["nm"].is_null()) {
            if (v["nm"].is_string())
                var.nm = v["nm"].get<std::string>();
            else
                var.nm = v["nm"].dump();
        }
        out[name] = std::move(var);
    }
    return out;
}

Response CodroidClient::SaveGlobalVar(const std::string& name, const Variable& value, int id) {
    std::map<std::string, Variable> m;
    m[name] = value;
    return saveGlobalVars(m, id);
}

} // namespace Codroid
