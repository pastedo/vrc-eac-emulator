#include "client_packet_handler.h"

#include <memory>

#include "base64.hpp"
#include "common/protocol/packet_id.h"
#include "common/protocol/packets/handshake_packet.h"
#include "common/protocol/packets/notify_message_to_server_packet.h"
#include "plog/Log.h"
#include "win/emulator_client.h"
#include "win/eos_impl/eos_general_impl.h"
#include "win/eos_impl/eos_anticheat_impl.h"
#include "win/eos_impl/eos_platform_impl.h"
#include "win/sock/packet_sender.h"

client_packet_handler::client_packet_handler(std::weak_ptr<packet_sender> sender) : sender(sender) {
	registry.register_handler(HANDSHAKE_PACKET_ID, std::bind(&client_packet_handler::handle_handshake, this, std::placeholders::_1));
	registry.register_handler(NOTIFY_MESSAGE_TO_SERVER_PACKET_ID, std::bind(&client_packet_handler::handle_notify_msg_to_server, this, std::placeholders::_1));
}

void client_packet_handler::on_connected() {
	PLOGI.printf("Sending handshake packet to the server");
	auto packet = std::make_shared<handshake_packet>();
	packet->timestamp = std::time(nullptr);
	send_packet(packet);
	replay_eos_initialize_if_needed();
	replay_platform_create_if_needed();
	recreate_anticheat_session_if_needed();
}

handler_registry& client_packet_handler::get_handler_registry() {
	return registry;
}

void client_packet_handler::add_notify_message_to_server(notify_message_to_server_callback callback) {
	std::lock_guard lock(notify_message_to_server_callbacks_mutex);
	notify_message_to_server_callbacks.push_back(callback);
}

void client_packet_handler::replay_notify_message_to_server_bindings() {
	std::vector<notify_message_to_server_callback> callbacks;
	{
		std::lock_guard lock(notify_message_to_server_callbacks_mutex);
		callbacks = notify_message_to_server_callbacks;
	}

	if (callbacks.empty()) {
		return;
	}

	auto* client = emulator_client::get_instance();
	if (client == nullptr) {
		return;
	}

	for (const auto& callback : callbacks) {
		auto packet = std::make_shared<notify_message_to_server_packet>();
		packet->require_bind = true;
		packet->options.ApiVersion = callback.options.ApiVersion;
		client->send_packet(packet);
	}

	PLOGI.printf("Replayed %d AntiCheat notify binding(s)", static_cast<int>(callbacks.size()));
}

void client_packet_handler::handle_handshake(std::shared_ptr<packet> packet) {
	PLOGD.printf("Session successfully created: timestamp=%lld", std::static_pointer_cast<handshake_packet>(packet)->timestamp);
}

void client_packet_handler::handle_notify_msg_to_server(std::shared_ptr<packet> packet) {
	auto notify_message_to_server = std::static_pointer_cast<notify_message_to_server_packet>(packet);
	auto decoded = base64::decode_into<std::vector<char>>(notify_message_to_server->base64_message_data.c_str());
	if (decoded.size() != notify_message_to_server->message_data_size) {
		PLOGE.printf("Decoded message size does not match message data size");
		return;
	}

	std::vector<notify_message_to_server_callback> callbacks;
	{
		std::lock_guard lock(notify_message_to_server_callbacks_mutex);
		callbacks = notify_message_to_server_callbacks;
	}

	for (auto& callback : callbacks) {
		EOS_AntiCheatClient_OnMessageToServerCallbackInfo info{};
		info.ClientData = callback.client_data;
		info.MessageData = decoded.data();
		info.MessageDataSizeBytes = notify_message_to_server->message_data_size;
		callback.notification_fn(&info);
	}
}

void client_packet_handler::send_packet(std::shared_ptr<packet> packet) {
	if (std::shared_ptr<packet_sender> ptr = sender.lock()) {
		ptr->send_packet(packet);
	}
}
