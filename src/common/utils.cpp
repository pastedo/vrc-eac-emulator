#include "utils.h"

#include <Windows.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

namespace {
	constexpr size_t log_file_max_size = 5 * 1024 * 1024;
	constexpr int log_file_generation_count = 3;

	void reset_log_files(const std::string& log_file_path) {
		const std::filesystem::path path(log_file_path);
		std::error_code error_code;
		std::filesystem::remove(path, error_code);

		const auto stem = path.stem().string();
		const auto extension = path.extension().string();
		for (int file_index = 1; file_index < log_file_generation_count; ++file_index) {
			const auto rotated_file_name = stem + "." + std::to_string(file_index) + extension;
			std::filesystem::remove(path.parent_path() / rotated_file_name, error_code);
		}
	}
}

void utils::create_console() {
	AllocConsole();

	FILE* newStdout;
	FILE* newStdin;
	freopen_s(&newStdout, "CONOUT$", "w", stdout);
	freopen_s(&newStdin, "CONIN$", "r", stdin);

	SetConsoleTitleA("EAC Emulator");
}

void utils::init_logger(bool use_colored, plog::Severity severity, const std::optional<std::string>& log_file_path) {
	static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
	static plog::ColorConsoleAppender<plog::TxtFormatter> coloredConsoleAppender;
	static std::unique_ptr<plog::RollingFileAppender<plog::TxtFormatter>> fileAppender;
	static std::string currentLogFilePath;
	static bool fileAppenderAttached = false;

	plog::IAppender* appender = use_colored ? &coloredConsoleAppender : &consoleAppender;
	auto& logger = plog::init(severity, appender);
	if (!log_file_path.has_value()) {
		return;
	}

	if (fileAppender == nullptr || currentLogFilePath != log_file_path.value()) {
		currentLogFilePath = log_file_path.value();
		reset_log_files(currentLogFilePath);
		fileAppender = std::make_unique<plog::RollingFileAppender<plog::TxtFormatter>>(currentLogFilePath.c_str(), log_file_max_size, log_file_generation_count);
		fileAppenderAttached = false;
	}

	if (!fileAppenderAttached) {
		logger.addAppender(fileAppender.get());
		fileAppenderAttached = true;
	}
}

void utils::sleep(unsigned int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
std::optional<std::string> utils::read_file(const std::string& path) {
	std::ifstream stream = std::ifstream(path);
	if (!stream) {
		stream.close();
		return std::nullopt;
	}

	std::string content = std::string(std::istreambuf_iterator(stream), std::istreambuf_iterator<char>());
	stream.close();
	return std::optional{content};
}
void utils::write_file(const std::string& path, const std::string& content) {
	std::ofstream stream = std::ofstream(path);
	stream << content;
	stream.close();
}
