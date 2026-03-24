#include "login_status_handler.h"

#include "common/api/requests/login_status_request.h"
#include "common/api/response/login_status_response.h"

#include "../eos/eos_connect.h"
#include "../eos/eos_platform.h"
#include "../servers/websocket_server.h"

std::shared_ptr<response> login_status_handler::handle(std::shared_ptr<request> request) {
	auto login_status = std::static_pointer_cast<login_status_request>(request);
	EOS_HConnect connect_interface = eos_platform::get_connect_interface();
	if (connect_interface == 0) {
		PLOGW.printf("Connect interface unavailable during login status query. Waiting for websocket reconnection and platform creation...");
		websocket_server::wait_for_connection();
		eos_platform::wait_for_platform_created();
		connect_interface = eos_platform::get_connect_interface();
	}

	EOS_ELoginStatus result = 0;
	if (connect_interface != 0) {
		result = eos_connect::get_login_status(connect_interface, login_status->user_id);
	} else {
		PLOGE.printf("Connect interface is still unavailable after waiting");
	}

	auto response = std::make_shared<login_status_response>();
	response->status = result;
	return response;
}
