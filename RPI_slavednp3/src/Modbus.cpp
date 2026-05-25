#include "Tipos.h"

#ifdef LIBMODBUS_MASTER
const CommandPointConfig* FindCommandConfig(const vector<CommandPointConfig>& configs,
                                            uint16_t dnp3_index,
                                            WriteTargetType expected_target)
{
    for (const auto& cfg : configs) {
        if (cfg.dnp3_index == dnp3_index && cfg.target == expected_target) {
            return &cfg;
        }
    }
    return nullptr;
}

CommandStatus MapModbusErrnoToDNP3Status()
{
    if (errno == ETIMEDOUT) {
        return CommandStatus::TIMEOUT;
    }

    if (errno == EINVAL) {
        return CommandStatus::FORMAT_ERROR;
    }

    return CommandStatus::DOWNSTREAM_FAIL;
}

bool CreateAndConnect(const char* ip, int port, int slave_id, modbus_t*& ctx)
{
    ctx = modbus_new_tcp(ip, port);
    if (ctx == nullptr) {
        return false;
    }

    if (modbus_set_slave(ctx, slave_id) == -1) {
        modbus_free(ctx);
        ctx = nullptr;
        return false;
    }

    modbus_set_response_timeout(ctx,
                                MODBUS_RESPONSE_TIMEOUT_SEC,
                                MODBUS_RESPONSE_TIMEOUT_USEC);
    modbus_set_byte_timeout(ctx,
                            MODBUS_BYTE_TIMEOUT_SEC,
                            MODBUS_BYTE_TIMEOUT_USEC);

    if (modbus_connect(ctx) == -1) {
        modbus_free(ctx);
        ctx = nullptr;
        return false;
    }

    return true;
}

bool ReadModbusPoint(const ReadPointConfig& point, double& value_out)
{
    lock_guard<mutex> modbus_lock(modbus_access_mutex);

    modbus_t* ctx = nullptr;
    if (!CreateAndConnect(point.ip.c_str(), point.port, point.slave_id, ctx)) {
        return false;
    }

    uint16_t tab_reg[4] = {0, 0, 0, 0};
    uint8_t tab_bit[1] = {0};
    const int register_count = RegisterCountForType(point.type);
    int rc = -1;

    if (IsDiscreteBitType(point.type)) {
        if (IsCoilStatusType(point.type)) {
            rc = modbus_read_bits(ctx, point.address, 1, tab_bit);
        } else {
            rc = modbus_read_input_bits(ctx, point.address, 1, tab_bit);
        }
    } else if (IsHoldingRegisterType(point.type)) {
        rc = modbus_read_registers(ctx, point.address, register_count, tab_reg);
    } else {
        rc = modbus_read_input_registers(ctx, point.address, register_count, tab_reg);
    }

    if ((IsDiscreteBitType(point.type) && rc == 1) || (!IsDiscreteBitType(point.type) && rc == register_count)) {
        switch (point.type) {
        case ReadValueType::CoilStatusBool:
        case ReadValueType::InputStatusBool:
            value_out = tab_bit[0] ? 1.0 : 0.0;
            break;
        case ReadValueType::HoldingInt16:
        case ReadValueType::InputInt16:
            value_out = static_cast<int16_t>(tab_reg[0]);
            break;

        case ReadValueType::HoldingUInt16:
        case ReadValueType::InputUInt16:
            value_out = static_cast<uint16_t>(tab_reg[0]);
            break;

        case ReadValueType::HoldingInt32:
        case ReadValueType::InputInt32: {
            const uint32_t raw = (static_cast<uint32_t>(tab_reg[0]) << 16) |
                                  static_cast<uint32_t>(tab_reg[1]);
            value_out = static_cast<int32_t>(raw);
            break;
        }

        case ReadValueType::HoldingUInt32:
        case ReadValueType::InputUInt32: {
            const uint32_t raw = (static_cast<uint32_t>(tab_reg[0]) << 16) |
                                  static_cast<uint32_t>(tab_reg[1]);
            value_out = static_cast<uint32_t>(raw);
            break;
        }

        case ReadValueType::HoldingFloat32:
        case ReadValueType::InputFloat32: {
            const uint32_t raw = (static_cast<uint32_t>(tab_reg[0]) << 16) |
                                  static_cast<uint32_t>(tab_reg[1]);
            float f = 0.0f;
            std::memcpy(&f, &raw, sizeof(float));
            value_out = static_cast<double>(f);
            break;
        }
        }

        modbus_close(ctx);
        modbus_free(ctx);
        return true;
    }

    modbus_close(ctx);
    modbus_free(ctx);
    return false;
}


bool BuildHoldingRegisterWritePayload(const CommandPointConfig& cfg,
                                      double value,
                                      uint16_t regs[2],
                                      int& register_count,
                                      CommandStatus& error_status,
                                      std::string& error_text)
{
    register_count = RegisterCountForType(cfg.value_type);
    error_status = CommandStatus::SUCCESS;
    error_text.clear();

    if (!IsHoldingRegisterType(cfg.value_type)) {
        error_status = CommandStatus::NOT_SUPPORTED;
        error_text = "O ponto de comando nao e Holding Register.";
        return false;
    }

    switch (cfg.value_type) {
    case ReadValueType::HoldingInt16: {
        const long long v = static_cast<long long>(std::llround(value));
        if (v < std::numeric_limits<int16_t>::min() || v > std::numeric_limits<int16_t>::max()) {
            error_status = CommandStatus::OUT_OF_RANGE;
            error_text = "Valor fora da faixa int16.";
            return false;
        }
        regs[0] = static_cast<uint16_t>(static_cast<int16_t>(v));
        register_count = 1;
        return true;
    }

    case ReadValueType::HoldingUInt16: {
        const long long v = static_cast<long long>(std::llround(value));
        if (v < 0 || v > std::numeric_limits<uint16_t>::max()) {
            error_status = CommandStatus::OUT_OF_RANGE;
            error_text = "Valor fora da faixa uint16.";
            return false;
        }
        regs[0] = static_cast<uint16_t>(v);
        register_count = 1;
        return true;
    }

    case ReadValueType::HoldingInt32: {
        const long long v = static_cast<long long>(std::llround(value));
        if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max()) {
            error_status = CommandStatus::OUT_OF_RANGE;
            error_text = "Valor fora da faixa int32.";
            return false;
        }
        const uint32_t raw = static_cast<uint32_t>(static_cast<int32_t>(v));
        regs[0] = static_cast<uint16_t>((raw >> 16) & 0xFFFF);
        regs[1] = static_cast<uint16_t>(raw & 0xFFFF);
        register_count = 2;
        return true;
    }

    case ReadValueType::HoldingUInt32: {
        const long long v = static_cast<long long>(std::llround(value));
        if (v < 0 || static_cast<unsigned long long>(v) > std::numeric_limits<uint32_t>::max()) {
            error_status = CommandStatus::OUT_OF_RANGE;
            error_text = "Valor fora da faixa uint32.";
            return false;
        }
        const uint32_t raw = static_cast<uint32_t>(v);
        regs[0] = static_cast<uint16_t>((raw >> 16) & 0xFFFF);
        regs[1] = static_cast<uint16_t>(raw & 0xFFFF);
        register_count = 2;
        return true;
    }

    case ReadValueType::HoldingFloat32: {
        const float f = static_cast<float>(value);
        uint32_t raw = 0;
        std::memcpy(&raw, &f, sizeof(float));
        regs[0] = static_cast<uint16_t>((raw >> 16) & 0xFFFF);
        regs[1] = static_cast<uint16_t>(raw & 0xFFFF);
        register_count = 2;
        return true;
    }

    default:
        error_status = CommandStatus::NOT_SUPPORTED;
        error_text = "Tipo de dado nao suportado para escrita Holding Register.";
        return false;
    }
}

CommandStatus WriteHoldingRegister(const CommandPointConfig& cfg, double value)
{
    uint16_t regs[2] = {0, 0};
    int register_count = 1;
    CommandStatus payload_status = CommandStatus::SUCCESS;
    std::string payload_error;

    if (!BuildHoldingRegisterWritePayload(cfg, value, regs, register_count, payload_status, payload_error)) {
        std::ostringstream msg;
        msg << "idx=" << cfg.dnp3_index
            << " ip=" << cfg.ip << ":" << cfg.port
            << " id=" << cfg.slave_id
            << " target=" << WriteTargetTypeToString(cfg.target)
            << " tipo=" << ReadValueTypeToString(cfg.value_type)
            << " addr=" << cfg.modbus_address
            << " value=" << value
            << " desc=\"" << cfg.description << "\""
            << " erro=\"" << payload_error << "\"";
        LogLine(AppLogLevel::Error, "MODBUS-WRITE-PAYLOAD", msg.str());
        return payload_status;
    }

    {
        std::ostringstream msg;
        msg << "idx=" << cfg.dnp3_index
            << " ip=" << cfg.ip << ":" << cfg.port
            << " id=" << cfg.slave_id
            << " target=" << WriteTargetTypeToString(cfg.target)
            << " tipo=" << ReadValueTypeToString(cfg.value_type)
            << " addr=" << cfg.modbus_address
            << " qtd=" << register_count
            << " value=" << value
            << " desc=\"" << cfg.description << "\"";
        LogLine(AppLogLevel::Info, "MODBUS-WRITE-REQ", msg.str());
    }

    lock_guard<mutex> modbus_lock(modbus_access_mutex);

    modbus_t* ctx = nullptr;
    if (!CreateAndConnect(cfg.ip.c_str(), cfg.port, cfg.slave_id, ctx)) {
        return MapModbusErrnoToDNP3Status();
    }

    int rc = -1;
    if (register_count == 1) {
        rc = modbus_write_register(ctx, cfg.modbus_address, regs[0]);
    } else {
        rc = modbus_write_registers(ctx, cfg.modbus_address, register_count, regs);
    }

    if ((register_count == 1 && rc == 1) || (register_count > 1 && rc == register_count)) {
        std::ostringstream msg;
        msg << "idx=" << cfg.dnp3_index
            << " ip=" << cfg.ip << ":" << cfg.port
            << " id=" << cfg.slave_id
            << " target=" << WriteTargetTypeToString(cfg.target)
            << " tipo=" << ReadValueTypeToString(cfg.value_type)
            << " addr=" << cfg.modbus_address
            << " qtd=" << register_count
            << " value=" << value
            << " desc=\"" << cfg.description << "\"";
        LogLine(AppLogLevel::Info, "MODBUS-WRITE-OK", msg.str());

        modbus_close(ctx);
        modbus_free(ctx);
        return CommandStatus::SUCCESS;
    }

    std::ostringstream msg;
    msg << "idx=" << cfg.dnp3_index
        << " ip=" << cfg.ip << ":" << cfg.port
        << " id=" << cfg.slave_id
        << " target=" << WriteTargetTypeToString(cfg.target)
        << " tipo=" << ReadValueTypeToString(cfg.value_type)
        << " addr=" << cfg.modbus_address
        << " qtd=" << register_count
        << " value=" << value
        << " desc=\"" << cfg.description << "\""
        << " erro=\"" << modbus_strerror(errno) << "\"";
    LogLine(AppLogLevel::Error, "MODBUS-WRITE", msg.str());

    modbus_close(ctx);
    modbus_free(ctx);
    return MapModbusErrnoToDNP3Status();
}

CommandStatus WriteCoil(const CommandPointConfig& cfg, bool value)
{
    {
        std::ostringstream msg;
        msg << "ip=" << cfg.ip << ":" << cfg.port
            << " id=" << cfg.slave_id
            << " target=" << WriteTargetTypeToString(cfg.target)
            << " addr=" << cfg.modbus_address
            << " value=" << (value ? 1 : 0)
            << " desc=\"" << cfg.description << "\"";
        LogLine(AppLogLevel::Info, "MODBUS-WRITE-REQ", msg.str());
    }

    lock_guard<mutex> modbus_lock(modbus_access_mutex);

    modbus_t* ctx = nullptr;
    if (!CreateAndConnect(cfg.ip.c_str(), cfg.port, cfg.slave_id, ctx)) {
        return MapModbusErrnoToDNP3Status();
    }

    const int rc = modbus_write_bit(ctx, cfg.modbus_address, value ? 1 : 0);

    if (rc == 1) {
        std::ostringstream msg;
        msg << "ip=" << cfg.ip << ":" << cfg.port
            << " id=" << cfg.slave_id
            << " target=" << WriteTargetTypeToString(cfg.target)
            << " addr=" << cfg.modbus_address
            << " value=" << (value ? 1 : 0)
            << " desc=\"" << cfg.description << "\"";
        LogLine(AppLogLevel::Info, "MODBUS-WRITE-OK", msg.str());

        modbus_close(ctx);
        modbus_free(ctx);
        return CommandStatus::SUCCESS;
    }

    std::ostringstream msg;
    msg << "ip=" << cfg.ip << ":" << cfg.port
        << " id=" << cfg.slave_id
        << " target=" << WriteTargetTypeToString(cfg.target)
        << " addr=" << cfg.modbus_address
        << " value=" << (value ? 1 : 0)
        << " desc=\"" << cfg.description << "\""
        << " erro=\"" << modbus_strerror(errno) << "\"";
    LogLine(AppLogLevel::Error, "MODBUS-WRITE", msg.str());

    modbus_close(ctx);
    modbus_free(ctx);
    return MapModbusErrnoToDNP3Status();
}

#endif
