#include "packet_sender.h"

#include <functional>
#include <thread>

#include "common/protocol/packet_codec.h"
#include "hv/wsdef.h"

void packet_sender::run_loopback() {
	while (true) {
		std::vector<std::shared_ptr<packet>> pending_packets;
		{
			std::unique_lock lock(send_mutex);
			cv.wait(lock, [this]() {
				return !queued_packet.empty();
			});
			pending_packets.swap(queued_packet);
		}

		for (auto& packet : pending_packets) {
			write_stream stream = packet_codec::encode(packet);
			auto buf = stream.as_buffer();
			websocket_client.send(static_cast<char*>(buf.data), buf.size, WS_OPCODE_BINARY);
			buf.free();
			PLOGD.printf("Packet sent: name=%s id=%d", packet->get_name().c_str(), packet->get_id());
		}
	}
}

packet_sender::packet_sender(hv::WebSocketClient& websocket_client) : websocket_client(websocket_client) {
}

void packet_sender::start() {
	std::thread(std::bind(&packet_sender::run_loopback, this)).detach();
}

void packet_sender::send_packet(std::shared_ptr<packet> packet) {
	{
		std::lock_guard lock(send_mutex);
		queued_packet.push_back(packet);
	}
	cv.notify_one();
}
