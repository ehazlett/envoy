#include "server/listener_manager_impl.h"

#include "envoy/registry/registry.h"

#include "common/common/assert.h"
#include "common/json/config_schemas.h"
#include "common/network/listen_socket_impl.h"
#include "common/network/utility.h"
#include "common/ssl/context_config_impl.h"

#include "server/configuration_impl.h" // TODO(mattklein123): Remove post 1.4.0

namespace Envoy {
namespace Server {

std::vector<Configuration::NetworkFilterFactoryCb>
ProdListenerComponentFactory::createFilterFactoryList_(
    const std::vector<Json::ObjectSharedPtr>& filters, Server::Instance& server,
    Configuration::FactoryContext& context) {
  std::vector<Configuration::NetworkFilterFactoryCb> ret;
  for (size_t i = 0; i < filters.size(); i++) {
    const std::string string_type = filters[i]->getString("type");
    const std::string string_name = filters[i]->getString("name");
    Json::ObjectSharedPtr config = filters[i]->getObject("config");
    ENVOY_LOG(info, "  filter #{}:", i);
    ENVOY_LOG(info, "    type: {}", string_type);
    ENVOY_LOG(info, "    name: {}", string_name);

    // Map filter type string to enum.
    Configuration::NetworkFilterType type;
    if (string_type == "read") {
      type = Configuration::NetworkFilterType::Read;
    } else if (string_type == "write") {
      type = Configuration::NetworkFilterType::Write;
    } else {
      ASSERT(string_type == "both");
      type = Configuration::NetworkFilterType::Both;
    }

    // Now see if there is a factory that will accept the config.
    Configuration::NamedNetworkFilterConfigFactory* factory =
        Registry::FactoryRegistry<Configuration::NamedNetworkFilterConfigFactory>::getFactory(
            string_name);
    if (factory != nullptr && factory->type() == type) {
      Configuration::NetworkFilterFactoryCb callback =
          factory->createFilterFactory(*config, context);
      ret.push_back(callback);
    } else {
      // DEPRECATED
      // This name wasn't found in the named map, so search in the deprecated list registry.
      bool found_filter = false;
      for (Configuration::NetworkFilterConfigFactory* config_factory :
           Configuration::MainImpl::filterConfigFactories()) {
        Configuration::NetworkFilterFactoryCb callback =
            config_factory->tryCreateFilterFactory(type, string_name, *config, server);
        if (callback) {
          ret.push_back(callback);
          found_filter = true;
          break;
        }
      }

      if (!found_filter) {
        throw EnvoyException(
            fmt::format("unable to create filter factory for '{}'/'{}'", string_name, string_type));
      }
    }
  }
  return ret;
}

Network::ListenSocketSharedPtr
ProdListenerComponentFactory::createListenSocket(Network::Address::InstanceConstSharedPtr address,
                                                 bool bind_to_port) {
  // For each listener config we share a single TcpListenSocket among all threaded listeners.
  // UdsListenerSockets are not managed and do not participate in hot restart as they are only
  // used for testing. First we try to get the socket from our parent if applicable.
  // TODO(mattklein123): UDS support.
  ASSERT(address->type() == Network::Address::Type::Ip);
  const std::string addr = fmt::format("tcp://{}", address->asString());
  const int fd = server_.hotRestart().duplicateParentListenSocket(addr);
  if (fd != -1) {
    ENVOY_LOG(info, "obtained socket for address {} from parent", addr);
    return std::make_shared<Network::TcpListenSocket>(fd, address);
  } else {
    return std::make_shared<Network::TcpListenSocket>(address, bind_to_port);
  }
}

ListenerImpl::ListenerImpl(const Json::Object& json, Instance& server,
                           ListenerComponentFactory& factory, const std::string& name,
                           bool workers_started, uint64_t hash)
    : Json::Validator(json, Json::Schema::LISTENER_SCHEMA), server_(server),
      address_(Network::Utility::resolveUrl(json.getString("address"))),
      global_scope_(server.stats().createScope("")),
      bind_to_port_(json.getBoolean("bind_to_port", true)),
      use_proxy_proto_(json.getBoolean("use_proxy_proto", false)),
      use_original_dst_(json.getBoolean("use_original_dst", false)),
      per_connection_buffer_limit_bytes_(
          json.getInteger("per_connection_buffer_limit_bytes", 1024 * 1024)),
      name_(name), workers_started_(workers_started), hash_(hash) {

  // ':' is a reserved char in statsd. Do the translation here to avoid costly inline translations
  // later.
  std::string final_stat_name = fmt::format("listener.{}.", address_->asString());
  std::replace(final_stat_name.begin(), final_stat_name.end(), ':', '_');
  listener_scope_ = server.stats().createScope(final_stat_name);

  if (json.hasObject("ssl_context")) {
    Ssl::ContextConfigImpl context_config(*json.getObject("ssl_context"));
    ssl_context_ =
        server.sslContextManager().createSslServerContext(*listener_scope_, context_config);
  }

  filter_factories_ = factory.createFilterFactoryList(json.getObjectArray("filters"), *this);
}

bool ListenerImpl::createFilterChain(Network::Connection& connection) {
  return Configuration::FilterChainUtility::buildFilterChain(connection, filter_factories_);
}

ListenerManagerImpl::ListenerManagerImpl(Instance& server,
                                         ListenerComponentFactory& listener_factory,
                                         WorkerFactory& worker_factory)
    : server_(server), factory_(listener_factory) {
  for (uint32_t i = 0; i < std::max(1U, server.options().concurrency()); i++) {
    workers_.emplace_back(worker_factory.createWorker());
  }
}

Init::Manager& ListenerImpl::initManager() {
  // fixfix comment
  if (workers_started_) {
    return dynamic_init_manager_;
  } else {
    return server_.initManager();
  }
}

void ListenerImpl::setSocket(const Network::ListenSocketSharedPtr& socket) {
  ASSERT(!socket_);
  socket_ = socket;
}

bool ListenerManagerImpl::addOrUpdateListener(const Json::Object& json) {
  const std::string name = json.getString("name", server_.random().uuid());
  uint64_t hash = json.hash();
  ENVOY_LOG(debug, "begin add/update listener: name={} hash={}", name, hash);

  // Optional<ListenerList::iterator> listener_to_replace;
  auto existing_active_listener = getListenerByName(active_listeners_, name);
  auto existing_warming_listener = getListenerByName(warming_listeners_, name);

  // fixfix check if address is the same.

  // Check to see if we need to update a warming listener first.
  if (existing_warming_listener != warming_listeners_.end()) {
    ASSERT(!workers_started_);
    ASSERT(false);
  } else if (existing_active_listener != active_listeners_.end()) {
    // Check to see if we need to update an active listener.
    if ((*existing_active_listener)->hash() == hash) {
      ENVOY_LOG(debug, "duplicate listener. no add/update");
      return false;
    }

    // listener_to_replace = existing_active_listener;
  }

  // fixfix logging.
  // fixfix stats.
  // fixfix unique address for not bound.
  std::unique_ptr<ListenerImpl> new_listener(
      new ListenerImpl(json, server_, factory_, name, workers_started_, hash));

  if (existing_warming_listener != warming_listeners_.end()) {
    // In this case we can just replace inline. fixfix init callbacks.
    new_listener->setSocket((*existing_warming_listener)->getSocket());
    *existing_warming_listener = std::move(new_listener);
  } else if (existing_active_listener != active_listeners_.end()) {
    // In this case we have no warming listener, so what we do depends on whether workers
    // have been started or not. Either way we get the socket from the existing listener.
    new_listener->setSocket((*existing_active_listener)->getSocket());
    if (workers_started_) {
      ENVOY_LOG(info, "add warming listener: name={}, hash={}, address={}", name, hash,
                new_listener->address()->asString());
      warming_listeners_.emplace_back(std::move(new_listener));
    } else {
      ENVOY_LOG(info, "update active listener: name={}, hash={}, address={}", name, hash,
                new_listener->address()->asString());
      *existing_active_listener = std::move(new_listener);
    }
  } else {
    new_listener->setSocket(
        factory_.createListenSocket(new_listener->address(), new_listener->bindToPort()));
    ENVOY_LOG(info, "add active listener: name={}, hash={}, address={}", name, hash,
              new_listener->address()->asString());
    active_listeners_.emplace_back(std::move(new_listener));
  }

  return true;
}

ListenerManagerImpl::ListenerList::iterator
ListenerManagerImpl::getListenerByName(ListenerList& listeners, const std::string& name) {
  for (auto it = listeners.begin(); it != listeners.end(); ++it) {
    if ((*it)->name() == name) {
      return it;
    }
  }
  return listeners.end();
}

std::vector<std::reference_wrapper<Listener>> ListenerManagerImpl::listeners() {
  std::vector<std::reference_wrapper<Listener>> ret;
  ret.reserve(active_listeners_.size());
  for (const auto& listener : active_listeners_) {
    ret.push_back(*listener);
  }
  return ret;
}

uint64_t ListenerManagerImpl::numConnections() {
  uint64_t num_connections = 0;
  for (const auto& worker : workers_) {
    num_connections += worker->numConnections();
  }

  return num_connections;
}

bool ListenerManagerImpl::removeListener(const std::string&) { return true; }

void ListenerManagerImpl::startWorkers(GuardDog& guard_dog) {
  ENVOY_LOG(warn, "all dependencies initialized. starting workers");
  ASSERT(!workers_started_);
  workers_started_ = true;
  for (const auto& worker : workers_) {
    ASSERT(warming_listeners_.empty());
    for (const auto& listener : active_listeners_) {
      worker->addListener(*listener);
    }
    worker->start(guard_dog);
  }
}

void ListenerManagerImpl::stopListeners() {
  for (const auto& worker : workers_) {
    worker->stopListeners();
  }
}

void ListenerManagerImpl::stopWorkers() {
  ASSERT(workers_started_);
  for (const auto& worker : workers_) {
    worker->stop();
  }
}

} // Server
} // Envoy
