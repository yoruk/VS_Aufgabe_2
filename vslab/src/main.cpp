/*
 * main.cpp
 *
 *  Created on: 20.11.2014
 *      Author: Eugen Winter, Michael Schmidt
 */

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/miller_rabin.hpp>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#define CLIENT_MANAGER_GROUP_NAME "cmgroup"
#define WORKER_GROUP_NAME "wgroup"

//using std::cout;
//using std::endl;
//using std::vector;
//using std::string;

using boost::multiprecision::int512_t;
using boost::multiprecision::miller_rabin_test;

using namespace std;
using namespace caf;

inline bool is_probable_prime(const int512_t& value) {
    // increase 25 to a higher value for higher accuracy
    return miller_rabin_test(value, 25);
}

void server(event_based_actor* self, long port);
void manager(event_based_actor* self, long workers, const string& host, long port, const group& server);
void worker(event_based_actor* self);

void server(event_based_actor* self, long port) {
    aout(self) << "server: server()" << endl;

    group clnt_mngr_grp;

    try {
        clnt_mngr_grp = group::get("local", CLIENT_MANAGER_GROUP_NAME);
        self->join(clnt_mngr_grp);
        io::publish_local_groups(port);
    } catch (exception& e) {
        cerr << to_verbose_string(e) // prints exception type and e.what()
             << endl;
    }

    self->become (
//        on(atom("srvping")) >> [=] {
//            aout(self) << "Server: got PING! " << endl;
//            aout(self) << "Server: sending PING! " << endl;

//            self->send(clnt_grp, atom("clntping"));
//        },
        on(atom("quit")) >> [=] {
            aout(self) << "server: got quit message -> quitting! " << endl;
            self->quit();
        },
//        on(atom("ping"), arg_match) >> [=] (actor w) -> message {
//            aout(self) << "server: got PING! " << endl;
//            aout(self) << "server: sending PING! " << endl;

//            return make_message(atom("ping"));
//        }
        others() >> [=] {
            aout(self) << to_string(self->last_dequeued()) << endl;
        }
    );
}

void manager(event_based_actor* self, long workers, const string& host, long port, const group& server) {
    aout(self) << "manager: manager()" << endl;

    std::ostringstream group_addr;
    group_addr << CLIENT_MANAGER_GROUP_NAME << "@" << host << ":" << port;

    //test
    group wkr_grp = group::get("local", WORKER_GROUP_NAME);
    self->join(wkr_grp);

    // connect to server if needed
    if (!server) {
        aout(self) << "manager: trying to connect to: " << host << ":" << port << endl;

        try {
            //auto new_serv = io::remote_actor(host, port);
            //self->monitor(new_serv);
            group new_serv = io::remote_group(group_addr.str());
            aout(self) << "manager: connection to server succeeded" << endl;
            self->send(self, atom("sWorkers"));
            manager(self, workers, host, port, new_serv);
            return;
        } catch (exception&) {
            aout(self) << "manager: connection to server failed -> quitting" << endl;
            self->quit();
        }
    }

    self->become (
        on(atom("ping")) >> [=] {
            aout(self) << "Manager: got PING! " << endl;
            aout(self) << "Manager: sending PONG! " << endl;

            self->send(server, atom("pong"));
        },
        on(atom("sWorkers")) >> [=] {
            aout(self) << "manager: spawning workers: " << workers << endl;

            int i;
            for(i=0; i<workers; i++) {
                spawn_in_group(wkr_grp, worker);
            }

        },
        on(atom("kWorkers")) >> [=] {
            aout(self) << "manager: killing workers: " << workers << endl;

            int i;
            for(i=0; i<workers; i++) {
                self->send(wkr_grp, atom("suicide"));
            }

        },
        on(atom("quit")) >> [=] {
            aout(self) << "manager: got quit message -> quitting! " << endl;
            self->quit();
        },
        others() >> [=] {
            aout(self) << to_string(self->last_dequeued()) << endl;
        }
    );
}

void client(event_based_actor* self, const string& host, long port, int512_t n, const group& server) {
    aout(self) << "client: client()" << endl;

    std::ostringstream group_addr;
    group_addr << CLIENT_MANAGER_GROUP_NAME << "@" << host << ":" << port;

    // connect to server if needed
    if (!server) {
        aout(self) << "client: trying to connect to: " << host << ":" << port << endl;

        try {
            //auto new_serv = io::remote_actor(host, port);
            //self->monitor(new_serv);
            //auto new_serv = group::get("remote", "clientgroup@localhost:6667");
            group new_serv = io::remote_group(group_addr.str());
            aout(self) << "client: connection to server succeeded" << endl;
            client(self, host, port, n, new_serv);
            return;
        } catch (exception&) {
            aout(self) << "client: connection to server failed -> quitting" << endl;
            self->quit();
        }
    }

    self->become (
        on(atom("quit")) >> [=] {
            aout(self) << "client: got quit message -> quitting! " << endl;
            self->quit();
        },
        on(atom("pong")) >> [=] {
            aout(self) << "client: got PONG! " << endl;
            aout(self) << "client: sending PING! " << endl;

            self->send(server, atom("ping"));
        },
        others() >> [=] {
            aout(self) << to_string(self->last_dequeued()) << endl;
        }
    );
}

void worker(event_based_actor* self) {
    aout(self) << "worker: worker()" << endl;

    self->become (
        on(atom("suicide")) >> [=] {
            aout(self) << "worker: got suicide message -> ARGH!!! " << endl;
            aout(self) << "worker: ARGH!!! " << endl;
            self->quit();
        },
        on(atom("ping"), arg_match) >> [=] (actor w) -> message {
            aout(self) << "server: got PING! " << endl;
            aout(self) << "server: sending PONG! " << endl;

            return make_message(atom("pong"));
        }
    );
}

void run_server(long port) {
    string line;

    cout << "run_server(), using port: " << port << endl;

//    try {
//        // try to publish server actor at given port
//        io::publish(spawn(server, port), port);
//    } catch (exception& e) {
//        cerr << "run_server: unable to publish server actor at port " << port << "\n"
//             << to_verbose_string(e) // prints exception type and e.what()
//             << endl;
//    }

//    spawn(server, port);

    try {
        io::publish_local_groups(port);
    } catch (exception& e) {
        cerr << to_verbose_string(e) // prints exception type and e.what()
        << endl;
    }

    while(getline(cin, line)) {
        if (line == "quit") {
           break;
        } else {
            cerr << "illegal command" << endl;
        }
    }
}

void run_manager(long workers, const string& host, long port) {
    cout << "run_manager(), num_workers: " << workers << " server_address: " << host << " server_port: " << port << endl;

    spawn(manager, workers, host, port, invalid_group);
}

void run_client(const string& host, long port) {
    int512_t n;

    cout << "run_client(), " << " server_address: " << host << " server_port: " << port << endl;

    cout << "### Client Input: ####\n\nPlease enter the number to calculate : ";
    cin >> n;

    auto client_handle = spawn(client, host, port, n, invalid_group);
    anon_send(client_handle, atom("pong"));
}

// projection: string => long
optional<long> long_arg(const string& arg) {
    char* last = nullptr;
    auto res = strtol(arg.c_str(), &last, 10);

    if (last == (arg.c_str() + arg.size())) {
        return res;
    }

    return none;
}

// tell CAF how to serialize int512_t
class int512_t_uti : public caf::detail::abstract_uniform_type_info<int512_t> {
protected:
    void serialize(const void* vptr, caf::serializer* sink) const override {
        std::ostringstream oss;
        oss << *reinterpret_cast<const int512_t*>(vptr);
        sink->write(oss.str());
    }

    void deserialize(void* vptr, caf::deserializer* source) const override {
        std::istringstream iss{source->read<string>()};
        iss >> *reinterpret_cast<int512_t*>(vptr);
    }
};

int main(int argc, char** argv) {
  // allow int512_t and vector<int512_t> to be used in messages
  announce(typeid(int512_t), uniform_type_info_ptr{new int512_t_uti});
  announce<vector<int512_t>>();

  // parse command line arguments
	message_builder{argv + 1, argv + argc}.apply({
		on("-s", long_arg) >> run_server,
		on("-m", long_arg, val<string>, long_arg) >> run_manager,
		on("-c", val<string>, long_arg) >> run_client,
		others() >> [] {
			cout << "usage:" << endl
			<< "  server:  -s <PORT>" << endl
			<< "  manager: -m <NUM_WORKERS> <SERVER_HOST> <SERVER_PORT>" << endl
			<< "  client:  -c <SERVER_HOST> <SERVER_PORT>" << endl
			<< endl;
		}
	});

	await_all_actors_done();
	shutdown();

    return 0;
}
