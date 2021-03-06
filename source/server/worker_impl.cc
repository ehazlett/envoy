#include "server/worker_impl.h"

#include <chrono>

#include "envoy/event/dispatcher.h"
#include "envoy/event/timer.h"
#include "envoy/server/configuration.h"
#include "envoy/thread_local/thread_local.h"

#include "common/api/api_impl.h"
#include "common/common/thread.h"

namespace Envoy {
namespace Server {

WorkerPtr ProdWorkerFactory::createWorker() {
  return WorkerPtr{new WorkerImpl(tls_, options_.fileFlushIntervalMsec())};
}

WorkerImpl::WorkerImpl(ThreadLocal::Instance& tls,
                       std::chrono::milliseconds file_flush_interval_msec)
    : tls_(tls), handler_(new ConnectionHandlerImpl(
                     log(), Api::ApiPtr{new Api::Impl(file_flush_interval_msec)})) {
  tls_.registerThread(handler_->dispatcher(), false);
}

void WorkerImpl::addListener(Listener& listener) {
  const Network::ListenerOptions listener_options = {.bind_to_port_ = listener.bindToPort(),
                                                     .use_proxy_proto_ = listener.useProxyProto(),
                                                     .use_original_dst_ = listener.useOriginalDst(),
                                                     .per_connection_buffer_limit_bytes_ =
                                                         listener.perConnectionBufferLimitBytes()};
  if (listener.sslContext()) {
    handler_->addSslListener(listener.filterChainFactory(), *listener.sslContext(),
                             listener.socket(), listener.listenerScope(), listener_options);
  } else {
    handler_->addListener(listener.filterChainFactory(), listener.socket(),
                          listener.listenerScope(), listener_options);
  }
}

uint64_t WorkerImpl::numConnections() {
  uint64_t ret = 0;
  if (handler_) {
    ret = handler_->numConnections();
  }
  return ret;
}

void WorkerImpl::start(GuardDog& guard_dog) {
  thread_.reset(new Thread::Thread([this, &guard_dog]() -> void { threadRoutine(guard_dog); }));
}

void WorkerImpl::stop() {
  // It's possible for the server to cleanly shut down while cluster initialization during startup
  // is happening, so we might not yet have a thread.
  if (thread_) {
    handler_->dispatcher().exit();
    thread_->join();
  }
}

void WorkerImpl::stopListeners() {
  handler_->dispatcher().post([this]() -> void { handler_->closeListeners(); });
}

void WorkerImpl::threadRoutine(GuardDog& guard_dog) {
  ENVOY_LOG(info, "worker entering dispatch loop");
  auto watchdog = guard_dog.createWatchDog(Thread::Thread::currentThreadId());
  watchdog->startWatchdog(handler_->dispatcher());
  handler_->dispatcher().run(Event::Dispatcher::RunType::Block);
  ENVOY_LOG(info, "worker exited dispatch loop");
  guard_dog.stopWatching(watchdog);

  // We must close all active connections before we actually exit the thread. This prevents any
  // destructors from running on the main thread which might reference thread locals. Destroying
  // the handler does this as well as destroying the dispatcher which purges the delayed deletion
  // list.
  handler_->closeConnections();
  tls_.shutdownThread();
  watchdog.reset();
  handler_.reset();
}

} // Server
} // Envoy
