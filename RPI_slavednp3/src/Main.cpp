#include "Tipos.h"

int main(int argc, char* argv[])
{
    const auto logLevels = levels::NORMAL;

#ifdef LIBMODBUS_MASTER
    const string config_file = GetConfigFileFromArgs(argc, argv);
    g_active_config_file = config_file;
    g_profile = BuildProfile(config_file);

    {
        std::ostringstream msg;
        msg << "Modo único: configuração por arquivo"
            << " | arquivo=\"" << g_active_config_file << "\""
            << " | perfil=\"" << g_profile.name << "\"";
        LogLine(AppLogLevel::Info, "SYSTEM", msg.str());
    }

    DNP3Manager manager(1, ConsoleLogger::Create());

    auto channel = manager.AddTCPServer(
        "server",
        logLevels,
        ServerAcceptMode::CloseExisting,
        IPEndpoint("0.0.0.0", DNP3_TCP_PORT),
        PrintingChannelListener::Create());

    OutstationStackConfig stackConfig(ConfigureDatabase(g_profile));
    stackConfig.outstation.eventBufferConfig = EventBufferConfig::AllTypes(100);
    stackConfig.outstation.params.allowUnsolicited = true;
    stackConfig.link.LocalAddr = DNP3_LOCAL_ADDR;
    stackConfig.link.RemoteAddr = DNP3_REMOTE_ADDR;

    auto command_handler = CreateCommandHandler(
        g_profile.analog_commands,
        g_profile.binary_commands);

    auto outstation = channel->AddOutstation(
        "outstation",
        command_handler,
        std::make_shared<DefaultOutstationApplication>(),
        stackConfig);

    outstation->Enable();

    vector<ReadPointState> point_states(g_profile.read_points.size());
    ApplyInitialStateToDNP3(g_profile.read_points, point_states, outstation);

    vector<thread> threads;
    threads.reserve(g_profile.read_points.size());
    for (int i = 0; i < static_cast<int>(g_profile.read_points.size()); ++i) {
        threads.emplace_back(PollPoint, i, &g_profile.read_points, &point_states, outstation);
    }

    for (auto& t : threads) {
        t.detach();
    }

    thread console_thread(ConsoleThread);
    console_thread.detach();

#ifdef __linux__
    thread http_thread(HttpServerThread, &point_states);
    http_thread.detach();
#else
    LogLine(AppLogLevel::Warn, "WEB", "Painel web embutido disponível apenas neste exemplo Linux.");
#endif

    LogLine(AppLogLevel::Info, "SYSTEM", "DNP3 outstation habilitada na porta 20000");
    LogLine(AppLogLevel::Info, "SYSTEM", "Mestre Modbus TCP ativo");
    PrintMappings();

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
#else
    DNP3Manager manager(1, ConsoleLogger::Create());

    auto channel = manager.AddTCPServer(
        "server",
        logLevels,
        ServerAcceptMode::CloseExisting,
        IPEndpoint("0.0.0.0", DNP3_TCP_PORT),
        PrintingChannelListener::Create());

    OutstationStackConfig stackConfig{DatabaseConfig()};
    auto outstation = channel->AddOutstation(
        "outstation",
        std::make_shared<SimpleCommandHandler>(CommandStatus::NOT_SUPPORTED),
        std::make_shared<DefaultOutstationApplication>(),
        stackConfig);

    outstation->Enable();

    LogLine(AppLogLevel::Warn, "SYSTEM", "LIBMODBUS_MASTER nao definido. Apenas outstation DNP3 foi iniciada.");

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
#endif

    return 0;
}
