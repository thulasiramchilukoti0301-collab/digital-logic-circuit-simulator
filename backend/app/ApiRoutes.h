#pragma once

namespace httplib {
class Server;
}

namespace digital_logic::app {

void register_api_routes(httplib::Server& server);

} // namespace digital_logic::app
