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
#include <stdlib.h>
#include <unistd.h>
#include <chrono>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/miller_rabin.hpp>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#define CLIENT_MANAGER_GROUP_NAME "cmgroup"
#define WORKER_GROUP_NAME "wgroup"
#define MAX_ITERATIONS 1000


using std::cout;
using std::endl;
using std::vector;
using std::string;
using std::chrono::high_resolution_clock;

using boost::multiprecision::int512_t;
using boost::multiprecision::miller_rabin_test;

//using namespace std; //TEST
using namespace caf;


// random genrator for 512-bit values
typedef boost::random::independent_bits_engine<boost::random::mt19937, 512, int512_t> generator_type;
generator_type random_gen;

// prototypes
//void client(event_based_actor* self, const string& host, long port, int512_t n, const group& server);
void manager(event_based_actor* self, long workers, const string& host, long port, const group& server);
//void worker(event_based_actor* self, const group& server, int512_t state_id);

// prime-detection
inline bool is_probable_prime(const int512_t& value) {
    // increase 25 to a higher value for higher accuracy
    return miller_rabin_test(value, 25);
}

/*
void server(event_based_actor* self, long port) {
    cout << "server: server()" << endl;

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
//            cout << "Server: got PING! " << endl;
//            cout << "Server: sending PING! " << endl;

//            self->send(clnt_grp, atom("clntping"));
//        },
        on(atom("quit")) >> [=] {
            cout << "server: got quit message -> quitting! " << endl;
            self->quit();
        },
//        on(atom("ping"), arg_match) >> [=] (actor w) -> message {
//            cout << "server: got PING! " << endl;
//            cout << "server: sending PING! " << endl;

//            return make_message(atom("ping"));
//        }
        others() >> [=] {
            cout << to_string(self->last_dequeued()) << endl;
        }
    );
}
*/

void worker(event_based_actor* self, const group& server, int512_t state_id) {
    cout << "worker: worker()" << endl;

    self->become (
        on(atom("calculate"), arg_match) >> [=] (int512_t n) {
            cout << "worker: calculate" << endl;
            // variables for calculation-analysing
            int512_t a = random_gen() % n + 1;
            int512_t cycles = 0;
            std::clock_t cpu_time = std::clock();
            // send a continuation message to self
            self->send(self, atom("continue"), n, a, cycles, cpu_time, (state_id + 1));
            // change state to accept continuation
            worker(self, server, state_id + 1);
        },
        on(atom("continue"), arg_match) >> [=] (int512_t n, int512_t a, int512_t cycles, clock_t cpu_time, int512_t state) {
            cout << "worker: continue" << endl;
            if (state != state_id) {
                return; // message is obsolete, new calculation is already running
            }
            // calculate factor and send result to manager
            cout << "worker: calculation progress for N =  " << n << " in progress..." << endl;
            cout << "current state: " << state << endl;

            int512_t x = random_gen() % n + 1;
            if (x == 0) { x++; } // assert: 0 < x < N
            int512_t y = x;
            int512_t d = 0;
            int512_t p = 1;
            int512_t limit = boost::multiprecision::pow(n, 1/4); // 4th square-root N

            // start computation for MAX_ITERATIONS
            for(int i = 0; i < MAX_ITERATIONS; i++) {
                x = (x * x + a) % n;
                y = (y * y + a) % n;
                y = (y * y + a) % n;

                if ( (y - x) > 0) {
                    d = (y - x) % n;
                } else {
                    d = -(y - x) % n;
                }

                p = boost::math::gcd(d, n);

                if(p != 1) {
                    if(p != n || is_probable_prime(p)) {
                        cout << "worker: found factor: " << p << endl;
                        self->send(server, atom("factor"), p, cycles + i, cpu_time);
                    } else {
                        self->send(self, atom("continue"), n, a, cycles + i, cpu_time, state);
                    }
                    return;
                } else {
                    if (cycles > limit) {
                        self->send(server, atom("nofactor"), cpu_time);
                        self->send(self, atom("calculate"), n);
                        return;
                    }
                }
            }
            self->send(self, atom("continue"), n, a, cycles + MAX_ITERATIONS, cpu_time, state);
        }
    );
}

/*
void worker_busy(event_based_actor *self, actor manager, int512_t a, int512_t n, int512_t x, int512_t y, int512_t cycles)
{
    //cout << "[Worker] Spawned ... " << endl;

    self->become(
    [=] (int512_t N)
        {
        cout << "[Worker" << a << "]: Received new N: " << N << endl;
        int512_t xrand = gen() % n + 1;
        worker_busy(self, manager, a, N, xrand, xrand, 0);
    },
        others() >> [=]
    {
            cout << "[Worker] Unexpected: " << to_string(self->last_dequeued()) << endl;
        },
    after(std::chrono::seconds(0)) >> [=]
    {
      int512_t xn = x;
      int512_t yn = y;
      int512_t limit = 2*boost::multiprecision::sqrt((boost::multiprecision::sqrt(n)));
      //std::clock_t time = std::clock();
      for(int i=0; i<MAX_ITERATION; i++) {
          xn = (xn*xn + a) % n;
          yn = (yn*yn + a) % n;
          yn = (yn*yn + a) % n;
          int512_t d  = (yn - xn) % n;
          if(d < 0) d *= -1;
          int512_t p  = boost::math::gcd(d, n);
        if(p != 1)
            {
                if(p != n || is_probable_prime(p))
        {
            cout << "[Worker" << a << "]: Factor: " << p << endl;
            self->send(manager, p, cycles+i);
            worker(self, manager,  a);
            }
        else
        {
            int512_t xrand = gen() % n + 1;
            worker_busy(self, manager, a, n, xrand, xrand, 0);
        }
        return;
        }
        else
        {
        if(cycles > limit)
        {
            int512_t xrand = gen() % n + 1;
            worker_busy(self, manager, a, n, xrand, xrand, 0);
            return;
        }
        }
          }
      //cout << (std::clock()-time) / (double)CLOCKS_PER_SEC << endl;
          worker_busy(self, manager, a, n, xn, yn, cycles+1000);

    }
    );
}
*/

void manager(event_based_actor* self, long workers, const string& host, long port, const group& server) {
    cout << "manager: manager()" << endl;

    std::ostringstream group_addr;
    group_addr << CLIENT_MANAGER_GROUP_NAME << "@" << host << ":" << port;

    //test
    group wkr_grp = group::get("local", WORKER_GROUP_NAME);
    self->join(wkr_grp);

    // connect to server if needed
    if (!server) {
        cout << "manager: trying to connect to: " << host << ":" << port << endl;

        try {
            //auto new_serv = io::remote_actor(host, port);
            //self->monitor(new_serv);
            group new_serv = io::remote_group(group_addr.str());
            self->join(new_serv);
            cout << "manager: connection to server succeeded" << endl;
            self->send(self, atom("sWorkers"));
            manager(self, workers, host, port, new_serv);
            return;
        } catch (std::exception&) {
            cout << "manager: connection to server failed -> quitting" << endl;
            self->quit();
        }
    }

    self->become (
        on(atom("ping")) >> [=] {
            cout << "Manager: got PING! " << endl;
            cout << "Manager: sending PONG! " << endl;

            self->send(server, atom("pong"));
        },
        on(atom("sWorkers")) >> [=] {
            cout << "manager: spawning workers: " << workers << endl;

            int i;
            for(i=0; i<workers; i++) {
                spawn_in_group(wkr_grp, worker, wkr_grp, 0); // 0 = start state
            }

        },
        on(atom("kWorkers")) >> [=] {
            cout << "manager: killing workers: " << workers << endl;

            int i;
            for(i=0; i<workers; i++) {
                self->send(wkr_grp, atom("suicide"));
            }

        },
        on(atom("factor"), arg_match) >> [=] (int512_t p, int512_t cycles, std::clock_t cpu_time) {
            cout << "manager: got factor: " << p << endl;
            cout << "manager: forwarding factor to client" << endl;

            self->send(server, atom("factor"), p, cycles, cpu_time);
        },
        on(atom("nofactor"), arg_match) >> [=] (std::clock_t cpu_time) {
            cout << "manager: got no factor: " << endl;
            cout << "manager: forwarding factor to client" << endl;

            self->send(server, atom("nofactor"), cpu_time);
        },
        on(atom("calculate"), arg_match) >> [=] (int512_t n) {
            cout << "manager: calculate: " << n << endl;
            cout << "manager: forwarding to worker" << endl;

            self->send(wkr_grp, atom("calculate"), n);
        },
        on(atom("quit")) >> [=] {
            cout << "manager: got quit message -> quitting! " << endl;
            self->quit();
        } //,
//        others() >> [=] {
//            cout << to_string(self->last_dequeued()) << endl;
//        }
    );
}

void client(event_based_actor* self, const string& host, long port, int512_t n, const group& server,
            int512_t cycles, std::clock_t cpu_time, std::chrono::high_resolution_clock::time_point wall_time) {

    cout << "client: client()" << endl;

    std::ostringstream group_addr;
    group_addr << CLIENT_MANAGER_GROUP_NAME << "@" << host << ":" << port;

    // connect to server if needed
    if (!server) {
        cout << "client: trying to connect to: " << host << ":" << port << endl;

        try {
            //auto new_serv = io::remote_actor(host, port);
            //self->monitor(new_serv);
            //auto new_serv = group::get("remote", "clientgroup@localhost:6667");
            group new_serv = io::remote_group(group_addr.str());
            self->join(new_serv);
            cout << "client: connection to server succeeded" << endl;
            self->send(new_serv, atom("calculate"), n);
            client(self, host, port, n, new_serv, cycles, cpu_time, wall_time);
            return;
        } catch (std::exception&) {
            cout << "client: connection to server failed -> quitting" << endl;
            self->quit();
        }
    }

    self->become (
        on(atom("result"), arg_match) >> [=] () {
            cout << "client: got quit message -> quitting! " << endl;
            self->quit();
        },
        on(atom("pong")) >> [=] {
            cout << "client: got PONG! " << endl;
            cout << "client: sending PING! " << endl;

            self->send(server, atom("ping"));
        },
        on(atom("factor"), arg_match) >> [=] (int512_t p, int512_t new_cycles, std::clock_t new_cpu_time) {
            cout << "client: got factor: " << p << endl;

            int512_t tmp_n = n;
            int512_t tmp_cycles = cycles + new_cycles;
            std::clock_t tmp_cpu_time = cpu_time + new_cpu_time;

            cout << p << " ";

            while(!(tmp_n % p)) {
                tmp_n /= p;
                cout << p << " ";
            }

            if((tmp_n / p) == 1) {
                cout << "***** PRIME FACTORISATION *****" << endl << endl;
                cout << "CPU-Time: " << (std::clock() - tmp_cpu_time) / (double)CLOCKS_PER_SEC;
                cout << "Wall-Time: " <<  std::chrono::duration_cast<std::chrono::duration<double>>(high_resolution_clock::now() - wall_time).count() << endl;
                cout << "Cycles: " << tmp_cycles << endl;
            } else {
                self->send(server, atom("calculate"), tmp_n);
                client(self, host, port, tmp_n, server, tmp_cycles, tmp_cpu_time, wall_time);
            }
        },
        on(atom("nofactor"), arg_match) >> [=] (std::clock_t new_cpu_time) {
            cout << "client: got no factor" << endl;

            std::clock_t tmp_cpu_time = cpu_time + new_cpu_time;
            client(self, host, port, n, server, cycles, tmp_cpu_time, wall_time);
        }
//        others() >> [=] {
//            cout << to_string(self->last_dequeued()) << endl;
//        }
    );
}

/*
void worker(event_based_actor* self) {
    cout << "worker: worker()" << endl;

    self->become (
        on(atom("suicide")) >> [=] {
            cout << "worker: got suicide message -> ARGH!!! " << endl;
            cout << "worker: ARGH!!! " << endl;
            self->quit();
        },
        on(atom("ping"), arg_match) >> [=] (actor w) -> message {
            cout << "server: got PING! " << endl;
            cout << "server: sending PONG! " << endl;

            return make_message(atom("pong"));
        }
    );
}
*/

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
    } catch (std::exception& e) {
        std::cerr << to_verbose_string(e) // prints exception type and e.what()
        << endl;
    }

    while(getline(std::cin, line)) {
        if (line == "quit") {
           break;
        } else {
            std::cerr << "illegal command" << endl;
        }
    }
}

void run_manager(long workers, const string& host, long port) {
    cout << "run_manager(), num_workers: " << workers << " server_address: " << host << " server_port: " << port << endl;

    spawn(manager, workers, host, port, invalid_group);
}

void run_client(const string& host, long port) {
    int512_t n;
    std::chrono::high_resolution_clock::time_point wall_time;

    cout << "run_client(), " << " server_address: " << host << " server_port: " << port << endl;

    cout << "### Client Input: ####\n\nPlease enter the number to calculate : ";
    std::cin >> n;

    wall_time = high_resolution_clock::now();
    auto client_handle = spawn(client, host, port, n, invalid_group, 0, 0, wall_time); // cycles and cpu_time can be 0
    //anon_send(client_handle, atom("pong"));
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
