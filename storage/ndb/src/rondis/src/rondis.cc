/*
   Copyright (c) 2024, 2024, Hopsworks and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signal.h>
#include <atomic>
#include <mutex>

#include <ndbapi/NdbApi.hpp>
#include <ndbapi/Ndb.hpp>
#include "server_thread.h"
#include "pink_conn.h"
#include "redis_conn.h"
#include "pink_thread.h"
#include "dispatch_thread.h"
#include "rondb.h"
#include "common.h"

using namespace pink;

std::vector<Ndb *> ndb_objects;
std::map<std::string, std::string> db;

class RondisHandle : public ServerHandle
{
public:
    RondisHandle() : counter(0) {}

    /*
        We define this so each connection knows from which worker thread it is
        running from. This enables us to to distribute Ndb objects across
        multiple worker threads.
    */
    int CreateWorkerSpecificData(void **data) const override
    {
        std::lock_guard<std::mutex> lock(mutex);
        *data = new int(counter++);
        return 0;
    }

private:
    mutable std::mutex mutex;
    mutable int counter;
};

class RondisConn : public RedisConn
{
public:
    RondisConn(
        int fd,
        const std::string &ip_port,
        Thread *thread,
        void *worker_specific_data);
    virtual ~RondisConn() = default;

protected:
    int DealMessage(const RedisCmdArgsType &argv, std::string *response) override;

private:
    int _worker_id;
};

RondisConn::RondisConn(
    int fd,
    const std::string &ip_port,
    Thread *thread,
    void *worker_specific_data)
    : RedisConn(fd, ip_port, thread)
{
    int worker_id = *static_cast<int *>(worker_specific_data);
    _worker_id = worker_id;
}

int RondisConn::DealMessage(const RedisCmdArgsType &argv, std::string *response)
{
    /*    
        printf("Received Redis message: ");
        for (int i = 0; i < argv.size(); i++)
        {
            printf("%s ", argv[i].c_str());
        }
        printf("\n");
    */
    return rondb_redis_handler(argv, response, _worker_id);
}

class RondisConnFactory : public ConnFactory
{
public:
    virtual std::shared_ptr<PinkConn> NewPinkConn(
        int connfd,
        const std::string &ip_port,
        Thread *thread,
        void *worker_specific_data,
        [[maybe_unused]]/*todo remove?*/ pink::PinkEpoll *pink_epoll = nullptr
    ) const override
    {
        return std::make_shared<RondisConn>(connfd, ip_port, thread, worker_specific_data);
    }
};

static std::atomic<bool> running(false);

static void IntSigHandle(const int sig)
{
    printf("Catch Signal %d, cleanup...\n", sig);
    running.store(false);
    printf("server Exit");
}

static void SignalSetup()
{
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, &IntSigHandle);
    signal(SIGQUIT, &IntSigHandle);
    signal(SIGTERM, &IntSigHandle);
}

int main(int argc, char *argv[])
{
    int port = 6379;
    const char *connect_string = "localhost:13000";
    int worker_threads = 2;
    if (argc != 4)
    {
        printf("Not receiving 3 arguments, just using defaults\n");
    }
    else
    {
        port = atoi(argv[1]);
        connect_string = argv[2];
        worker_threads = atoi(argv[3]);
    }
    printf("Server will listen to %d and connect to MGMd at %s\n", port, connect_string);

    if (worker_threads < MAX_CONNECTIONS) {
        printf("Number of worker threads must be at least %d, otherwise we are wasting resources\n", MAX_CONNECTIONS);
        return -1;
    }

    ndb_objects.resize(worker_threads);

    if (setup_rondb(connect_string, worker_threads) != 0)
    {
        printf("Failed to setup RonDB environment\n");
        return -1;
    }
    SignalSetup();

    ConnFactory *conn_factory = new RondisConnFactory();

    RondisHandle *handle = new RondisHandle();

    ServerThread *my_thread = NewDispatchThread(port, worker_threads, conn_factory, 1000, 1000, handle);
    if (my_thread->StartThread() != 0)
    {
        printf("StartThread error happened!\n");
        rondb_end();
        return -1;
    }

    running.store(true);
    while (running.load())
    {
        sleep(1);
    }
    my_thread->StopThread();

    delete my_thread;
    delete conn_factory;

    rondb_end();

    return 0;
}
