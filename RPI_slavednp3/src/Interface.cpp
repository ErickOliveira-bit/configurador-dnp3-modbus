#include "Tipos.h"

#ifdef LIBMODBUS_MASTER
void PrintMappings()
{
    LogLine(AppLogLevel::Info, "MAP", "==== PONTOS DE LEITURA ====");
    for (const auto& p : g_profile.read_points) {
        std::ostringstream msg;
        msg << "DNP3=" << Dnp3PointTypeLabel(p.type) << " " << p.dnp3_analog_index
            << " ip=" << p.ip << ":" << p.port
            << " id=" << p.slave_id
            << " tipo=" << ReadValueTypeToString(p.type)
            << " count=" << p.count
            << " addr=" << p.address
            << " desc=\"" << p.description << "\"";
        LogLine(AppLogLevel::Info, "READ", msg.str());
    }

    LogLine(AppLogLevel::Info, "MAP", "==== COMANDOS ANALOGICOS ====");
    for (const auto& c : g_profile.analog_commands) {
        std::ostringstream msg;
        msg << "IDX=" << c.dnp3_index
            << " ip=" << c.ip << ":" << c.port
            << " id=" << c.slave_id
            << " target=" << WriteTargetTypeToString(c.target)
            << " tipo=" << ReadValueTypeToString(c.value_type)
            << " addr=" << c.modbus_address
            << " desc=\"" << c.description << "\"";
        LogLine(AppLogLevel::Info, "AO", msg.str());
    }

    LogLine(AppLogLevel::Info, "MAP", "==== COMANDOS BINARIOS ====");
    for (const auto& c : g_profile.binary_commands) {
        std::ostringstream msg;
        msg << "IDX=" << c.dnp3_index
            << " ip=" << c.ip << ":" << c.port
            << " id=" << c.slave_id
            << " target=" << WriteTargetTypeToString(c.target)
            << " tipo=" << ReadValueTypeToString(c.value_type)
            << " addr=" << c.modbus_address
            << " desc=\"" << c.description << "\"";
        LogLine(AppLogLevel::Info, "CROB", msg.str());
    }
}

string TrimCopy(const string& input)
{
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

string JsonEscape(const string& s)
{
    std::ostringstream oss;
    for (char ch : s) {
        switch (ch) {
        case '"': oss << "\\\""; break;
        case '\\': oss << "\\\\"; break;
        case '\b': oss << "\\b"; break;
        case '\f': oss << "\\f"; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)ch << std::dec;
            } else {
                oss << ch;
            }
        }
    }
    return oss.str();
}

string UrlDecode(const string& s)
{
    string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            const string hex = s.substr(i + 1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            out.push_back(ch);
            i += 2;
        } else if (s[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

string BuildStateJson(const vector<ReadPointState>& point_states)
{
    lock_guard<mutex> data_lock(data_mutex);
    lock_guard<mutex> log_lock(log_mutex);

    std::ostringstream oss;
    oss << "{";
    oss << "\"mode\":\"" << JsonEscape(OperatingModeToString(g_profile.mode)) << "\",";
    oss << "\"profile\":\"" << JsonEscape(g_profile.name) << "\",";
    oss << "\"read_points\":[";
    for (size_t i = 0; i < g_profile.read_points.size(); ++i) {
        const auto& p = g_profile.read_points[i];
        const auto& s = point_states[i];
        if (i) oss << ",";
        oss << "{";
        oss << "\"description\":\"" << JsonEscape(p.description) << "\",";
        oss << "\"ip\":\"" << JsonEscape(p.ip) << "\",";
        oss << "\"port\":" << p.port << ",";
        oss << "\"slave_id\":" << p.slave_id << ",";
        oss << "\"type\":\"" << JsonEscape(ReadValueTypeToString(p.type)) << "\",";
        oss << "\"register_kind\":\"" << JsonEscape(ReadValueTypeRegisterKind(p.type)) << "\",";
        oss << "\"data_type\":\"" << JsonEscape(ReadValueTypeDataType(p.type)) << "\",";
        oss << "\"address\":" << p.address << ",";
        oss << "\"count\":" << p.count << ",";
        oss << "\"dnp_type\":\"" << JsonEscape(Dnp3PointTypeLabel(p.type)) << "\",";
        oss << "\"dnp_index\":" << p.dnp3_analog_index << ",";
        oss << "\"dnp_object\":" << Dnp3ObjectGroup(p.type) << ",";
        oss << "\"dnp_variation\":" << Dnp3StaticVariationNumber(p.type) << ",";
        oss << "\"value\":" << s.analog_value << ",";
        oss << "\"connected\":" << (s.connection_status ? "true" : "false") << ",";
        oss << "\"failures\":" << s.failure_count;
        oss << "}";
    }
    oss << "],";

    oss << "\"analog_commands\":[";
    for (size_t i = 0; i < g_profile.analog_commands.size(); ++i) {
        const auto& c = g_profile.analog_commands[i];
        if (i) oss << ",";
        oss << "{";
        oss << "\"index\":" << c.dnp3_index << ",";
        oss << "\"ip\":\"" << JsonEscape(c.ip) << "\",";
        oss << "\"port\":" << c.port << ",";
        oss << "\"slave_id\":" << c.slave_id << ",";
        oss << "\"target\":\"" << JsonEscape(WriteTargetTypeToString(c.target)) << "\",";
        oss << "\"address\":" << c.modbus_address << ",";
        oss << "\"description\":\"" << JsonEscape(c.description) << "\"";
        oss << "}";
    }
    oss << "],";

    oss << "\"binary_commands\":[";
    for (size_t i = 0; i < g_profile.binary_commands.size(); ++i) {
        const auto& c = g_profile.binary_commands[i];
        if (i) oss << ",";
        oss << "{";
        oss << "\"index\":" << c.dnp3_index << ",";
        oss << "\"ip\":\"" << JsonEscape(c.ip) << "\",";
        oss << "\"port\":" << c.port << ",";
        oss << "\"slave_id\":" << c.slave_id << ",";
        oss << "\"target\":\"" << JsonEscape(WriteTargetTypeToString(c.target)) << "\",";
        oss << "\"address\":" << c.modbus_address << ",";
        oss << "\"description\":\"" << JsonEscape(c.description) << "\"";
        oss << "}";
    }
    oss << "],";

    oss << "\"logs\":[";
    for (size_t i = 0; i < g_recent_logs.size(); ++i) {
        const auto& l = g_recent_logs[i];
        if (i) oss << ",";
        oss << "{";
        oss << "\"time\":\"" << JsonEscape(l.time) << "\",";
        oss << "\"level\":\"" << JsonEscape(l.level) << "\",";
        oss << "\"scope\":\"" << JsonEscape(l.scope) << "\",";
        oss << "\"text\":\"" << JsonEscape(l.text) << "\"";
        oss << "}";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

void PrintConsoleHelp();
void PrintMappings();

bool ExecuteCommandLine(const string& raw_line)
{
    const string line = TrimCopy(raw_line);
    if (line.empty()) return true;

    if (line == "help") {
        PrintConsoleHelp();
        return true;
    }

    if (line == "show") {
        PrintMappings();
        return true;
    }

    if (line == "quit" || line == "exit") {
        g_running.store(false);
        LogLine(AppLogLevel::Info, "CMD", "Encerramento solicitado pelo console.");
        return true;
    }

    LogLine(AppLogLevel::Warn, "CMD", "Comando desconhecido. Use: help, show ou quit.");
    return false;
}


#ifdef __linux__

string GetQueryParam(const string& path, const string& key);

string HtmlEscape(const string& s)
{
    string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}


string BuildCsvLineFromForm(const string& path)
{
    const string name = GetQueryParam(path, "name");
    const string ip = GetQueryParam(path, "ip");
    const string port = GetQueryParam(path, "port");
    const string slave_id = GetQueryParam(path, "slave_id");
    const string reg = GetQueryParam(path, "reg");
    const string address = GetQueryParam(path, "address");
    string data_type = GetQueryParam(path, "data_type");
    const string dnp_index = GetQueryParam(path, "dnp_index");

    if (name.empty() || ip.empty() || port.empty() || slave_id.empty() ||
        reg.empty() || address.empty() || data_type.empty() || dnp_index.empty()) {
        return "";
    }

    // Coil Status e Input Status são pontos discretos/booleanos.
    // Mesmo que o navegador envie outro valor por engano, salvamos como bool.
    const string reg_norm = NormalizeToken(reg);
    if (reg_norm == "coil_status" || reg_norm == "coil" ||
        reg_norm == "input_status" || reg_norm == "discrete_input") {
        data_type = "bool";
    }

    return name + ";" + ip + ";" + port + ";" + slave_id + ";" + reg + ";" + address + ";" + data_type + ";" + dnp_index;
}

bool IsSameConfiguredPoint(const ReadPointConfig& a, const ReadPointConfig& b)
{
    return std::string(a.description) == std::string(b.description) &&
           std::string(a.ip) == std::string(b.ip) &&
           a.port == b.port &&
           a.slave_id == b.slave_id &&
           a.type == b.type &&
           a.address == b.address &&
           a.dnp3_analog_index == b.dnp3_analog_index;
}

bool AppendPointToConfigFileFromLine(const string& csv_line, const string& file_path, string& error)
{
    ReadPointConfig tmp;
    if (!TryBuildReadPointFromCsvLine(csv_line, tmp, error)) {
        return false;
    }

    const auto existing_points = LoadConfiguredPointsFromCsvForUi(file_path);
    for (const auto& existing : existing_points) {
        if (IsSameConfiguredPoint(existing, tmp)) {
            error = "Este ponto já existe no arquivo. Ele não foi duplicado.";
            return false;
        }
        if (PublishesAsDnp3Binary(existing.type) == PublishesAsDnp3Binary(tmp.type) &&
            existing.dnp3_analog_index == tmp.dnp3_analog_index) {
            error = "Já existe um ponto usando o mesmo tipo e índice DNP3. Altere o índice antes de salvar.";
            return false;
        }
    }

    std::ofstream out(file_path, std::ios::app);
    if (!out.is_open()) {
        error = "Não foi possível abrir o arquivo " + file_path + " para escrita.";
        return false;
    }

    out << csv_line << "\n";
    return true;
}

bool RemovePointFromConfigFileByIndex(size_t remove_index, const string& file_path, string& error)
{
    const auto configured_points = LoadConfiguredPointsFromCsvForUi(file_path);
    if (remove_index >= configured_points.size()) {
        error = "Índice de ponto inválido.";
        return false;
    }

    ProfileConfig updated_profile;
    updated_profile.mode = OperatingMode::ConfigFile;
    updated_profile.name = "Configuração Modbus -> DNP3: " + file_path;

    for (size_t i = 0; i < configured_points.size(); ++i) {
        if (i != remove_index) {
            updated_profile.read_points.push_back(configured_points[i]);
        }
    }

    if (!SaveProfileReadPointsToCsv(updated_profile, file_path)) {
        error = "Não foi possível regravar o arquivo " + file_path + ".";
        return false;
    }

    return true;
}

string BuildHtmlPageServerRendered(const vector<ReadPointState>& point_states, const string& notice)
{
    lock_guard<mutex> data_lock(data_mutex);
    lock_guard<mutex> log_lock(log_mutex);

    std::ostringstream oss;
    oss << "<!doctype html><html lang=\"pt-br\"><head>"
        << "<meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        << "<title>Configurador de Pontos de Comunicação</title>"
        << "<style>"
        << "body{font-family:Arial,sans-serif;background:#f6f7fb;margin:0;color:#1f2937}"
        << "header{padding:16px 20px;background:#111827;color:white}"
        << "main{padding:20px;max-width:1280px;margin:auto}"
        << ".row{display:grid;grid-template-columns:1.2fr 1fr;gap:16px}"
        << ".card{background:white;border-radius:16px;padding:16px;box-shadow:0 1px 3px rgba(0,0,0,.08);margin-bottom:16px}"
        << ".title{font-size:18px;font-weight:700;margin-bottom:12px}"
        << ".small{font-size:13px;color:#6b7280}"
        << ".ok{display:inline-block;padding:6px 10px;border-radius:999px;background:#dcfce7;font-size:12px;margin-right:8px}"
        << ".bad{display:inline-block;padding:6px 10px;border-radius:999px;background:#fee2e2;font-size:12px;margin-right:8px}"
        << ".warn{display:inline-block;padding:6px 10px;border-radius:999px;background:#fef3c7;font-size:12px;margin-right:8px}"
        << ".grid{display:grid;gap:12px}.grid3{grid-template-columns:repeat(3,1fr)}.grid2{grid-template-columns:repeat(2,1fr)}"
        << "table{width:100%;border-collapse:collapse;font-size:14px}th,td{padding:8px;border-bottom:1px solid #e5e7eb;text-align:left;vertical-align:top}"
        << "input,select,button{padding:10px 12px;border-radius:10px;border:1px solid #d1d5db;font-size:14px}"
        << "button{cursor:pointer;background:#111827;color:#fff;border:none}button.secondary{background:#4b5563}"
        << ".logs{max-height:420px;overflow:auto}.log{border:1px solid #e5e7eb;border-radius:12px;padding:10px;margin-bottom:8px;background:#fff}"
        << ".statusbox{padding:12px;border-radius:12px;background:#f9fafb}.mono{font-family:ui-monospace,SFMono-Regular,Menlo,monospace}"
        << ".formgrid{display:grid;grid-template-columns:1.2fr .8fr .8fr .8fr auto;gap:8px;align-items:end}"
        << ".notice{padding:12px;border-radius:12px;background:#e0f2fe;border:1px solid #bae6fd;margin-bottom:16px}"
        << ".refresh-pill{display:inline-block;padding:6px 10px;border-radius:999px;background:#dbeafe;font-size:12px;margin-left:8px}"
        << "@media (max-width:980px){.row,.grid3,.grid2,.formgrid{grid-template-columns:1fr}}"
        << "</style>"
        << "<script>"
        << "const FORM_KEY='dnp3_modbus_form_state_v3';const SCROLL_KEY='dnp3_modbus_scroll_y_v3';const AUTO_KEY='dnp3_modbus_auto_refresh_v3';let __refreshTimer=null;"
        << "function __setRefreshStatus(txt){const el=document.getElementById('refreshStatus');if(!el)return;el.textContent=txt;}"
        << "function __syncRefreshButtons(){const enabled=localStorage.getItem(AUTO_KEY)==='1';const start=document.getElementById('startRefreshBtn');const stop=document.getElementById('stopRefreshBtn');if(start)start.disabled=enabled;if(stop)stop.disabled=!enabled;__setRefreshStatus(enabled?'Atualização automática ligada (2s)':'Atualização automática parada');}"
        << "function __startRefresh(){try{localStorage.setItem(AUTO_KEY,'1');}catch(e){}if(__refreshTimer)clearInterval(__refreshTimer);__syncRefreshButtons();__refreshTimer=setInterval(function(){if(localStorage.getItem(AUTO_KEY)==='1'){__saveFormState();window.location.href='/';}},2000);}"
        << "function __stopRefresh(){try{localStorage.setItem(AUTO_KEY,'0');}catch(e){}if(__refreshTimer)clearInterval(__refreshTimer);__refreshTimer=null;__syncRefreshButtons();}"
        << "function __syncModbusDataType(){const reg=document.getElementById('modbusRegType');const dt=document.getElementById('modbusDataType');if(!reg||!dt)return;const bit=(reg.value==='coil_status'||reg.value==='input_status');Array.from(dt.options).forEach(function(o){if(bit){o.hidden=(o.value!=='bool');}else{o.hidden=(o.value==='bool');}});if(bit){dt.value='bool';dt.disabled=true;}else{dt.disabled=false;if(dt.value==='bool')dt.value='int16';}__updateDnp3Suggestion();}"
        << "function __updateDnp3Suggestion(){const reg=document.getElementById('modbusRegType');const dt=document.getElementById('modbusDataType');if(!reg||!dt)return;const bit=(reg.value==='coil_status'||reg.value==='input_status');const data=dt.value;let tipo=bit?'Binary Input':'Analog Input';let obj=bit?'1':'30';let variation='2';let classe='Class 1';let desc=bit?'Entrada binária DNP3 para ponto discreto/booleano':'Entrada analógica DNP3 inteira de 16 bits';if(!bit&&data==='float32'){variation='5';desc='Entrada analógica DNP3 em ponto flutuante de 32 bits';}else if(!bit&&(data==='int32'||data==='uint32')){variation='1';desc='Entrada analógica DNP3 inteira de 32 bits';}const set=(id,val)=>{const el=document.getElementById(id);if(el)el.textContent=val;};set('dnpSugType',tipo);set('dnpSugObj',obj);set('dnpSugVar',variation);set('dnpSugClass',classe);set('dnpSugDesc',desc);}"
        << "function __saveFormState(){const form=document.getElementById('pointForm');if(!form)return;const data={};Array.from(form.elements).forEach(function(el){if(!el.name)return;data[el.name]=el.value;});try{localStorage.setItem(FORM_KEY,JSON.stringify(data));localStorage.setItem(SCROLL_KEY,String(window.scrollY||0));}catch(e){}}"
        << "function __restoreFormState(){let raw=null;try{raw=localStorage.getItem(FORM_KEY);}catch(e){}if(!raw)return;let data={};try{data=JSON.parse(raw)||{};}catch(e){return;}const form=document.getElementById('pointForm');if(!form)return;Array.from(form.elements).forEach(function(el){if(!el.name)return;if(Object.prototype.hasOwnProperty.call(data,el.name)){el.value=data[el.name];}});__syncModbusDataType();let y=0;try{y=parseInt(localStorage.getItem(SCROLL_KEY)||'0',10)||0;}catch(e){}if(y>0){setTimeout(function(){window.scrollTo(0,y);},50);}}"
        << "window.addEventListener('DOMContentLoaded',function(){const reg=document.getElementById('modbusRegType');if(reg){reg.addEventListener('change',function(){__syncModbusDataType();__saveFormState();});}const dt=document.getElementById('modbusDataType');if(dt){dt.addEventListener('change',function(){__updateDnp3Suggestion();__saveFormState();});}document.querySelectorAll('#pointForm input,#pointForm select').forEach(function(el){el.addEventListener('input',__saveFormState);el.addEventListener('change',__saveFormState);});document.querySelectorAll('form').forEach(function(f){f.addEventListener('submit',function(){const dt=document.getElementById('modbusDataType');if(dt)dt.disabled=false;try{localStorage.removeItem(FORM_KEY);}catch(e){}});});const start=document.getElementById('startRefreshBtn');const stop=document.getElementById('stopRefreshBtn');if(start)start.addEventListener('click',__startRefresh);if(stop)stop.addEventListener('click',__stopRefresh);__restoreFormState();__syncModbusDataType();__updateDnp3Suggestion();if(localStorage.getItem(AUTO_KEY)==='1'){__startRefresh();}else{try{localStorage.setItem(AUTO_KEY,'0');}catch(e){}__syncRefreshButtons();}});"
        << "</script></head><body>";

    oss << "<header><div style=\"font-size:24px;font-weight:700\">Configurador de Pontos de Comunicação</div>";
    oss << "<div class=\"small\" style=\"margin-top:8px;display:flex;gap:8px;align-items:center;flex-wrap:wrap\">"
        << "<span id=\"refreshStatus\" class=\"refresh-pill\">Atualização automática parada</span>"
        << "<button type=\"button\" id=\"startRefreshBtn\" class=\"secondary\" style=\"padding:6px 10px;font-size:12px\">Iniciar atualização</button>"
        << "<button type=\"button\" id=\"stopRefreshBtn\" class=\"secondary\" style=\"padding:6px 10px;font-size:12px\">Parar atualização</button>"
        << "</div></header><main>";

    if (!notice.empty()) {
        oss << "<div class=\"notice\"><strong>Resultado:</strong> " << HtmlEscape(notice) << "</div>";
    }

    oss << "<div class=\"row\"><section>";

    oss << "<div class=\"card\"><div class=\"title\">Cadastrar ponto de comunicação</div>";
    oss << "<form id=\"pointForm\" method=\"GET\" action=\"/addpoint\" style=\"margin-top:12px\">";
    oss << "<div class=\"grid grid3\">";
    oss << "<div><div class=\"small\">Nome do ponto</div><input name=\"name\" value=\"Temperatura\"></div>";
    oss << "<div><div class=\"small\">Protocolo de origem</div><select name=\"source_protocol\"><option value=\"modbus_tcp\">Modbus TCP</option></select></div>";
    oss << "<div><div class=\"small\">Endereço IP do equipamento</div><input name=\"ip\" value=\"192.168.100.200\"></div>";
    oss << "<div><div class=\"small\">Porta TCP do equipamento</div><input name=\"port\" value=\"502\"></div>";
    oss << "<div><div class=\"small\">Endereço do escravo Modbus (Unit ID)</div><input name=\"slave_id\" value=\"1\"></div>";
    oss << "<div><div class=\"small\">Tipo de ponto Modbus</div><select id=\"modbusRegType\" name=\"reg\"><option value=\"coil_status\">Coil Status</option><option value=\"input_status\">Input Status</option><option value=\"holding\" selected>Holding Register</option><option value=\"input_register\">Input Register</option></select></div>";
    oss << "<div><div class=\"small\">Endereço inicial do ponto Modbus</div><input name=\"address\" value=\"0\"></div>";
    oss << "<div><div class=\"small\">Formato do dado lido</div><select id=\"modbusDataType\" name=\"data_type\"><option value=\"bool\">bool</option><option value=\"int16\" selected>int16</option><option value=\"uint16\">uint16</option><option value=\"int32\">int32</option><option value=\"uint32\">uint32</option><option value=\"float32\">float32</option></select></div>";
    oss << "<div><div class=\"small\">Índice DNP3 sugerido/editável</div><input name=\"dnp_index\" value=\"0\"></div>";
    oss << "</div>";
    oss << "<div class=\"statusbox\" style=\"margin-top:12px\">";
    oss << "<div class=\"title\" style=\"font-size:16px;margin-bottom:8px\">Publicação DNP3 sugerida automaticamente</div>";
    oss << "<div class=\"grid grid3\">";
    oss << "<div><div class=\"small\">Tipo DNP3</div><strong id=\"dnpSugType\">Analog Input</strong></div>";
    oss << "<div><div class=\"small\">Objeto / Grupo</div><strong id=\"dnpSugObj\">30</strong></div>";
    oss << "<div><div class=\"small\">Variação estática</div><strong id=\"dnpSugVar\">2</strong></div>";
    oss << "<div><div class=\"small\">Classe</div><strong id=\"dnpSugClass\">Class 1</strong></div>";
    oss << "<div style=\"grid-column:span 2\"><div class=\"small\">Descrição</div><strong id=\"dnpSugDesc\">Entrada analógica DNP3 inteira de 16 bits</strong></div>";
    oss << "</div>";
    oss << "<div class=\"small\" style=\"margin-top:8px\">O tipo, objeto, variação e classe são calculados pelo sistema. O único campo DNP3 que você altera é o índice.</div>";
    oss << "</div>";
    oss << "<div style=\"margin-top:12px\"><button type=\"submit\">Salvar ponto</button></div>";
    oss << "</form>";
    oss << "<div style=\"margin-top:10px\"><a href=\"/clearpoints\" onclick=\"return confirm(\'Tem certeza que deseja limpar a lista de pontos? Depois reinicie o programa.\')\"><button type=\"button\" class=\"secondary\">Limpar lista de pontos</button></a></div>";
    oss << "</div>";

    oss << "<div class=\"card\"><div class=\"title\">Pontos configurados</div>";
    const auto configured_points_for_ui = LoadConfiguredPointsFromCsvForUi(g_active_config_file);
    if (configured_points_for_ui.empty()) {
        oss << "<div class=\"statusbox\"><strong>Nenhum ponto configurado ainda.</strong><br>Cadastre um ponto acima para iniciar a configuração.</div>";
    } else {
        oss << "<div class=\"small\" style=\"margin-bottom:8px\">Esta tabela mostra os pontos cadastrados para leitura Modbus TCP e publicação DNP3.</div>";
        oss << "<table><thead><tr><th>Ponto</th><th>Equipamento de origem</th><th>Tipo Modbus</th><th>Dado</th><th>Endereço</th><th>Qtd</th><th>Publicação DNP3 sugerida</th><th>Orientação para supervisório</th><th>Valor</th><th>Conectado</th><th>Falhas</th><th>Status</th><th>Ação</th></tr></thead><tbody>";
        for (size_t i = 0; i < configured_points_for_ui.size(); ++i) {
            const auto& p = configured_points_for_ui[i];
            const int runtime_index = FindRuntimePointIndexForUi(p);
            const bool active_now = runtime_index >= 0 &&
                                    static_cast<size_t>(runtime_index) < point_states.size();

            oss << "<tr><td>" << HtmlEscape(p.description) << "</td>"
                << "<td class=\"mono\">" << HtmlEscape(p.ip) << ":" << p.port << " | id " << p.slave_id << "</td>"
                << "<td>" << HtmlEscape(ReadValueTypeRegisterKind(p.type)) << "</td>"
                << "<td>" << HtmlEscape(ReadValueTypeDataType(p.type)) << "</td>"
                << "<td>" << p.address << "</td>"
                << "<td>" << p.count << "</td>"
                << "<td>" << HtmlEscape(Dnp3PointTypeLabel(p.type))
                << " " << p.dnp3_analog_index
                << "<br><span class=\"small\">Objeto " << Dnp3ObjectGroup(p.type)
                << " / Var " << Dnp3StaticVariationNumber(p.type)
                << " / " << HtmlEscape(Dnp3ClassLabel(p.type)) << "</span></td>"
                << "<td class=\"small\">Elipse E3: P1/N1=0, P2/N2=1, P3/N3="
                << (Dnp3ObjectGroup(p.type) * 100 + Dnp3StaticVariationNumber(p.type))
                << ", P4/N4=" << p.dnp3_analog_index
                << "<br>Tipo: " << HtmlEscape(Dnp3PointTypeLabel(p.type))
                << " | porta TCP " << DNP3_TCP_PORT << "</td>";

            if (active_now) {
                const auto& s = point_states[static_cast<size_t>(runtime_index)];
                oss << "<td>" << s.analog_value << "</td>"
                    << "<td>" << (s.connection_status ? "SIM" : "NÃO") << "</td>"
                    << "<td>" << s.failure_count << "</td>"
                    << "<td><span class=\"ok\">ATIVO</span></td>";
            } else {
                oss << "<td>-</td><td>-</td><td>-</td><td><span class=\"warn\">PENDENTE REINICIAR</span></td>";
            }

            oss << "<td><a href=\"/deletepoint?idx=" << i << "\" onclick=\"return confirm('Remover este ponto do arquivo de configuração? Depois reinicie o programa para o DNP3 recriar o banco.');\"><button type=\"button\" class=\"secondary\">Remover</button></a></td></tr>";
        }
        oss << "</tbody></table>";
    }
    oss << "</div>";

    oss << "</section><section>";

    oss << "<div class=\"card\"><div class=\"title\">Logs</div><div class=\"logs\">";
    for (const auto& l : g_recent_logs) {
        const char* cls = (l.level == "ERROR") ? "bad" : (l.level == "WARN" ? "warn" : "ok");
        oss << "<div class=\"log\"><div><strong>" << HtmlEscape(l.scope) << "</strong> <span class=\"" << cls << "\">" << HtmlEscape(l.level) << "</span></div>"
            << "<div>" << HtmlEscape(l.text) << "</div>"
            << "<div class=\"small\">" << HtmlEscape(l.time) << "</div></div>";
    }
    oss << "</div></div>";

    oss << "</section></div></main></body></html>";
    return oss.str();
}

bool SendAll(int fd, const char* data, size_t len)
{
    size_t total = 0;
    while (total < len) {
        const ssize_t sent = send(fd, data + total, len - total, 0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (sent == 0) return false;
        total += static_cast<size_t>(sent);
    }
    return true;
}

void SendHttpResponse(int client_fd, const string& status, const string& content_type, const string& body)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "Connection: close\r\n\r\n";
    const string headers = oss.str();
    SendAll(client_fd, headers.c_str(), headers.size());
    SendAll(client_fd, body.c_str(), body.size());
    shutdown(client_fd, SHUT_RDWR);
}

string UrlEncodeSimple(const string& s)
{
    std::ostringstream oss;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << static_cast<char>(c);
        } else if (c == ' ') {
            oss << '+';
        } else {
            oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(c) << std::nouppercase << std::dec;
        }
    }
    return oss.str();
}

void SendHttpRedirectToHome(int client_fd, const string& notice)
{
    const string location = "/?notice=" + UrlEncodeSimple(notice);
    const string body = "Redirecionando...";
    std::ostringstream oss;
    oss << "HTTP/1.1 303 See Other\r\n"
        << "Location: " << location << "\r\n"
        << "Content-Type: text/plain; charset=utf-8\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "Connection: close\r\n\r\n";
    const string headers = oss.str();
    SendAll(client_fd, headers.c_str(), headers.size());
    SendAll(client_fd, body.c_str(), body.size());
    shutdown(client_fd, SHUT_RDWR);
}

string GetQueryParam(const string& path, const string& key)
{
    const auto qpos = path.find('?');
    if (qpos == string::npos) return "";
    string query = path.substr(qpos + 1);
    string token;
    std::istringstream iss(query);
    while (std::getline(iss, token, '&')) {
        auto eq = token.find('=');
        string k = token.substr(0, eq);
        string v = (eq == string::npos) ? "" : token.substr(eq + 1);
        if (k == key) return UrlDecode(v);
    }
    return "";
}

void HandleHttpClient(int client_fd, const vector<ReadPointState>* point_states)
{
    char buffer[8192];
    const ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buffer[n] = 0;
    string req(buffer);
    std::istringstream iss(req);
    string method, path, version;
    iss >> method >> path >> version;

    string notice;

    if (path.rfind("/deletepoint", 0) == 0) {
        const string idx_text = GetQueryParam(path, "idx");
        try {
            const size_t idx = static_cast<size_t>(std::stoul(idx_text));
            string error;
            if (RemovePointFromConfigFileByIndex(idx, g_active_config_file, error)) {
                notice = "Ponto removido do arquivo de configuração. Reinicie o programa para o DNP3 recriar o banco sem esse ponto.";
            } else {
                notice = "Falha ao remover ponto: " + error;
            }
        } catch (const std::exception&) {
            notice = "Falha ao remover ponto: índice inválido.";
        }
        SendHttpRedirectToHome(client_fd, notice);
        close(client_fd);
        return;
    }

    if (path.rfind("/clearpoints", 0) == 0) {
        ProfileConfig empty_profile;
        empty_profile.mode = OperatingMode::ConfigFile;
        empty_profile.name = "Configuração Modbus -> DNP3: " + g_active_config_file;
        if (SaveProfileReadPointsToCsv(empty_profile, g_active_config_file)) {
            notice = "Arquivo de pontos limpo. Reinicie o programa para a tabela iniciar vazia.";
        } else {
            notice = "Falha ao limpar o arquivo de pontos.";
        }
        SendHttpRedirectToHome(client_fd, notice);
        close(client_fd);
        return;
    }

    if (path.rfind("/addpoint", 0) == 0) {
        const string csv_line = BuildCsvLineFromForm(path);
        if (!csv_line.empty()) {
            string error;
            if (AppendPointToConfigFileFromLine(csv_line, g_active_config_file, error)) {
                notice = "Ponto salvo em " + g_active_config_file + ". Ele já aparece na lista como ponto do arquivo. Reinicie o programa para o DNP3 criar o índice e iniciar a leitura.";
            } else {
                notice = "Falha ao salvar ponto: " + error;
            }
        } else {
            notice = "Formulário incompleto. Preencha todos os campos do ponto.";
        }
        SendHttpRedirectToHome(client_fd, notice);
        close(client_fd);
        return;
    }

    if (path == "/" || path.rfind("/index", 0) == 0 || path.rfind("/?", 0) == 0) {
        const string home_notice = GetQueryParam(path, "notice");
        SendHttpResponse(client_fd, "200 OK", "text/html; charset=utf-8", BuildHtmlPageServerRendered(*point_states, home_notice));
        close(client_fd);
        return;
    }

    SendHttpResponse(client_fd, "404 Not Found", "text/plain; charset=utf-8", "Not found");
    close(client_fd);
}

void HttpServerThread(const vector<ReadPointState>* point_states)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LogLine(AppLogLevel::Error, "WEB", "Falha ao criar socket HTTP");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(UI_HTTP_PORT);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LogLine(AppLogLevel::Error, "WEB", "Falha ao fazer bind na porta " + std::to_string(UI_HTTP_PORT) + ": " + std::string(strerror(errno)));
        close(server_fd);
        return;
    }
    if (listen(server_fd, 16) < 0) {
        LogLine(AppLogLevel::Error, "WEB", "Falha ao escutar na porta " + std::to_string(UI_HTTP_PORT) + ": " + std::string(strerror(errno)));
        close(server_fd);
        return;
    }

    LogLine(AppLogLevel::Info, "WEB", "Painel web ativo em http://localhost:" + std::to_string(UI_HTTP_PORT));

    while (g_running.load()) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            continue;
        }
        HandleHttpClient(client_fd, point_states);
    }

    close(server_fd);
}
#endif

void PrintConsoleHelp()
{
    LogLine(AppLogLevel::Info, "HELP", "Uso normal: ./slavednp3  (lê automaticamente points_config.csv)");
    LogLine(AppLogLevel::Info, "HELP", "Opcional: ./slavednp3 --config=outro_arquivo.csv");
    LogLine(AppLogLevel::Info, "HELP", "Comandos locais: help | show | quit");
    LogLine(AppLogLevel::Info, "HELP", "A escrita não é feita pela interface. Ela vem do supervisório DNP3 e é convertida para Modbus automaticamente.");
    LogLine(AppLogLevel::Info, "HELP", "Pontos graváveis automaticamente: Holding Register -> Analog Output; Coil Status -> CROB.");
    LogLine(AppLogLevel::Info, "HELP", "Painel web: abra http://localhost:8080 ou http://IP_DA_RASPBERRY:8080");
}

void ConsoleThread()
{
    PrintConsoleHelp();

    string line;
    while (g_running.load()) {
        if (!std::getline(std::cin, line)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        ExecuteCommandLine(line);
    }
}
#endif
