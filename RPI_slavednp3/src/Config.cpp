#include "Tipos.h"

const char* OperatingModeToString(OperatingMode mode)
{
    (void)mode;
    return "config_file";
}

#ifdef LIBMODBUS_MASTER
const char* ReadValueTypeToString(ReadValueType type)
{
    switch (type) {
    case ReadValueType::CoilStatusBool: return "COIL_STATUS_BOOL";
    case ReadValueType::InputStatusBool:return "INPUT_STATUS_BOOL";
    case ReadValueType::HoldingInt16:   return "HOLDING_INT16";
    case ReadValueType::InputInt16:     return "INPUT_INT16";
    case ReadValueType::HoldingUInt16:  return "HOLDING_UINT16";
    case ReadValueType::InputUInt16:    return "INPUT_UINT16";
    case ReadValueType::HoldingInt32:   return "HOLDING_INT32";
    case ReadValueType::InputInt32:     return "INPUT_INT32";
    case ReadValueType::HoldingUInt32:  return "HOLDING_UINT32";
    case ReadValueType::InputUInt32:    return "INPUT_UINT32";
    case ReadValueType::HoldingFloat32: return "HOLDING_FLOAT32";
    case ReadValueType::InputFloat32:   return "INPUT_FLOAT32";
    default:                            return "UNKNOWN";
    }
}

bool IsHoldingRegisterType(ReadValueType type)
{
    return type == ReadValueType::HoldingInt16 ||
           type == ReadValueType::HoldingUInt16 ||
           type == ReadValueType::HoldingInt32 ||
           type == ReadValueType::HoldingUInt32 ||
           type == ReadValueType::HoldingFloat32;
}

bool IsInputRegisterType(ReadValueType type)
{
    return type == ReadValueType::InputInt16 ||
           type == ReadValueType::InputUInt16 ||
           type == ReadValueType::InputInt32 ||
           type == ReadValueType::InputUInt32 ||
           type == ReadValueType::InputFloat32;
}

bool IsCoilStatusType(ReadValueType type)
{
    return type == ReadValueType::CoilStatusBool;
}

bool IsInputStatusType(ReadValueType type)
{
    return type == ReadValueType::InputStatusBool;
}

bool IsDiscreteBitType(ReadValueType type)
{
    return IsCoilStatusType(type) || IsInputStatusType(type);
}

int RegisterCountForType(ReadValueType type)
{
    switch (type) {
    case ReadValueType::HoldingInt32:
    case ReadValueType::InputInt32:
    case ReadValueType::HoldingUInt32:
    case ReadValueType::InputUInt32:
    case ReadValueType::HoldingFloat32:
    case ReadValueType::InputFloat32:
        return 2;
    default:
        return 1;
    }
}

bool IsFloatType(ReadValueType type)
{
    return type == ReadValueType::HoldingFloat32 ||
           type == ReadValueType::InputFloat32;
}

bool PublishesAsDnp3Binary(ReadValueType type)
{
    return IsDiscreteBitType(type);
}

string Dnp3PointTypeLabel(ReadValueType type)
{
    return PublishesAsDnp3Binary(type) ? "Binary Input" : "Analog Input";
}

int Dnp3ObjectGroup(ReadValueType type)
{
    return PublishesAsDnp3Binary(type) ? 1 : 30;
}

int Dnp3StaticVariationNumber(ReadValueType type)
{
    if (PublishesAsDnp3Binary(type)) return 2;
    if (IsFloatType(type)) return 5;
    switch (type) {
    case ReadValueType::HoldingInt32:
    case ReadValueType::InputInt32:
    case ReadValueType::HoldingUInt32:
    case ReadValueType::InputUInt32:
        return 1;
    default:
        return 2;
    }
}

string Dnp3ClassLabel(ReadValueType type)
{
    return "Class 1";
}

string Dnp3SuggestionDescription(ReadValueType type)
{
    if (PublishesAsDnp3Binary(type)) return "Entrada binária DNP3 para ponto discreto/booleano";
    if (IsFloatType(type)) return "Entrada analógica DNP3 em ponto flutuante de 32 bits";
    switch (type) {
    case ReadValueType::HoldingInt32:
    case ReadValueType::InputInt32:
    case ReadValueType::HoldingUInt32:
    case ReadValueType::InputUInt32:
        return "Entrada analógica DNP3 inteira de 32 bits";
    default:
        return "Entrada analógica DNP3 inteira de 16 bits";
    }
}

StaticAnalogVariation StaticVariationForType(ReadValueType type)
{
    switch (type) {
    case ReadValueType::HoldingFloat32:
    case ReadValueType::InputFloat32:
        return StaticAnalogVariation::Group30Var5; // 32-bit float com flags
    case ReadValueType::HoldingInt16:
    case ReadValueType::InputInt16:
    case ReadValueType::HoldingUInt16:
    case ReadValueType::InputUInt16:
        return StaticAnalogVariation::Group30Var2; // 16-bit inteiro com flags
    default:
        return StaticAnalogVariation::Group30Var1; // 32-bit inteiro com flags
    }
}

string NormalizeToken(string value)
{
    value = TrimCopy(value);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ParseReadValueType(const string& register_kind, const string& data_type, ReadValueType& out)
{
    const string reg = NormalizeToken(register_kind);
    const string data = NormalizeToken(data_type);

    const bool coil_status = (reg == "coil" || reg == "coil_status" || reg == "coils" || reg == "0x");
    const bool input_status = (reg == "input_status" || reg == "discrete_input" || reg == "discrete" || reg == "1x");
    const bool holding = (reg == "holding" || reg == "hr" || reg == "4x" || reg == "holding_register");
    const bool input = (reg == "input" || reg == "ir" || reg == "3x" || reg == "input_register");

    if (coil_status) {
        if (data == "bool" || data == "boolean" || data == "bit" || data == "0_1") {
            out = ReadValueType::CoilStatusBool;
            return true;
        }
        return false;
    }

    if (input_status) {
        if (data == "bool" || data == "boolean" || data == "bit" || data == "0_1") {
            out = ReadValueType::InputStatusBool;
            return true;
        }
        return false;
    }

    if (!holding && !input) return false;

    if (data == "int16" || data == "short") { out = holding ? ReadValueType::HoldingInt16 : ReadValueType::InputInt16; return true; }
    if (data == "uint16" || data == "word") { out = holding ? ReadValueType::HoldingUInt16 : ReadValueType::InputUInt16; return true; }
    if (data == "int32" || data == "long") { out = holding ? ReadValueType::HoldingInt32 : ReadValueType::InputInt32; return true; }
    if (data == "uint32" || data == "dword") { out = holding ? ReadValueType::HoldingUInt32 : ReadValueType::InputUInt32; return true; }
    if (data == "float" || data == "float32" || data == "real") { out = holding ? ReadValueType::HoldingFloat32 : ReadValueType::InputFloat32; return true; }

    return false;
}

string ReadValueTypeRegisterKind(ReadValueType type)
{
    if (IsCoilStatusType(type)) return "coil_status";
    if (IsInputStatusType(type)) return "input_status";
    if (IsHoldingRegisterType(type)) return "holding";
    return "input_register";
}

string ReadValueTypeDataType(ReadValueType type)
{
    switch (type) {
    case ReadValueType::CoilStatusBool:
    case ReadValueType::InputStatusBool: return "bool";
    case ReadValueType::HoldingInt16:
    case ReadValueType::InputInt16: return "int16";
    case ReadValueType::HoldingUInt16:
    case ReadValueType::InputUInt16: return "uint16";
    case ReadValueType::HoldingInt32:
    case ReadValueType::InputInt32: return "int32";
    case ReadValueType::HoldingUInt32:
    case ReadValueType::InputUInt32: return "uint32";
    case ReadValueType::HoldingFloat32:
    case ReadValueType::InputFloat32: return "float32";
    default: return "unknown";
    }
}

const char* WriteTargetTypeToString(WriteTargetType type)
{
    switch (type) {
    case WriteTargetType::HoldingRegister:
        return "HR";
    case WriteTargetType::Coil:
        return "COIL";
    default:
        return "UNKNOWN";
    }
}

void HandleCommunicationFailure(ReadPointState& state)
{
    const bool previous_status = state.connection_status;

    state.connection_status = false;
    state.connection_changed = (state.connection_status != previous_status);
    state.failure_count++;

    if (state.failure_count >= ReadPointState::max_failures_before_zero) {
        state.analog_value = 0;
    }

    if (state.connection_changed) {
        state.last_change_time = chrono::system_clock::now();
    }
}

void HandleCommunicationSuccess(ReadPointState& state)
{
    const bool previous_status = state.connection_status;

    state.connection_status = true;
    state.connection_changed = (state.connection_status != previous_status);
    state.failure_count = 0;

    if (state.connection_changed) {
        state.last_change_time = chrono::system_clock::now();
    }
}


string GetConfigFileFromArgs(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const std::string prefix = "--config=";
        if (arg.rfind(prefix, 0) == 0) {
            return arg.substr(prefix.size());
        }
    }
    return DEFAULT_POINTS_CONFIG_FILE;
}

vector<string> SplitSemicolonLine(const string& line)
{
    vector<string> parts;
    string item;
    std::istringstream iss(line);
    while (std::getline(iss, item, ';')) {
        parts.push_back(TrimCopy(item));
    }
    return parts;
}


bool IsLegacyAutoExampleLine(const string& clean)
{
    return clean.rfind("Exemplo inteiro HR0;", 0) == 0 ||
           clean.rfind("Exemplo float32 HR10_HR11;", 0) == 0;
}

bool FileHasOnlyLegacyAutoExamples(const string& file_path)
{
    std::ifstream in(file_path);
    if (!in.is_open()) return false;

    bool found_legacy_example = false;
    string line;
    while (std::getline(in, line)) {
        const string clean = TrimCopy(line);
        if (clean.empty() || clean[0] == '#') {
            continue;
        }
        if (!IsLegacyAutoExampleLine(clean)) {
            return false;
        }
        found_legacy_example = true;
    }
    return found_legacy_example;
}

bool TryBuildReadPointFromCsvLine(const string& line, ReadPointConfig& point, string& error)
{
    const string clean = TrimCopy(line);
    if (clean.empty() || clean[0] == '#') return false;

    const auto parts = SplitSemicolonLine(clean);
    if (parts.size() < 8) {
        error = "Linha ignorada: use nome;ip;port;slave_id;registrador;endereco;tipo_dado;dnp3_indice";
        return false;
    }

    ReadValueType parsed_type{};
    if (!ParseReadValueType(parts[4], parts[6], parsed_type)) {
        error = "Tipo inválido na linha: registrador=" + parts[4] + " tipo_dado=" + parts[6] + ". Para Coil Status ou Input Status use tipo_dado=bool.";
        return false;
    }

    try {
        point.description = parts[0];
        point.ip = parts[1];
        point.port = std::stoi(parts[2]);
        point.slave_id = std::stoi(parts[3]);
        point.type = parsed_type;
        point.address = std::stoi(parts[5]);
        point.count = RegisterCountForType(parsed_type);
        point.dnp3_analog_index = static_cast<uint16_t>(std::stoi(parts[7]));
        point.dnp3_status_index = 0; // mantido apenas por compatibilidade interna; não é usado na nova interface
        return true;
    } catch (const std::exception& ex) {
        error = string("Erro convertendo linha: ") + ex.what();
        return false;
    }
}

bool SaveProfileReadPointsToCsv(const ProfileConfig& profile, const string& file_path)
{
    std::ofstream out(file_path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "# nome;ip;port;slave_id;registrador;endereco;tipo_dado;dnp3_indice\n";
    out << "# registrador: coil_status,input_status,holding,input_register | tipo_dado: bool,int16,uint16,int32,uint32,float32\n";
    out << "# O DNP3 é sugerido automaticamente. O único campo DNP3 editável é o índice.\n";
    for (const auto& p : profile.read_points) {
        out << p.description << ";"
            << p.ip << ";"
            << p.port << ";"
            << p.slave_id << ";"
            << ReadValueTypeRegisterKind(p.type) << ";"
            << p.address << ";"
            << ReadValueTypeDataType(p.type) << ";"
            << p.dnp3_analog_index << "\n";
    }
    return true;
}

void BuildAutomaticCommandMappingsFromReadPoints(ProfileConfig& profile)
{
    profile.analog_commands.clear();
    profile.binary_commands.clear();

    for (const auto& p : profile.read_points) {
        if (IsHoldingRegisterType(p.type)) {
            profile.analog_commands.push_back(CommandPointConfig{
                p.dnp3_analog_index,
                p.ip,
                p.port,
                p.slave_id,
                WriteTargetType::HoldingRegister,
                p.address,
                p.type,
                "Comando automatico AO -> Holding Register para: " + p.description
            });
        } else if (IsCoilStatusType(p.type)) {
            profile.binary_commands.push_back(CommandPointConfig{
                p.dnp3_analog_index,
                p.ip,
                p.port,
                p.slave_id,
                WriteTargetType::Coil,
                p.address,
                p.type,
                "Comando automatico CROB -> Coil para: " + p.description
            });
        }
    }
}


ProfileConfig BuildConfigFileProfile(const string& file_path)
{
    ProfileConfig profile;
    profile.mode = OperatingMode::ConfigFile;
    profile.name = "Configuração Modbus -> DNP3: " + file_path;

    std::ifstream in(file_path);
    if (!in.is_open()) {
        LogLine(AppLogLevel::Warn, "CONFIG", "Arquivo de configuração não encontrado: " + file_path + ". Criando automaticamente um CSV vazio para cadastro pela interface.");
        SaveProfileReadPointsToCsv(profile, file_path);
        return profile;
    }

    if (FileHasOnlyLegacyAutoExamples(file_path)) {
        in.close();
        LogLine(AppLogLevel::Warn, "CONFIG", "Foram encontrados somente exemplos automáticos antigos no points_config.csv. Limpando o arquivo para iniciar sem pontos pré-configurados.");
        SaveProfileReadPointsToCsv(profile, file_path);
        return profile;
    }

    in.clear();
    in.seekg(0, std::ios::beg);

    string line;
    int line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        ReadPointConfig point;
        string error;
        if (TryBuildReadPointFromCsvLine(line, point, error)) {
            profile.read_points.push_back(point);
        } else if (!TrimCopy(line).empty() && TrimCopy(line)[0] != '#') {
            LogLine(AppLogLevel::Warn, "CONFIG", "Linha " + std::to_string(line_number) + ": " + error);
        }
    }

    if (profile.read_points.empty()) {
        LogLine(AppLogLevel::Info, "CONFIG", "Nenhum ponto configurado ainda. A interface iniciará vazia para cadastro dos pontos Modbus -> DNP3.");
    }

    BuildAutomaticCommandMappingsFromReadPoints(profile);

    {
        std::ostringstream msg;
        msg << "Comandos DNP3 automaticos criados a partir dos pontos de leitura: "
            << profile.analog_commands.size() << " analogicos e "
            << profile.binary_commands.size() << " binarios. "
            << "Holding Register aceita Analog Output; Coil Status aceita CROB. "
            << "Input Register e Input Status sao somente leitura.";
        LogLine(AppLogLevel::Info, "CONFIG", msg.str());
    }

    return profile;
}

ProfileConfig BuildProfile(const string& config_file)
{
    // MODO ÚNICO DO PROJETO:
    // O gateway sempre carrega os pontos a partir do arquivo CSV.
    // A leitura Modbus -> DNP3 é cadastrada pela interface.
    // A escrita DNP3 -> Modbus continua ativa, mas é gerada automaticamente
    // para pontos Modbus que aceitam escrita: Holding Register e Coil Status.
    return BuildConfigFileProfile(config_file);
}


std::vector<ReadPointConfig> LoadConfiguredPointsFromCsvForUi(const string& file_path)
{
    std::vector<ReadPointConfig> points;
    std::ifstream in(file_path);
    if (!in.is_open()) {
        return points;
    }

    string line;
    while (std::getline(in, line)) {
        ReadPointConfig point;
        string error;
        if (TryBuildReadPointFromCsvLine(line, point, error)) {
            points.push_back(point);
        }
    }
    return points;
}

bool SameReadPointForUi(const ReadPointConfig& a, const ReadPointConfig& b)
{
    return a.description == b.description &&
           a.ip == b.ip &&
           a.port == b.port &&
           a.slave_id == b.slave_id &&
           a.type == b.type &&
           a.address == b.address &&
           a.dnp3_analog_index == b.dnp3_analog_index;
}

int FindRuntimePointIndexForUi(const ReadPointConfig& point)
{
    for (size_t i = 0; i < g_profile.read_points.size(); ++i) {
        if (SameReadPointForUi(point, g_profile.read_points[i])) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

#endif
