#include "Tipos.h"

mutex data_mutex;
mutex dnp3_update_mutex;
mutex log_mutex;
#ifdef LIBMODBUS_MASTER
mutex modbus_access_mutex;
#endif
atomic<bool> g_running{true};

deque<UiLogEntry> g_recent_logs;
string g_active_config_file = DEFAULT_POINTS_CONFIG_FILE;
std::vector<int> g_hidden_deleted_point_indexes;
#ifdef LIBMODBUS_MASTER
ProfileConfig g_profile;
#endif

bool IsPointHiddenAfterDelete(int index)
{
    return std::find(g_hidden_deleted_point_indexes.begin(),
                     g_hidden_deleted_point_indexes.end(),
                     index) != g_hidden_deleted_point_indexes.end();
}


const char* AppLogLevelToString(AppLogLevel level)
{
    switch (level) {
    case AppLogLevel::Info:
        return "INFO";
    case AppLogLevel::Warn:
        return "WARN";
    case AppLogLevel::Error:
        return "ERROR";
    default:
        return "LOG";
    }
}

string TimestampNow()
{
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto time_t_now = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_value{};
#ifdef _WIN32
    localtime_s(&tm_value, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_value);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y-%m-%d %H:%M:%S")
        << "."
        << std::setw(3) << std::setfill('0') << ms.count();

    return oss.str();
}

void LogLine(AppLogLevel level, const std::string& scope, const std::string& text)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    const std::string ts = TimestampNow();
    std::ostream& os = (level == AppLogLevel::Error) ? std::cerr : std::cout;

    os << "[" << ts << "] "
       << "[" << AppLogLevelToString(level) << "] "
       << "[" << scope << "] "
       << text
       << std::endl;

    g_recent_logs.push_front(UiLogEntry{ts, AppLogLevelToString(level), scope, text});
    while (g_recent_logs.size() > MAX_UI_LOGS) {
        g_recent_logs.pop_back();
    }
}

