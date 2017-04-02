// Copyright (c) 2017 Mirants Lu. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "saber/client/saber.h"
#include "saber/client/saber_client.h"
#include "saber/client/server_manager_impl.h"
#include "saber/util/murmurhash3.h"
#include "saber/util/logging.h"

namespace saber {

Saber::Saber(const Options& options)
    : options_(options),
      has_connected_(false),
      server_manager_(options.server_manager),
      send_loop_(nullptr),
      event_loop_(nullptr) {
}

Saber::~Saber() {
  delete server_manager_;
}

bool Saber::Start() {
  if (!server_manager_) {
    server_manager_ = new ServerManagerImpl();
    options_.server_manager = server_manager_;
  }
  server_manager_->UpdateServers(options_.servers);

  send_loop_ = send_thread_.Loop();
  event_loop_ = event_thread_.Loop();
  for (uint32_t i = 0; i < options_.group_size; ++i) {
    SaberClient* one = new SaberClient(options_, send_loop_, event_loop_);
    clients_.push_back(std::unique_ptr<SaberClient>(one));
  }
  return true;
}

void Saber::Connect() {
  bool expected = false;
  if (has_connected_.compare_exchange_strong(expected, true)) {
    for (auto& i : clients_) {
      i->Start();
    }
  } else {
    LOG_WARN("Saber has connected, don't call it again!\n");
  }
}

void Saber::Close() {
  bool expected = true;
  if (has_connected_.compare_exchange_strong(expected, false)) {
    for (auto & i : clients_) {
      i->Stop();
    }
  } else {
    LOG_WARN("Saber has closed, don't call it again!");
  }
}

void Saber::Create(const CreateRequest& request,
                   void* context, const CreateCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->Create(request, context, cb);
}

void Saber::Delete(const DeleteRequest& request,
                   void* context, const DeleteCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->Delete(request, context, cb);
}

void Saber::Exists(const ExistsRequest& request, Watcher* watcher,
                   void* context, const ExistsCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->Exists(request, watcher, context, cb);
}

void Saber::GetData(const GetDataRequest& request, Watcher* watcher,
                    void* context, const GetDataCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->GetData(request, watcher, context, cb);
}

void Saber::SetData(const SetDataRequest& request,
                    void* context, const SetDataCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->SetData(request, context, cb);
}

void Saber::GetACL(const GetACLRequest& request,
                   void* context, const GetACLCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->GetACL(request, context, cb);
}

void Saber::SetACL(const SetACLRequest& request,
                   void* context, const SetACLCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->SetACL(request, context, cb);
}

void Saber::GetChildren(const GetChildrenRequest& request, Watcher* watcher,
                        void* context, const ChildrenCallback& cb) {
  std::string root = request.path();
  clients_[Shard(root)]->GetChildren(request, watcher, context, cb);
}

uint32_t Saber::Shard(const std::string& s) {
  if (options_.group_size == 1) {
    return 0;
  } else {
    uint32_t h;
    MurmurHash3_x86_32(s.c_str(), static_cast<int>(s.size()), 0, &h);
    return (h % options_.group_size);
  }
}

}  // namespace saber