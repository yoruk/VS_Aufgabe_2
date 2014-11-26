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

void server(event_based_actor* self);
void manager(event_based_actor* self, long workers, const string& host, long port, const actor& server);
void worker(event_based_actor* self);

void server(event_based_actor* self) {
    aout(self) << "server: server()" << endl;

    self->become (
        on(atom("quit")) >> [=] {
            self->quit();
        }
    );

    //self->quit();
}

void manager(event_based_actor* self, long workers, const string& host, long port, const actor& server) {
    aout(self) << "manager: manager()" << endl;

    auto grp = group::get("local", "tolle worker");
    self->join(grp);

    // connect to server if needed
    if (!server) {
        aout(self) << "manager: trying to connect to: " << host << ":" << port << endl;

        try {
            auto new_serv = io::remote_actor(host, port);
            self->monitor(new_serv);
            aout(self) << "manager: reconnection to server succeeded" << endl;
            self->sync_send(self, atom("sWorkers"));
        } catch (exception&) {
            aout(self) << "manager: connection to server failed, quitting" << endl;
            self->quit();
            //self->delayed_send(self, chrono::seconds(3), atom("reconnect"));
        }
    }

    self->become (
        on(atom("reconnect")) >> [=] {
            aout(self) << "manager: trying to reconnect" << endl;
            manager(self, workers, host, port, invalid_actor);
        },
        on(atom("sWorkers")) >> [=] {
            aout(self) << "manager: spawning workers: " << workers << endl;
            //self->delayed_send(self, chrono::seconds(3), atom("sWorkers"));

            int i;
            for(i=0; i<workers; i++) {
                spawn_in_group(grp, worker);
            }

        },
        on(atom("kWorkers")) >> [=] {
            aout(self) << "manager: killing workers: " << workers << endl;

            // hier worker killen
            int i;
            for(i=0; i<workers; i++) {
                self->send(grp, atom("suicide"));
            }

        },
        on(atom("quit")) >> [=] {
            self->quit();
        }
    );
}

void client(event_based_actor* self, const string& host, long port, int512_t n, const actor& server) {
    aout(self) << "client: client()" << endl;

    // connect to server if needed
    if (!server) {
        aout(self) << "client: trying to connect to: " << host << ":" << port << endl;

        try {
            auto new_serv = io::remote_actor(host, port);
            self->monitor(new_serv);
            aout(self) << "client: reconnection to server succeeded" << endl;
        } catch (exception&) {
            aout(self) << "client: connection to server failed, quitting" << endl;
            self->quit();
            //self->delayed_send(self, chrono::seconds(3), atom("reconnect"));
        }
    }

    self->become (
        on(atom("quit")) >> [=] {
            self->quit();
        }
    );
}

void worker(event_based_actor* self) {
    aout(self) << "worker: worker()" << endl;

    self->become (
        on(atom("suicide")) >> [=] {
            aout(self) << "worker: ARGH!!! " << endl;
            self->quit();
        }
    );
}

void run_server(long port) {
    cout << "run_server(), using port: " << port << endl;

    try {
        // try to publish server actor at given port
        io::publish(spawn(server), port);
    } catch (exception& e) {
        cerr << "*** unable to publish server actor at port " << port << "\n"
             << to_verbose_string(e) // prints exception type and e.what()
             << endl;
    }
}

void run_manager(long workers, const string& host, long port) {
    cout << "run_manager(), num_workers: " << workers << " server_address: " << host << " server_port: " << port << endl;

    spawn(manager, workers, host, port, invalid_actor);
}

void run_client(const string& host, long port) {
    int512_t n;

    cout << "run_client(), " << " server_address: " << host << " server_port: " << port << endl;

    cout << "### Client Input: ####\n\nPlease enter the number to calculate : ";
    cin >> n;

    spawn(client, host, port, n, invalid_actor);
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
