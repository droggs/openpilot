#include "tools/cabana/imgui/devicestream.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cereal/services.h"

DeviceStream::DeviceStream(std::string address) : zmq_address(std::move(address)), LiveStream() {
}

DeviceStream::~DeviceStream() {
  if (bridge_pid > 0) {
    kill(bridge_pid, SIGTERM);
    // Wait up to 3 seconds, then force kill (matching Qt's terminate/wait/kill pattern)
    for (int i = 0; i < 30; ++i) {
      int status;
      if (waitpid(bridge_pid, &status, WNOHANG) == bridge_pid) return;
      usleep(100000);
    }
    kill(bridge_pid, SIGKILL);
    int status;
    waitpid(bridge_pid, &status, 0);
  }
}

void DeviceStream::start() {
  // Idempotent: skip if already started (allows pre-start validation)
  if (bridge_pid > 0 || started_) return;
  started_ = true;

  if (!zmq_address.empty()) {
    // Get the path to the bridge binary
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    std::string bridge_path;
    if (len > 0) {
      exe_path[len] = '\0';
      std::string exe_dir(exe_path);
      auto pos = exe_dir.rfind('/');
      if (pos != std::string::npos) {
        bridge_path = exe_dir.substr(0, pos) + "/../../cereal/messaging/bridge";
      }
    }
    if (bridge_path.empty()) {
      bridge_path = "bridge";
    }

    // Resolve to absolute path
    char resolved[PATH_MAX];
    if (realpath(bridge_path.c_str(), resolved)) {
      bridge_path = resolved;
    }

    bridge_pid = fork();
    if (bridge_pid == 0) {
      // Child process
      execl(bridge_path.c_str(), "bridge", zmq_address.c_str(), "/\"can/\"", nullptr);
      _exit(1);
    } else if (bridge_pid < 0) {
      last_error_ = "Failed to fork bridge process";
      fprintf(stderr, "%s\n", last_error_.c_str());
      return;
    }
    // Give the bridge a moment to start, then check it didn't exit immediately
    usleep(500000);
    int status;
    pid_t result = waitpid(bridge_pid, &status, WNOHANG);
    if (result == bridge_pid) {
      char err_buf[256];
      snprintf(err_buf, sizeof(err_buf), "Bridge process exited immediately (exit code %d). Is '%s' installed?",
               WIFEXITED(status) ? WEXITSTATUS(status) : -1, bridge_path.c_str());
      last_error_ = err_buf;
      fprintf(stderr, "%s\n", last_error_.c_str());
      bridge_pid = -1;
      return;  // Abort — don't start streaming without a working bridge
    }
  }

  LiveStream::start();
}

void DeviceStream::streamThread() {
  zmq_address.empty() ? unsetenv("ZMQ") : setenv("ZMQ", "1", 1);

  std::unique_ptr<Context> context(Context::create());
  std::unique_ptr<SubSocket> sock(SubSocket::create(context.get(), "can", "127.0.0.1", false, true, services.at("can").queue_size));
  assert(sock != NULL);

  while (!stop_requested_) {
    std::unique_ptr<Message> msg(sock->receive(true));
    if (!msg) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    handleEvent(kj::ArrayPtr<capnp::word>((capnp::word*)msg->getData(), msg->getSize() / sizeof(capnp::word)));
  }
}
