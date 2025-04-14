#ifndef SHAREDMEMORYLOGGER_H
#define SHAREDMEMORYLOGGER_H

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include "SolverHubDef.h"

namespace EMP {

    enum class LogLevel {
        Debug,      // 详细调试信息
        Info,       // 普通操作信息
        Warning,    // 警告信息
        Error,      // 错误信息
        Critical    // 严重错误
    };

    // 日志记录系统基类
    class SOLVERHUB_API SharedMemoryLogger {
    public:
        SharedMemoryLogger(const std::string& logFilePath, bool isCreator, LogLevel minLevel = LogLevel::Info)
            : logFilePath_(logFilePath), isCreator_(isCreator), minLevel_(minLevel) {
            openLogFile();
        }

        virtual ~SharedMemoryLogger() {
            closeLogFile();
        }

        // 记录共享对象创建事件
        virtual void logObjectCreation(const std::string& objectType, const std::string& objectName, uint64_t version) {
            log(LogLevel::Info, "Created " + objectType + " object: " + objectName + ", version: " + std::to_string(version));
        }

        // 记录共享对象写入事件
        virtual void logObjectWrite(const std::string& objectType, const std::string& objectName, uint64_t version) {
            log(LogLevel::Debug, "Write to " + objectType + " object: " + objectName + ", new version: " + std::to_string(version));
        }

        // 记录共享对象读取事件
        virtual void logObjectRead(const std::string& objectType, const std::string& objectName, uint64_t version) {
            log(LogLevel::Debug, "Read from " + objectType + " object: " + objectName + ", version: " + std::to_string(version));
        }

        // 记录共享内存段创建事件
        virtual void logMemorySegmentCreation(const std::string& segmentName, size_t size) {
            log(LogLevel::Info, "Created memory segment: " + segmentName + ", size: " + std::to_string(size) + " bytes");
        }

        // 记录共享内存段增长事件
        virtual void logMemorySegmentGrowth(const std::string& segmentName, size_t oldSize, size_t newSize) {
            log(LogLevel::Info, "Grow memory segment: " + segmentName + ", from " + std::to_string(oldSize) + " to " + std::to_string(newSize) + " bytes");
        }

        // 记录异常信息
        virtual void logException(int type, int code, const std::string& message) {
            log(LogLevel::Error, "Exception - Type: " + std::to_string(type) + ", Code: " + std::to_string(code) + ", Message: " + message);
        }

        // 记录一般信息
        virtual void logInfo(const std::string& message) {
            log(LogLevel::Info, message);
        }

        // 记录警告信息
        virtual void logWarning(const std::string& message) {
            log(LogLevel::Warning, message);
        }

        // 记录错误信息
        virtual void logError(const std::string& message) {
            log(LogLevel::Error, message);
        }

        // 记录调试信息
        virtual void logDebug(const std::string& message) {
            log(LogLevel::Debug, message);
        }

        // 记录临界错误信息
        virtual void logCritical(const std::string& message) {
            log(LogLevel::Critical, message);
        }

        // 设置日志级别
        void setLogLevel(LogLevel level) {
            minLevel_ = level;
        }

    protected:
        friend class SharedMemoryManager;

        // 基础日志记录函数
        virtual void log(LogLevel level, const std::string& message) {
            if (level < minLevel_) {
                return;
            }

            std::lock_guard<std::mutex> lock(mutex_);

            if (!logFile_.is_open()) {
                if (!openLogFile()) {
                    std::cerr << "Failed to open log file: " << logFilePath_ << std::endl;
                    return;
                }
            }

            auto now = std::chrono::system_clock::now();
            auto nowTime = std::chrono::system_clock::to_time_t(now);
            auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            // 使用 localtime_s 替代 localtime
            std::tm timeInfo;
#ifdef _WIN32
            localtime_s(&timeInfo, &nowTime);
#else
            // 在非Windows平台上使用线程安全的 localtime_r
            localtime_r(&nowTime, &timeInfo);
#endif

            std::stringstream dateTimeStream;
            dateTimeStream << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << nowMs.count();

            logFile_ << dateTimeStream.str() << " [" << getLevelString(level) << "] "
                << (isCreator_ ? "[Creator] " : "[Client] ") << message << std::endl;

            logFile_.flush();
        }

        // 打开日志文件
        bool openLogFile() {
            try {
                // 确保目录存在
                std::filesystem::path logPath(logFilePath_);
                auto parentPath = logPath.parent_path();
                if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                    std::filesystem::create_directories(parentPath);
                }

                logFile_.open(logFilePath_, std::ios::app);
                if (logFile_.is_open()) {
                    // 添加日志头部
                    auto now = std::chrono::system_clock::now();
                    auto nowTime = std::chrono::system_clock::to_time_t(now);

                    // 使用 localtime_s 替代 localtime
                    std::tm timeInfo;
#ifdef _WIN32
                    localtime_s(&timeInfo, &nowTime);
#else
                    // 在非Windows平台上使用线程安全的 localtime_r
                    localtime_r(&nowTime, &timeInfo);
#endif

                    logFile_ << "\n--------------------------------------------------\n";
                    logFile_ << "Log session started at " << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
                        << " [" << (isCreator_ ? "Creator" : "Client") << "]\n";
                    logFile_ << "--------------------------------------------------\n";
                    return true;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error opening log file: " << e.what() << std::endl;
            }
            return false;
        }

        // 关闭日志文件
        void closeLogFile() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (logFile_.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto nowTime = std::chrono::system_clock::to_time_t(now);

                // 使用 localtime_s 替代 localtime
                std::tm timeInfo;
#ifdef _WIN32
                localtime_s(&timeInfo, &nowTime);
#else
                // 在非Windows平台上使用线程安全的 localtime_r
                localtime_r(&nowTime, &timeInfo);
#endif

                logFile_ << "--------------------------------------------------\n";
                logFile_ << "Log session ended at " << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
                    << " [" << (isCreator_ ? "Creator" : "Client") << "]\n";
                logFile_ << "--------------------------------------------------\n\n";
                logFile_.close();
            }
        }

        // 获取日志级别字符串
        std::string getLevelString(LogLevel level) {
            switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARNING";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Critical: return "CRITICAL";
            default: return "UNKNOWN";
            }
        }

    private:
        std::string logFilePath_;
        bool isCreator_;
        LogLevel minLevel_;
        std::ofstream logFile_;
        std::mutex mutex_;
    };

    // 创建端日志记录系统
    class SOLVERHUB_API CreatorLogger : public SharedMemoryLogger {
    public:
        CreatorLogger(const std::string& logFilePath, LogLevel minLevel = LogLevel::Info)
            : SharedMemoryLogger(logFilePath, true, minLevel) {
            logInfo("Creator process initialized");
        }

        ~CreatorLogger() override {
            logInfo("Creator process terminated");
        }
    };

    // 非创建端日志记录系统
    class SOLVERHUB_API ClientLogger : public SharedMemoryLogger {
    public:
        ClientLogger(const std::string& logFilePath, LogLevel minLevel = LogLevel::Info)
            : SharedMemoryLogger(logFilePath, false, minLevel) {
            logInfo("Client process initialized");
        }

        ~ClientLogger() override {
            logInfo("Client process terminated");
        }
    };

}

#endif // SHAREDMEMORYLOGGER_H