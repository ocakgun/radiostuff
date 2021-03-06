#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>


#include "axlib.h"

using namespace axlib;

namespace {
void usage(const char* av0, int err)
{
    FILE* f = stdout;
    if (err) {
        f = stderr;
    }
    fprintf(f,
            "Usage: %s […options…] -s <src call> <dst>\n"
            "%s\nExample:\n"
            "   %s -k my.priv -P peer.pub -s M0XXX-9 -p M0XXX-0 2E0XXX-9\n",
            av0,
            common_usage().c_str(),
            av0);
    exit(err);
}
} // namespace

std::string xgetline(std::istream& stream, const size_t max, const bool discard_first = false)
{
        std::vector<char> buf(max+1); // Since getline fails at count-1 bytes.
        std::cin.getline(&buf[0], buf.size());

        if (std::cin.eof()) {
                return "";
        }

        // Failbit is set if count-1 bytes have been read.
        if (std::cin.fail()) {
                if (!discard_first) {
                        std::clog << "]]] Command too long. Discarding.\n";
                }
                std::cin.clear();
                return xgetline(stream, max, true);
        }

        // Got complete line, but it could be a tail end of a too large line.
        if (discard_first) {
                return xgetline(stream, max);
        }

        const auto len = std::cin.gcount();
        if (len == 0) {
                return "";
        }

        return std::string(&buf[0], &buf[len-1]);
}


int main(int argc, char** argv)
{
    CommonOpts copt;
    int opt;
    auto lopts = common_long_opts();
    lopts.push_back({ 0, 0, 0, 0 });
    while ((opt = getopt_long(argc, argv, "ehk:l:P:p:s:w:", &lopts[0], NULL)) != -1) {
        if (common_opt(copt, opt)) {
            continue;
        }
        switch (opt) {
        case 'h':
            usage(argv[0], EXIT_SUCCESS);
        default: /* '?' */
            usage(argv[0], EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Need dest\n");
        exit(EXIT_FAILURE);
    }
    const std::string dst = argv[optind];

    auto sock = make_from_commonopts(copt);

    std::clog << "Connecting...\n";
    if (sock->connect(dst)) {
        std::clog << "Failed to connect!\n";
        return 1;
    }
    std::clog << "Connected!\n";

    std::mutex m;
    std::string cmd;
    bool time_to_die = false;
    std::thread reader([&] {
        for (;;) {
            usleep(100000); // Prevent CPU busyloop waiting for command to be sent.
            {
                std::unique_lock<std::mutex> l(m);
                if (!cmd.empty()) {
                    continue;
                }
            }

            const auto line = xgetline(std::cin, sock->max_packet_size());

            if (line.empty() || line == "exit") {
                break;
            }
            if (!std::cin.good()) {
                throw std::runtime_error(std::string("stdin read failure: ") +
                                         strerror(errno));
            }
            std::unique_lock<std::mutex> l(m);
            cmd = line;
        }
        std::unique_lock<std::mutex> l(m);
        time_to_die = true;
    });

    // We have to use select() instead of a thread. A thread would be
    // nicer (no polling needed wakeups), but the Linux kernel doesn't
    // support threads calling write() and read() at the same time on
    // the same AX_25 socket. I've sent a patch:
    // https://marc.info/?l=linux-hams&m=159319049624305&w=2
    //
    // Yes, I could make this 100% event-based, but it's just so nice
    // to not have to deal with partial command buffers that I
    // preferred this.
    for (;;) {
        // Check if done.
        {
            std::unique_lock<std::mutex> l(m);
            if (time_to_die) {
                break;
            }
        }

        // Check if any data is received.
        {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock->get_fd(), &fds);
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            const auto rc = select(sock->get_fd() + 1, &fds, NULL, NULL, &tv);
            if (rc == 1) {
                std::cout << sock->read() << std::flush;
            } else if (rc == -1) {
                throw std::runtime_error(std::string("select(): ") + strerror(errno));
            }
        }

        // Write anything queued up to write.
        {
            std::unique_lock<std::mutex> l(m);
            if (!cmd.empty()) {
                std::clog << "]]] Sending command <" << cmd << ">" << std::endl;
                sock->write(cmd);
                cmd = "";
            }
        }
    }
    reader.join();
    return 0;
}
