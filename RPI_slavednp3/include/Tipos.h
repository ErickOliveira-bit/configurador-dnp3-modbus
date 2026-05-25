#pragma once

#include <opendnp3/DNP3Manager.h>
#include <opendnp3/ConsoleLogger.h>
#include <opendnp3/logging/LogLevels.h>
#include <opendnp3/outstation/DefaultOutstationApplication.h>
#include <opendnp3/outstation/UpdateBuilder.h>
#include <opendnp3/outstation/OutstationStackConfig.h>
#include <opendnp3/outstation/ICommandHandler.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>
#include <opendnp3/channel/IPEndpoint.h>
#include <opendnp3/channel/PrintingChannelListener.h>

#ifdef LIBMODBUS_MASTER
#include <modbus/modbus.h>
#endif

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <deque>

#ifdef __linux__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace std;
using namespace opendnp3;

// ============================================================
// CONFIGURACOES GERAIS DO SISTEMA
// ============================================================
inline constexpr int DNP3_TCP_PORT = 20000;
inline constexpr int DNP3_LOCAL_ADDR = 2;
inline constexpr int DNP3_REMOTE_ADDR = 1;

inline constexpr int MODBUS_RESPONSE_TIMEOUT_SEC = 1;
inline constexpr int MODBUS_RESPONSE_TIMEOUT_USEC = 0;
inline constexpr int MODBUS_BYTE_TIMEOUT_SEC = 0;
inline constexpr int MODBUS_BYTE_TIMEOUT_USEC = 500000;

inline constexpr int MODBUS_POLL_INTERVAL_MS = 1000;
inline constexpr int INITIAL_THREAD_STAGGER_MS = 100;

inline constexpr size_t MAX_UI_LOGS = 200;
inline constexpr const char* DEFAULT_POINTS_CONFIG_FILE = "points_config.csv";

#ifdef __linux__
inline constexpr int UI_HTTP_PORT = 8080;
#endif

// ============================================================
// TIPOS GERAIS
// ============================================================
struct UiLogEntry
{
    std::string time;
    std::string level;
    std::string scope;
    std::string text;
};

enum class AppLogLevel
{
    Info,
    Warn,
    Error
};

enum class OperatingMode
{
    ConfigFile
};

#ifdef LIBMODBUS_MASTER
enum class ReadValueType
{
    CoilStatusBool,
    InputStatusBool,
    HoldingInt16,
    InputInt16,
    HoldingUInt16,
    InputUInt16,
    HoldingInt32,
    InputInt32,
    HoldingUInt32,
    InputUInt32,
    HoldingFloat32,
    InputFloat32
};

enum class WriteTargetType
{
    HoldingRegister,
    Coil
};

struct ReadPointConfig
{
    std::string ip;
    int port;
    int slave_id;
    ReadValueType type;
    int address;
    int count;
    uint16_t dnp3_analog_index;
    uint16_t dnp3_status_index;
    std::string description;
};

struct CommandPointConfig
{
    uint16_t dnp3_index;
    std::string ip;
    int port;
    int slave_id;
    WriteTargetType target;
    int modbus_address;
    ReadValueType value_type;
    std::string description;
};

struct ReadPointState
{
    double analog_value = 0.0;
    bool connection_status = false;
    bool connection_changed = true;
    int failure_count = 0;
    static constexpr int max_failures_before_zero = 5;
    chrono::system_clock::time_point last_change_time{};
};

struct ProfileConfig
{
    OperatingMode mode = OperatingMode::ConfigFile;
    std::string name;
    std::vector<ReadPointConfig> read_points;
    std::vector<CommandPointConfig> analog_commands;
    std::vector<CommandPointConfig> binary_commands;
};
#endif

// ============================================================
// GLOBAIS
// ============================================================
extern mutex data_mutex;
extern mutex dnp3_update_mutex;
extern mutex log_mutex;
#ifdef LIBMODBUS_MASTER
extern mutex modbus_access_mutex;
#endif
extern atomic<bool> g_running;
extern deque<UiLogEntry> g_recent_logs;
extern string g_active_config_file;
extern std::vector<int> g_hidden_deleted_point_indexes;
#ifdef LIBMODBUS_MASTER
extern ProfileConfig g_profile;
#endif

// ============================================================
// LOG
// ============================================================
bool IsPointHiddenAfterDelete(int index);
const char* AppLogLevelToString(AppLogLevel level);
string TimestampNow();
void LogLine(AppLogLevel level, const std::string& scope, const std::string& text);

// ============================================================
// UTILIDADES / CONFIGURACAO
// ============================================================
string TrimCopy(const string& input);
string JsonEscape(const string& s);
string UrlDecode(const string& s);
const char* OperatingModeToString(OperatingMode mode);

#ifdef LIBMODBUS_MASTER
const char* ReadValueTypeToString(ReadValueType type);
bool IsHoldingRegisterType(ReadValueType type);
bool IsInputRegisterType(ReadValueType type);
bool IsCoilStatusType(ReadValueType type);
bool IsInputStatusType(ReadValueType type);
bool IsDiscreteBitType(ReadValueType type);
int RegisterCountForType(ReadValueType type);
bool IsFloatType(ReadValueType type);
bool PublishesAsDnp3Binary(ReadValueType type);
string Dnp3PointTypeLabel(ReadValueType type);
int Dnp3ObjectGroup(ReadValueType type);
int Dnp3StaticVariationNumber(ReadValueType type);
string Dnp3ClassLabel(ReadValueType type);
string Dnp3SuggestionDescription(ReadValueType type);
StaticAnalogVariation StaticVariationForType(ReadValueType type);
string NormalizeToken(string value);
bool ParseReadValueType(const string& register_kind, const string& data_type, ReadValueType& out);
string ReadValueTypeRegisterKind(ReadValueType type);
string ReadValueTypeDataType(ReadValueType type);
const char* WriteTargetTypeToString(WriteTargetType type);

void HandleCommunicationFailure(ReadPointState& state);
void HandleCommunicationSuccess(ReadPointState& state);

string GetConfigFileFromArgs(int argc, char* argv[]);
vector<string> SplitSemicolonLine(const string& line);
bool IsLegacyAutoExampleLine(const string& clean);
bool FileHasOnlyLegacyAutoExamples(const string& file_path);
bool TryBuildReadPointFromCsvLine(const string& line, ReadPointConfig& point, string& error);
bool SaveProfileReadPointsToCsv(const ProfileConfig& profile, const string& file_path);
ProfileConfig BuildConfigFileProfile(const string& file_path);
ProfileConfig BuildProfile(const string& config_file = DEFAULT_POINTS_CONFIG_FILE);
std::vector<ReadPointConfig> LoadConfiguredPointsFromCsvForUi(const string& file_path);
bool SameReadPointForUi(const ReadPointConfig& a, const ReadPointConfig& b);
int FindRuntimePointIndexForUi(const ReadPointConfig& point);

// ============================================================
// MODBUS
// ============================================================
const CommandPointConfig* FindCommandConfig(const vector<CommandPointConfig>& configs,
                                            uint16_t dnp3_index,
                                            WriteTargetType expected_target);
CommandStatus MapModbusErrnoToDNP3Status();
bool CreateAndConnect(const char* ip, int port, int slave_id, modbus_t*& ctx);
bool ReadModbusPoint(const ReadPointConfig& point, double& value_out);
bool BuildHoldingRegisterWritePayload(const CommandPointConfig& cfg,
                                      double value,
                                      uint16_t* registers,
                                      int& register_count,
                                      CommandStatus& status);
CommandStatus WriteHoldingRegister(const CommandPointConfig& cfg, double value);
CommandStatus WriteCoil(const CommandPointConfig& cfg, bool value);

// ============================================================
// DNP3
// ============================================================
DatabaseConfig ConfigureDatabase(const ProfileConfig& profile);
void UpdateDNP3Values(UpdateBuilder& builder,
                      const ReadPointConfig& point,
                      const ReadPointState& state);
void PublishPointToDNP3(int point_index,
                        const ReadPointConfig& point,
                        vector<ReadPointState>& point_states,
                        shared_ptr<IOutstation> outstation);
void PollPoint(int point_index,
               const vector<ReadPointConfig>* points,
               vector<ReadPointState>* point_states,
               shared_ptr<IOutstation> outstation);
void ApplyInitialStateToDNP3(const vector<ReadPointConfig>& points,
                             vector<ReadPointState>& point_states,
                             shared_ptr<IOutstation> outstation);
std::string OperationTypeToStringSafe(OperationType op);
std::shared_ptr<ICommandHandler> CreateCommandHandler(const vector<CommandPointConfig>& analog_cmds,
                                                      const vector<CommandPointConfig>& binary_cmds);

// ============================================================
// INTERFACE / CONSOLE
// ============================================================
void PrintMappings();
string BuildStateJson(const vector<ReadPointState>& point_states);
bool ExecuteCommandLine(const string& raw_line);
void PrintConsoleHelp();
void ConsoleThread();
#ifdef __linux__
string GetQueryParam(const string& path, const string& key);
string HtmlEscape(const string& s);
string BuildCsvLineFromForm(const string& path);
bool IsSameConfiguredPoint(const ReadPointConfig& a, const ReadPointConfig& b);
bool AppendPointToConfigFileFromLine(const string& csv_line, const string& file_path, string& error);
bool RemovePointFromConfigFileByIndex(size_t remove_index, const string& file_path, string& error);
string BuildHtmlPageServerRendered(const vector<ReadPointState>& point_states, const string& notice = "");
string HttpResponse(const string& body, const string& type = "text/html; charset=utf-8", const string& status = "200 OK");
string RedirectResponse(const string& location);
void SendAll(int client, const string& data);
void HttpServerThread(const vector<ReadPointState>* point_states);
#endif
#endif
