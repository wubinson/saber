#include <stdio.h>

#include <vector>

#include <saber/client/saber.h>
#include <saber/util/countdownlatch.h>
#include <saber/util/logging.h>
#include <saber/util/timeops.h>
#include <voyager/core/bg_eventloop.h>

saber::CountDownLatch* g_latch = nullptr;

namespace saber {

class Client {
 public:
  Client(voyager::EventLoop* loop, const ClientOptions& options, int times)
      : times_(times), finish_(0), loop_(loop), client_(loop, options) {}

  void Start() {
    client_.Connect();
    Create();
  }

  void Create() {
    CreateRequest request;
    request.set_path("/ls");
    request.set_data("lock service");
    client_.Create(request, nullptr,
                   std::bind(&Client::OnCreate, this, std::placeholders::_1,
                             std::placeholders::_2, std::placeholders::_3));
  }

  void OnCreate(const std::string& path, void* context,
                const CreateResponse& response) {
    if (response.code() == RC_OK || response.code() == RC_NODE_EXISTS) {
      printf("create successful, begin to test!\n");
      loop_->RunInLoop(std::bind(&Client::SetData, this));
    } else {
      printf("create failed!\n");
      exit(0);
    }
  }

  void GetData() {
    start_ = NowMicros();
    GetDataRequest request;
    request.set_path("/ls");
    request.set_watch(false);
    for (int i = 0; i < times_; ++i) {
      client_.GetData(request, nullptr, nullptr,
                      std::bind(&Client::OnGetData, this, std::placeholders::_1,
                                std::placeholders::_2, std::placeholders::_3));
    }
  }

  void OnGetData(const std::string& path, void* context,
                 const GetDataResponse& response) {
    printf("Read finish:%d\n", ++finish_);
    if (finish_ == times_) {
      end_ = NowMicros();
      double time = (double)(end_ - start_) / 1000000;
      printf("Read Time: %f, TPS:%f\n", time, times_ / time);
      g_latch->CountDown();
      finish_ = 0;
    }
  }

  void SetData() {
    start_ = NowMicros();
    SetDataRequest request;
    request.set_path("/ls");
    request.set_data("lock service");
    request.set_version(-1);
    for (int i = 0; i < times_; ++i) {
      client_.SetData(request, nullptr,
                      std::bind(&Client::OnSetData, this, std::placeholders::_1,
                                std::placeholders::_2, std::placeholders::_3));
    }
  }

  void OnSetData(const std::string& path, void* context,
                 const SetDataResponse& response) {
    printf("Write finish:%d\n", ++finish_);
    if (finish_ == times_) {
      end_ = NowMicros();
      double time = (double)(end_ - start_) / 1000000;
      printf("Write Time: %f, TPS:%f\n", time, times_ / time);
      g_latch->CountDown();
      finish_ = 0;
    }
  }

 private:
  int times_;
  int finish_;
  uint64_t start_;
  uint64_t end_;
  voyager::EventLoop* loop_;
  Saber client_;

  Client(const Client&);
  void operator=(const Client&);
};

}  // namespace saber

int main(int argc, char** argv) {
  if (argc != 4) {
    printf("Usage: <%s> <server_ip:server_port,...> <concurrent> <times>\n",
           argv[0]);
    return -1;
  }

  saber::SetLogLevel(saber::LOGLEVEL_DEBUG);
  // saber::SetLogHandler(nullptr);
  saber::ClientOptions options;
  options.root = "/ls";
  options.servers = argv[1];

  int c = atoi(argv[2]);
  int times = atoi(argv[3]);

  g_latch = new saber::CountDownLatch(c);

  std::vector<voyager::BGEventLoop> threads(c);
  std::vector<std::unique_ptr<saber::Client>> clients;
  for (int i = 0; i < c; ++i) {
    clients.push_back(std::unique_ptr<saber::Client>(
        new saber::Client(threads[i].Loop(), options, times)));
    clients[i]->Start();
  }

  g_latch->Wait();
  delete g_latch;

  g_latch = new saber::CountDownLatch(c);

  for (int i = 0; i < c; ++i) {
    clients[i]->GetData();
  }

  g_latch->Wait();
  delete g_latch;

  return 0;
}
