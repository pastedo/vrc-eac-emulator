#include "eos_platform.h"

#include <condition_variable>
#include <mutex>
#include <plog/Log.h>
#include "common/eos/eos_api.h"
#include "eos_utils.h"

namespace {
	std::mutex platform_mutex;
	std::condition_variable platform_created_cv;
	EOS_HPlatform hPlatform = NULL;
}

void eos_platform::create_platform(EOS_Platform_Options const& options) {
	typedef EOS_HPlatform (EOS_CALL*EOS_Platform_Create)(EOS_Platform_Options const*);
	static auto eos_create_platform = reinterpret_cast<EOS_Platform_Create>(
		eos_utils::get_eos_proc_addr("EOS_Platform_Create"));
	if (!eos_create_platform) {
		PLOGF.printf("Could not resolve EOS_Platform_Create");
		return;
	}

	auto platform = eos_create_platform(&options);
	if (!platform) {
		PLOGE.printf("Failed to call EOS_Platform_Create!");
		return;
	}
	{
		std::lock_guard lock(platform_mutex);
		hPlatform = platform;
	}
	platform_created_cv.notify_all();
	PLOGI.printf("Platform Created: product_id=%s, client_id=%s", options.ProductId, options.ClientCredentials.ClientId);
}

void eos_platform::tick() {
	typedef void (EOS_CALL*EOS_Platform_Tick)(EOS_HPlatform);
	static auto eos_platform_tick = reinterpret_cast<EOS_Platform_Tick>(
		eos_utils::get_eos_proc_addr("EOS_Platform_Tick"));
	if (!eos_platform_tick) {
		PLOGF.printf("Could not resolve EOS_Platform_Tick");
		return;
	}

	EOS_HPlatform platform = NULL;
	{
		std::lock_guard lock(platform_mutex);
		platform = hPlatform;
	}

	eos_platform_tick(platform);
}

EOS_HConnect eos_platform::get_connect_interface() {
	typedef EOS_HConnect (EOS_CALL*EOS_Platform_GetConnectInterface)(EOS_HPlatform);
	static auto eos_platform_get_connect_interface = reinterpret_cast<EOS_Platform_GetConnectInterface>(
		eos_utils::get_eos_proc_addr("EOS_Platform_GetConnectInterface"));
	if (!eos_platform_get_connect_interface) {
		PLOGF.printf("Could not resolve EOS_Platform_GetConnectInterface");
		return 0;
	}

	EOS_HPlatform platform = NULL;
	{
		std::lock_guard lock(platform_mutex);
		platform = hPlatform;
	}

	if (platform == NULL) {
		PLOGW.printf("EOS_Platform_GetConnectInterface requested before platform creation");
		return 0;
	}

	EOS_HConnect hConnect = eos_platform_get_connect_interface(platform);
	if (!hConnect) {
		PLOGE.printf("Failed to call EOS_Platform_GetConnectInterface!");
		return 0;
	}

	return hConnect;
}

EOS_HAntiCheatClient eos_platform::get_anticheat_client_interface() {
	typedef EOS_HAntiCheatClient (EOS_CALL*EOS_Platform_GetAntiCheatClientInterface)(EOS_HPlatform);
	static auto eos_platform_get_anticheat_client_interface = reinterpret_cast<EOS_Platform_GetAntiCheatClientInterface>(
		eos_utils::get_eos_proc_addr("EOS_Platform_GetAntiCheatClientInterface"));
	if (!eos_platform_get_anticheat_client_interface) {
		PLOGF.printf("Could not resolve EOS_Platform_GetAntiCheatClientInterface");
		return 0;
	}

	EOS_HPlatform platform = NULL;
	{
		std::lock_guard lock(platform_mutex);
		platform = hPlatform;
	}

	return eos_platform_get_anticheat_client_interface(platform);
}

bool eos_platform::is_platform_created() {
	std::lock_guard lock(platform_mutex);
	return hPlatform != NULL;
}

void eos_platform::wait_for_platform_created() {
	std::unique_lock lock(platform_mutex);
	platform_created_cv.wait(lock, []() {
		return hPlatform != NULL;
	});
}
