#include "Tipos.h"

#ifdef LIBMODBUS_MASTER
DatabaseConfig ConfigureDatabase(const ProfileConfig& profile)
{
    DatabaseConfig config;

    if (profile.read_points.empty()) {
        return config;
    }

    uint16_t max_analog = 0;
    uint16_t max_binary = 0;
    bool has_analog = false;
    bool has_binary = false;

    for (const auto& point : profile.read_points) {
        if (PublishesAsDnp3Binary(point.type)) {
            max_binary = std::max(max_binary, point.dnp3_analog_index);
            has_binary = true;
        } else {
            max_analog = std::max(max_analog, point.dnp3_analog_index);
            has_analog = true;
        }
    }

    if (has_analog) {
        for (uint16_t index = 0; index <= max_analog; ++index) {
            config.analog_input[index] = AnalogConfig();
            config.analog_input[index].clazz = PointClass::Class1;
            config.analog_input[index].svariation = StaticAnalogVariation::Group30Var1;

            // Também cria o Analog Output Status no mesmo índice.
            // Isso ajuda o mestre DNP3 a reconhecer que o outstation aceita comandos analógicos nesse índice.
            config.analog_output_status[index] = AOStatusConfig();
            config.analog_output_status[index].clazz = PointClass::Class1;
            config.analog_output_status[index].svariation = StaticAnalogOutputStatusVariation::Group40Var1;
            config.analog_output_status[index].evariation = EventAnalogOutputStatusVariation::Group42Var1;
        }
    }

    if (has_binary) {
        for (uint16_t index = 0; index <= max_binary; ++index) {
            config.binary_input[index] = BinaryConfig();
            config.binary_input[index].clazz = PointClass::Class1;
            config.binary_input[index].svariation = StaticBinaryVariation::Group1Var2;
            config.binary_input[index].evariation = EventBinaryVariation::Group2Var2;

            // Também cria o Binary Output Status no mesmo índice.
            // Isso ajuda o mestre DNP3 a reconhecer que o outstation aceita CROB nesse índice.
            config.binary_output_status[index] = BOStatusConfig();
            config.binary_output_status[index].clazz = PointClass::Class1;
            config.binary_output_status[index].svariation = StaticBinaryOutputStatusVariation::Group10Var2;
            config.binary_output_status[index].evariation = EventBinaryOutputStatusVariation::Group11Var2;
        }
    }

    for (const auto& point : profile.read_points) {
        if (PublishesAsDnp3Binary(point.type)) {
            config.binary_input[point.dnp3_analog_index].clazz = PointClass::Class1;
            config.binary_input[point.dnp3_analog_index].svariation = StaticBinaryVariation::Group1Var2;
            config.binary_input[point.dnp3_analog_index].evariation = EventBinaryVariation::Group2Var2;
        } else {
            config.analog_input[point.dnp3_analog_index].clazz = PointClass::Class1;
            config.analog_input[point.dnp3_analog_index].svariation = StaticVariationForType(point.type);
        }
    }

    return config;
}

void UpdateDNP3Values(UpdateBuilder& builder,
                      const ReadPointConfig& point,
                      const ReadPointState& state)
{
    const bool too_many_failures =
        (state.failure_count >= ReadPointState::max_failures_before_zero);

    const double value_to_publish = too_many_failures ? 0.0 : state.analog_value;

    if (PublishesAsDnp3Binary(point.type)) {
        builder.Update(Binary(value_to_publish != 0.0, Flags(0x01)), point.dnp3_analog_index);
    } else {
        builder.Update(Analog(value_to_publish), point.dnp3_analog_index);
    }
}

void PublishPointToDNP3(int point_index,
                        const ReadPointConfig& point,
                        vector<ReadPointState>& point_states,
                        shared_ptr<IOutstation> outstation)
{
    lock_guard<mutex> dnp_lock(dnp3_update_mutex);
    UpdateBuilder builder;

    {
        lock_guard<mutex> data_lock(data_mutex);
        UpdateDNP3Values(builder,
                         point,
                         point_states[point_index]);
        point_states[point_index].connection_changed = false;
    }

    outstation->Apply(builder.Build());
}

// ============================================================
// POLLING THREAD
// ============================================================
void PollPoint(int point_index,
               const vector<ReadPointConfig>* points,
               vector<ReadPointState>* point_states,
               shared_ptr<IOutstation> outstation)
{
    const ReadPointConfig& point = (*points)[point_index];

    this_thread::sleep_for(chrono::milliseconds(INITIAL_THREAD_STAGGER_MS * point_index));

    while (g_running.load()) {
        bool read_success = false;
        double new_value = 0.0;

        read_success = ReadModbusPoint(point, new_value);

        if (!read_success) {
            std::ostringstream msg;
            msg << "idx=" << point_index
                << " ip=" << point.ip << ":" << point.port
                << " id=" << point.slave_id
                << " tipo=" << ReadValueTypeToString(point.type)
                << " addr=" << point.address
                << " qtd=" << point.count
                << " desc=\"" << point.description << "\""
                << " erro=\"" << modbus_strerror(errno) << "\"";
            LogLine(AppLogLevel::Error, "MODBUS-READ", msg.str());
        }

        {
            lock_guard<mutex> lock(data_mutex);

            if (read_success) {
                (*point_states)[point_index].analog_value = new_value;
                HandleCommunicationSuccess((*point_states)[point_index]);

                std::ostringstream msg;
                msg << "idx=" << point_index
                    << " ip=" << point.ip << ":" << point.port
                    << " id=" << point.slave_id
                    << " tipo=" << ReadValueTypeToString(point.type)
                    << " addr=" << point.address
                    << " dnp=" << Dnp3PointTypeLabel(point.type) << " " << point.dnp3_analog_index
                    << " valor=" << new_value
                    << " desc=\"" << point.description << "\"";
                LogLine(AppLogLevel::Info, "MODBUS-READ-OK", msg.str());
            } else {
                HandleCommunicationFailure((*point_states)[point_index]);

                std::ostringstream msg;
                msg << "idx=" << point_index
                    << " ip=" << point.ip << ":" << point.port
                    << " id=" << point.slave_id
                    << " failure_count=" << (*point_states)[point_index].failure_count
                    << " desc=\"" << point.description << "\"";
                LogLine(AppLogLevel::Warn, "MODBUS-STATE", msg.str());
            }
        }

        PublishPointToDNP3(point_index, point, *point_states, outstation);
        std::this_thread::sleep_for(std::chrono::milliseconds(MODBUS_POLL_INTERVAL_MS));
    }
}

void ApplyInitialStateToDNP3(const vector<ReadPointConfig>& points,
                             vector<ReadPointState>& point_states,
                             shared_ptr<IOutstation> outstation)
{
    UpdateBuilder builder;

    for (size_t i = 0; i < points.size(); ++i) {
        const auto& point = points[i];
        auto& state = point_states[i];
        state.analog_value = 0;
        state.connection_status = false;
        state.connection_changed = true;
        state.failure_count = 0;
        state.last_change_time = chrono::system_clock::now();

        UpdateDNP3Values(builder,
                         point,
                         state);
    }

    outstation->Apply(builder.Build());
}

// ============================================================
// DNP3 COMMAND HANDLER
// ============================================================
std::string OperationTypeToStringSafe(OperationType op)
{
    switch (op) {
    case OperationType::LATCH_ON:  return "LATCH_ON";
    case OperationType::LATCH_OFF: return "LATCH_OFF";
    case OperationType::PULSE_ON:  return "PULSE_ON";
    case OperationType::PULSE_OFF: return "PULSE_OFF";
    default:
        return "UNKNOWN(" + std::to_string(static_cast<int>(op)) + ")";
    }
}

std::string OperateTypeToStringSafe(OperateType opType)
{
    return std::to_string(static_cast<int>(opType));
}

// ============================================================
// DNP3 COMMAND HANDLER
// ============================================================
class ModbusCommandHandler final : public ICommandHandler
{
public:
    ModbusCommandHandler(const vector<CommandPointConfig>& analog_cmds,
                         const vector<CommandPointConfig>& binary_cmds)
        : analog_cmds_(analog_cmds),
          binary_cmds_(binary_cmds)
    {
    }

    void Begin() override {}
    void End() override {}

    CommandStatus Select(const ControlRelayOutputBlock& command, uint16_t index) override
    {
        const auto* cfg = FindCommandConfig(binary_cmds_, index, WriteTargetType::Coil);
        std::ostringstream msg;
        msg << "index=" << index
            << " opType=" << OperationTypeToStringSafe(command.opType)
            << (cfg ? " aceito" : " nao_mapeado");
        LogLine(cfg ? AppLogLevel::Info : AppLogLevel::Warn, "DNP3-CMD-SELECT-CROB", msg.str());
        return cfg ? CommandStatus::SUCCESS : CommandStatus::NOT_SUPPORTED;
    }

    CommandStatus Operate(const ControlRelayOutputBlock& command,
                          uint16_t index,
                          IUpdateHandler& handler,
                          OperateType opType) override
    {
        (void)handler;

        const auto* cfg = FindCommandConfig(binary_cmds_, index, WriteTargetType::Coil);
        std::ostringstream msg;
        msg << "index=" << index
            << " opType=" << OperationTypeToStringSafe(command.opType)
            << " operateType=" << OperateTypeToStringSafe(opType)
            << (cfg ? " recebido" : " nao_mapeado");
        LogLine(cfg ? AppLogLevel::Info : AppLogLevel::Warn, "DNP3-CMD-OPERATE-CROB", msg.str());

        if (cfg == nullptr) {
            return CommandStatus::NOT_SUPPORTED;
        }

        // Tratamento robusto para comandos binários vindos do Elipse.
        // Em alguns testes o Elipse envia o CROB, mas o valor escrito no IOTag
        // nao chega como 0/1 comum; ele chega como um tipo de operacao DNP3.
        // Por isso a decisao aqui fica baseada no opType do CROB.
        bool coil_value = true;
        bool opType_known = true;

        switch (command.opType) {
        case OperationType::LATCH_ON:
        case OperationType::PULSE_ON:
            coil_value = true;
            break;
        case OperationType::LATCH_OFF:
        case OperationType::PULSE_OFF:
            coil_value = false;
            break;
        default:
            opType_known = false;
            // Se o tipo vier desconhecido, ainda assim executamos o comando como ON.
            // Isso evita receber o comando DNP3 e nao fazer nada no Modbus.
            coil_value = true;
            break;
        }

        {
            std::ostringstream write_msg;
            write_msg << "index=" << index
                      << " opType=" << OperationTypeToStringSafe(command.opType)
                      << " conhecido=" << (opType_known ? "sim" : "nao")
                      << " valor_modbus=" << (coil_value ? 1 : 0)
                      << " destino=" << cfg->ip << ":" << cfg->port
                      << " id=" << cfg->slave_id
                      << " coil_addr=" << cfg->modbus_address
                      << " desc="" << cfg->description << """;
            LogLine(AppLogLevel::Info, "DNP3-CMD-CROB-WRITE", write_msg.str());
        }

        const CommandStatus status = WriteCoil(*cfg, coil_value);

        {
            std::ostringstream result_msg;
            result_msg << "index=" << index
                       << " resultado=" << static_cast<int>(status)
                       << " valor_modbus=" << (coil_value ? 1 : 0);
            LogLine(status == CommandStatus::SUCCESS ? AppLogLevel::Info : AppLogLevel::Error,
                    "DNP3-CMD-CROB-RESULT",
                    result_msg.str());
        }

        return status;
    }

    CommandStatus Select(const AnalogOutputInt16& command, uint16_t index) override
    {
        const auto* cfg = FindCommandConfig(analog_cmds_, index, WriteTargetType::HoldingRegister);
        LogAnalogSelect("DNP3-CMD-SELECT-AO16", index, static_cast<double>(command.value), cfg);
        return cfg ? CommandStatus::SUCCESS : CommandStatus::NOT_SUPPORTED;
    }

    CommandStatus Operate(const AnalogOutputInt16& command,
                          uint16_t index,
                          IUpdateHandler& handler,
                          OperateType opType) override
    {
        (void)handler;
        return OperateAnalog(static_cast<double>(command.value), index, "AO16", opType);
    }

    CommandStatus Select(const AnalogOutputInt32& command, uint16_t index) override
    {
        const auto* cfg = FindCommandConfig(analog_cmds_, index, WriteTargetType::HoldingRegister);
        LogAnalogSelect("DNP3-CMD-SELECT-AO32", index, static_cast<double>(command.value), cfg);
        return cfg ? CommandStatus::SUCCESS : CommandStatus::NOT_SUPPORTED;
    }

    CommandStatus Operate(const AnalogOutputInt32& command,
                          uint16_t index,
                          IUpdateHandler& handler,
                          OperateType opType) override
    {
        (void)handler;
        return OperateAnalog(static_cast<double>(command.value), index, "AO32", opType);
    }

    CommandStatus Select(const AnalogOutputFloat32& command, uint16_t index) override
    {
        const auto* cfg = FindCommandConfig(analog_cmds_, index, WriteTargetType::HoldingRegister);
        LogAnalogSelect("DNP3-CMD-SELECT-AO32F", index, static_cast<double>(command.value), cfg);
        return cfg ? CommandStatus::SUCCESS : CommandStatus::NOT_SUPPORTED;
    }

    CommandStatus Operate(const AnalogOutputFloat32& command,
                          uint16_t index,
                          IUpdateHandler& handler,
                          OperateType opType) override
    {
        (void)handler;
        return OperateAnalog(static_cast<double>(command.value), index, "AO32F", opType);
    }

    CommandStatus Select(const AnalogOutputDouble64& command, uint16_t index) override
    {
        const auto* cfg = FindCommandConfig(analog_cmds_, index, WriteTargetType::HoldingRegister);
        LogAnalogSelect("DNP3-CMD-SELECT-AO64F", index, static_cast<double>(command.value), cfg);
        return cfg ? CommandStatus::SUCCESS : CommandStatus::NOT_SUPPORTED;
    }

    CommandStatus Operate(const AnalogOutputDouble64& command,
                          uint16_t index,
                          IUpdateHandler& handler,
                          OperateType opType) override
    {
        (void)handler;
        return OperateAnalog(static_cast<double>(command.value), index, "AO64F", opType);
    }

private:
    void LogAnalogSelect(const std::string& scope,
                         uint16_t index,
                         double value,
                         const CommandPointConfig* cfg)
    {
        std::ostringstream msg;
        msg << "index=" << index
            << " valor=" << value
            << (cfg ? " aceito" : " nao_mapeado");
        if (cfg) {
            msg << " tipo_destino=" << ReadValueTypeToString(cfg->value_type)
                << " addr=" << cfg->modbus_address;
        }
        LogLine(cfg ? AppLogLevel::Info : AppLogLevel::Warn, scope, msg.str());
    }

    CommandStatus OperateAnalog(double value,
                                uint16_t index,
                                const std::string& command_type,
                                OperateType opType)
    {
        const auto* cfg = FindCommandConfig(analog_cmds_, index, WriteTargetType::HoldingRegister);
        std::ostringstream msg;
        msg << "index=" << index
            << " comando=" << command_type
            << " operateType=" << OperateTypeToStringSafe(opType)
            << " valor=" << value
            << (cfg ? " recebido" : " nao_mapeado");
        if (cfg) {
            msg << " tipo_destino=" << ReadValueTypeToString(cfg->value_type)
                << " addr=" << cfg->modbus_address;
        }
        LogLine(cfg ? AppLogLevel::Info : AppLogLevel::Warn, "DNP3-CMD-OPERATE-AO", msg.str());

        if (cfg == nullptr) {
            return CommandStatus::NOT_SUPPORTED;
        }

        return WriteHoldingRegister(*cfg, value);
    }

    const vector<CommandPointConfig>& analog_cmds_;
    const vector<CommandPointConfig>& binary_cmds_;
};



std::shared_ptr<ICommandHandler> CreateCommandHandler(const vector<CommandPointConfig>& analog_cmds,
                                                      const vector<CommandPointConfig>& binary_cmds)
{
    return std::make_shared<ModbusCommandHandler>(analog_cmds, binary_cmds);
}
#endif
